/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_journal_store.h"

#include "base/timer.h"
#include "core/version.h"
#include "settings.h"
#include "telegator/telegator_config.h"

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSysInfo>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Telegator {
namespace {

constexpr auto kFileLimit = qint64(4 * 1024 * 1024);
constexpr auto kKeepFiles = 20;
constexpr auto kBatchRecords = 200;
constexpr auto kUploadDelay = crl::time(3000);
constexpr auto kRetryDelay = crl::time(60000);
constexpr auto kCurrentName = "current.jsonl";

[[nodiscard]] QString Folder() {
	return cWorkingDir() + u"telegator_journal/"_q;
}

struct Device {
	QString id;
	QString name;
	QString os;
};

// Random id kept next to telegator.json: copied data keeps it, the name tells.
[[nodiscard]] Device ReadDevice() {
	auto result = Device{
		.name = QSysInfo::machineHostName(),
		.os = QSysInfo::prettyProductName(),
	};
	auto file = QFile(cWorkingDir() + u"telegator_device.json"_q);
	if (file.open(QIODevice::ReadOnly)) {
		result.id = QJsonDocument::fromJson(file.readAll()).object().value(
			u"id"_q).toString();
		file.close();
	}
	if (result.id.isEmpty()) {
		result.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		if (file.open(QIODevice::WriteOnly)) {
			auto object = QJsonObject();
			object.insert(u"id"_q, result.id);
			file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
		}
	}
	return result;
}

[[nodiscard]] const Device &CurrentDevice() {
	static const auto result = ReadDevice();
	return result;
}

class Store final {
public:
	Store();

	void write(QJsonObject record);

private:
	struct Cursor {
		QString file;
		qint64 offset = 0;
	};

	void rotate();
	void dropOld();
	[[nodiscard]] QStringList files() const;
	[[nodiscard]] Cursor readCursor() const;
	void saveCursor(const Cursor &cursor);
	void upload();
	void uploaded(Cursor next);

	base::Timer _timer;
	std::unique_ptr<QNetworkAccessManager> _network;
	QString _rotatedWhileSending;
	bool _sending = false;

};

Store::Store() : _timer([=] { upload(); }) {
	QDir().mkpath(Folder());
	_timer.callOnce(kUploadDelay);
}

void Store::write(QJsonObject record) {
	const auto &device = CurrentDevice();
	record.insert(u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces));
	record.insert(
		u"at"_q,
		QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
	record.insert(u"device_id"_q, device.id);
	record.insert(u"device_name"_q, device.name);
	record.insert(u"os"_q, device.os);
	record.insert(u"app"_q, QString::fromLatin1(AppVersionStr));

	auto file = QFile(Folder() + kCurrentName);
	if (!file.open(QIODevice::Append)) {
		LOG(("Telegator: journal not writable: %1").arg(file.errorString()));
		return;
	}
	file.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n');
	const auto size = file.size();
	file.close();
	if (size >= kFileLimit) {
		rotate();
	}
	if (!_sending && !_timer.isActive()) {
		_timer.callOnce(kUploadDelay);
	}
}

// Closed files are named by time, so the name order is the write order.
void Store::rotate() {
	const auto name = u"%1.jsonl"_q.arg(
		QDateTime::currentDateTimeUtc().toString(u"yyyyMMdd-HHmmss-zzz"_q));
	if (!QFile::rename(Folder() + kCurrentName, Folder() + name)) {
		return;
	}
	auto cursor = readCursor();
	if (cursor.file == kCurrentName) {
		cursor.file = name;
		saveCursor(cursor);
	}
	if (_sending && _rotatedWhileSending.isEmpty()) {
		_rotatedWhileSending = name;
	}
	dropOld();
}

void Store::dropOld() {
	auto closed = files();
	closed.removeAll(kCurrentName);
	const auto cursor = readCursor();
	while (closed.size() > kKeepFiles) {
		const auto name = closed.takeFirst();
		if (!Journal().url.isEmpty()
			&& (cursor.file.isEmpty() || name >= cursor.file)) {
			LOG(("Telegator: journal file %1 dropped before upload").arg(name));
		}
		QFile::remove(Folder() + name);
	}
}

QStringList Store::files() const {
	auto result = QDir(Folder()).entryList(
		{ u"*.jsonl"_q },
		QDir::Files,
		QDir::Name);
	if (result.removeAll(kCurrentName)) {
		result.push_back(kCurrentName);
	}
	return result;
}

Store::Cursor Store::readCursor() const {
	auto file = QFile(Folder() + u"cursor.json"_q);
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const auto object = QJsonDocument::fromJson(file.readAll()).object();
	return {
		.file = object.value(u"file"_q).toString(),
		.offset = qint64(object.value(u"offset"_q).toDouble()),
	};
}

void Store::saveCursor(const Cursor &cursor) {
	auto object = QJsonObject();
	object.insert(u"file"_q, cursor.file);
	object.insert(u"offset"_q, double(cursor.offset));
	auto file = QFile(Folder() + u"cursor.json"_q);
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
	}
}

void Store::upload() {
	const auto &config = Journal();
	if (config.url.isEmpty() || _sending) {
		return;
	}
	auto cursor = readCursor();
	const auto all = files();
	auto index = int(all.indexOf(cursor.file));
	if (index < 0) {
		// WHY: the cursor file was dropped: continue from the next kept one.
		index = 0;
		while (index < all.size()
			&& all[index] != kCurrentName
			&& all[index] < cursor.file) {
			++index;
		}
		cursor = { .file = all.value(index), .offset = 0 };
	}
	auto records = QJsonArray();
	auto next = Cursor();
	for (; index >= 0 && index < all.size(); ++index) {
		const auto name = all[index];
		auto file = QFile(Folder() + name);
		if (!file.open(QIODevice::ReadOnly)) {
			break;
		}
		auto offset = (name == cursor.file) ? cursor.offset : qint64(0);
		file.seek(offset);
		while (records.size() < kBatchRecords) {
			const auto line = file.readLine();
			if (line.isEmpty() || !line.endsWith('\n')) {
				break;
			}
			offset += line.size();
			const auto record = QJsonDocument::fromJson(line);
			if (record.isObject()) {
				records.push_back(record.object());
			}
		}
		next = { .file = name, .offset = offset };
		if (records.size() >= kBatchRecords || name == kCurrentName) {
			break;
		}
	}
	if (records.isEmpty()) {
		if (!next.file.isEmpty()
			&& (next.file != cursor.file || next.offset != cursor.offset)) {
			saveCursor(next);
		}
		return;
	}
	if (!_network) {
		_network = std::make_unique<QNetworkAccessManager>();
	}
	auto request = QNetworkRequest(QUrl(config.url));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		u"application/json"_q);
	request.setRawHeader("Authorization", "Bearer " + config.key);
	auto body = QJsonObject();
	body.insert(u"records"_q, records);
	_sending = true;
	const auto reply = _network->post(
		request,
		QJsonDocument(body).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		_sending = false;
		const auto guard = gsl::finally([&] {
			_rotatedWhileSending = QString();
		});
		const auto status = reply->attribute(
			QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() == QNetworkReply::NoError
			&& status >= 200
			&& status < 300) {
			uploaded(next);
		} else if (status == 400 || status == 413) {
			// WHY: the server will refuse this batch every time, waiting
			// would stop the whole queue of this computer.
			LOG(("Telegator: journal batch refused, status %1, skipped"
				).arg(status));
			uploaded(next);
		} else {
			LOG(("Telegator: journal upload failed, status %1, %2"
				).arg(status
				).arg(reply->errorString()));
			_timer.callOnce(kRetryDelay);
		}
	});
}

void Store::uploaded(Cursor next) {
	if (next.file == kCurrentName && !_rotatedWhileSending.isEmpty()) {
		next.file = _rotatedWhileSending;
	}
	saveCursor(next);
	_timer.callOnce(0);
}

[[nodiscard]] Store &Log() {
	static auto result = Store();
	return result;
}

} // namespace

void JournalStoreStart() {
	(void)Log();
}

void JournalStoreWrite(QJsonObject record) {
	Log().write(std::move(record));
}

} // namespace Telegator
