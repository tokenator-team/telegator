/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_types.h"

#include <QtCore/QJsonObject>

class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Telegator {

// Journal of people's actions: local JSONL, queued upload to the server.

// WHY: deleted messages leave memory before the request goes, so their chat
// and text are taken here, from the one place of deleting chosen messages.
void JournalDelete(
	not_null<Main::Session*> session,
	const MessageIdsList &ids,
	bool revoke);

// One record: who (the account and the operator of its panel), what, where.
// The text is cut to a limit. Actions inside Telegator itself (the panel
// page, its menu items) and requests to Telegram are written the same way.
void JournalWrite(
	not_null<Main::Session*> session,
	const QString &action,
	PeerData *peer,
	MsgId msgId,
	const QString &text,
	QJsonObject details = {});

[[nodiscard]] QString JournalShort(QString text);

// The chat of a record: id, type, name, username, bot or not.
[[nodiscard]] QJsonObject JournalChat(PeerData *peer);

// Who is signed in the panel page of this account, empty when nobody.
void JournalSetOperator(
	not_null<Main::Session*> session,
	const QString &name);

} // namespace Telegator
