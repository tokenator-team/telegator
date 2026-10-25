/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>

class HistoryItem;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Telegator {

// What the panel page sees: the script that makes window.Telegator, and the
// objects the client hands to it.

[[nodiscard]] QByteArray BridgeScript();
[[nodiscard]] QJsonObject BridgeTheme();
[[nodiscard]] QJsonObject BridgeChat(
	not_null<Main::Session*> session,
	PeerData *peer);
[[nodiscard]] QJsonObject BridgeMessage(not_null<HistoryItem*> item);

} // namespace Telegator
