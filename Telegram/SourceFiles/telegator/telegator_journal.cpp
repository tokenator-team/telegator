/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_journal.h"

#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "telegator/telegator_journal_store.h"

#include <QtCore/QJsonArray>

namespace Telegator {
namespace {

constexpr auto kTextLimit = 500;

// Who is signed in the panel page of each account.
[[nodiscard]] base::flat_map<uint64, QString> &Operators() {
	static auto result = base::flat_map<uint64, QString>();
	return result;
}

[[nodiscard]] QString OperatorOf(uint64 accountId) {
	const auto &operators = Operators();
	const auto i = operators.find(accountId);
	return (i != end(operators)) ? i->second : QString();
}

[[nodiscard]] QString ChatType(not_null<PeerData*> peer) {
	return peer->isUser()
		? u"user"_q
		: peer->isChat()
		? u"chat"_q
		: u"channel"_q;
}

} // namespace

QString JournalShort(QString text) {
	return (text.size() > kTextLimit)
		? (text.left(kTextLimit) + QChar(0x2026))
		: text;
}

QJsonObject JournalChat(PeerData *peer) {
	auto result = QJsonObject();
	if (!peer) {
		return result;
	}
	const auto bare = peer->isUser()
		? peerToUser(peer->id).bare
		: peer->isChat()
		? peerToChat(peer->id).bare
		: peerToChannel(peer->id).bare;
	result.insert(u"id"_q, QString::number(bare));
	result.insert(u"type"_q, ChatType(peer));
	result.insert(u"name"_q, peer->name());
	result.insert(u"username"_q, peer->username());
	if (const auto user = peer->asUser()) {
		result.insert(u"bot"_q, user->isBot());
	}
	return result;
}

void JournalWrite(
		not_null<Main::Session*> session,
		const QString &action,
		PeerData *peer,
		MsgId msgId,
		const QString &text,
		QJsonObject details) {
	const auto user = session->user();
	auto record = QJsonObject();
	record.insert(u"account_id"_q, QString::number(user->id.value));
	record.insert(u"account_name"_q, user->name());
	record.insert(u"account_username"_q, user->username());
	record.insert(u"operator"_q, OperatorOf(user->id.value));
	record.insert(u"action"_q, action);
	record.insert(u"chat"_q, JournalChat(peer));
	if (msgId) {
		record.insert(u"message_id"_q, double(msgId.bare));
	}
	if (!text.isEmpty()) {
		record.insert(u"text"_q, JournalShort(text));
	}
	if (!details.isEmpty()) {
		record.insert(u"details"_q, details);
	}
	JournalStoreWrite(std::move(record));
}

void JournalDelete(
		not_null<Main::Session*> session,
		const MessageIdsList &ids,
		bool revoke) {
	auto byChat = base::flat_map<not_null<PeerData*>, QJsonArray>();
	auto texts = base::flat_map<not_null<PeerData*>, QString>();
	auto first = base::flat_map<not_null<PeerData*>, MsgId>();
	for (const auto &id : ids) {
		const auto item = session->data().message(id);
		if (!item) {
			continue;
		}
		const auto peer = item->history()->peer;
		byChat[peer].push_back(double(item->id.bare));
		if (!first.contains(peer)) {
			first.emplace(peer, item->id);
			texts.emplace(peer, item->originalText().text);
		}
	}
	for (const auto &[peer, list] : byChat) {
		auto details = QJsonObject();
		details.insert(u"ids"_q, list);
		details.insert(u"revoke"_q, revoke);
		JournalWrite(
			session,
			u"delete"_q,
			peer,
			first[peer],
			texts[peer],
			details);
	}
}

void JournalSetOperator(
		not_null<Main::Session*> session,
		const QString &name) {
	Operators()[session->user()->id.value] = name.trimmed();
}

} // namespace Telegator
