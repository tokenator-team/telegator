/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_panel_state.h"

#include "main/main_session.h"
#include "settings.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace Telegator {
namespace {

constexpr auto kMenuLimit = 10;
constexpr auto kMenuTextLimit = 64;

[[nodiscard]] QString StatePath() {
	return cWorkingDir() + u"tdata/telegator_state.json"_q;
}

[[nodiscard]] QJsonObject &State() {
	static auto result = [] {
		auto file = QFile(StatePath());
		return file.open(QIODevice::ReadOnly)
			? QJsonDocument::fromJson(file.readAll()).object()
			: QJsonObject();
	}();
	return result;
}

void SaveState(const QString &key, const QJsonValue &value) {
	auto &state = State();
	if (state.value(key) == value) {
		return;
	}
	state.insert(key, value);
	auto file = QFile(StatePath());
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
	}
}

} // namespace

int SavedPanelWidth() {
	return State().value(u"panel_width"_q).toInt();
}

void SavePanelWidth(int width) {
	SaveState(u"panel_width"_q, width);
}

bool SavedPanelShown() {
	return State().value(u"panel_shown"_q).toBool();
}

void SavePanelShown(bool shown) {
	SaveState(u"panel_shown"_q, shown);
}

QString AccountKey(not_null<Main::Session*> session) {
	return QString::number(session->userId().bare);
}

std::vector<MenuItem> ParseMenu(const QJsonArray &items) {
	auto result = std::vector<MenuItem>();
	for (const auto &value : items) {
		const auto object = value.toObject();
		const auto id = object.value(u"id"_q).toString().trimmed();
		const auto label = object.value(u"label"_q).toString().trimmed();
		if (id.isEmpty()
			|| label.isEmpty()
			|| id.size() > kMenuTextLimit
			|| label.size() > kMenuTextLimit) {
			continue;
		}
		result.push_back({ .id = id, .label = label });
		if (result.size() == kMenuLimit) {
			break;
		}
	}
	return result;
}

std::vector<MenuItem> SavedMenu(
		not_null<Main::Session*> session) {
	const auto menu = State().value(u"menu"_q).toObject();
	return ParseMenu(menu.value(AccountKey(session)).toArray());
}

void SaveMenu(
		not_null<Main::Session*> session,
		const std::vector<MenuItem> &items) {
	auto list = QJsonArray();
	for (const auto &item : items) {
		auto object = QJsonObject();
		object.insert(u"id"_q, item.id);
		object.insert(u"label"_q, item.label);
		list.push_back(object);
	}
	auto menu = State().value(u"menu"_q).toObject();
	menu.insert(AccountKey(session), list);
	SaveState(u"menu"_q, menu);
}

} // namespace Telegator
