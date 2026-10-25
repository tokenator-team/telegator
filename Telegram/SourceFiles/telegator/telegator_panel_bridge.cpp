/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_panel_bridge.h"

#include "core/version.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "telegator/telegator_panel_state.h"
#include "styles/style_window.h"

#include <QtCore/QJsonObject>

namespace Telegator {
namespace {

constexpr auto kBridgeVersion = 1;

[[nodiscard]] QString CssColor(const style::color &color) {
	const auto c = color->c;
	return u"rgba(%1,%2,%3,%4)"_q
		.arg(c.red())
		.arg(c.green())
		.arg(c.blue())
		.arg(c.alphaF());
}

} // namespace

// WHY: the page lives on the owner's server and changes without a client
// update, so the page API is a versioned contract kept in the ROADMAP.
QByteArray BridgeScript() {
	return QByteArray(R"JS(
window.Telegator = {
	version: { app: '%APP%', bridge: %BRIDGE% },
	chat: null,
	onchat: null,
	theme: null,
	ontheme: null,
	onmenu: null,
	send: function (message) {
		if (window.external && window.external.invoke) {
			window.external.invoke(JSON.stringify(message));
		}
	},
	insertText: function (text, chatKey) {
		if (!chatKey) {
			console.warn('Telegator.insertText needs chat.peer.key');
			return;
		}
		this.send({
			event: 'insert_text',
			text: String(text),
			chat: String(chatKey)
		});
	},
	setMenu: function (items) {
		if (!Array.isArray(items)) {
			console.warn('Telegator.setMenu needs [{ id, label }]');
			return;
		}
		this.send({
			event: 'set_menu',
			items: items.map(function (item) {
				return {
					id: (item && item.id != null) ? String(item.id) : '',
					label: (item && item.label != null) ? String(item.label) : ''
				};
			})
		});
	},
	open: function () {
		this.send({ event: 'open' });
	},
	setOperator: function (name) {
		this.send({
			event: 'operator',
			name: (name != null) ? String(name) : ''
		});
	},
	_set: function (field, callback, value) {
		this[field] = value;
		this._fire(callback, value);
	},
	_fire: function (callback, value) {
		if (typeof this[callback] === 'function') {
			this[callback](value);
		}
	}
};
document.addEventListener('DOMContentLoaded', function () {
	window.Telegator.send({ event: 'ready' });
});
)JS").replace("%APP%", AppVersionStr).replace(
		"%BRIDGE%",
		QByteArray::number(kBridgeVersion));
}

QJsonObject BridgeTheme() {
	auto result = QJsonObject();
	result.insert(u"bg"_q, CssColor(st::windowBg));
	result.insert(u"fg"_q, CssColor(st::windowFg));
	result.insert(u"sub"_q, CssColor(st::windowSubTextFg));
	result.insert(u"over"_q, CssColor(st::windowBgOver));
	result.insert(u"accent"_q, CssColor(st::windowBgActive));
	result.insert(u"accent-fg"_q, CssColor(st::windowFgActive));
	return result;
}

QJsonObject BridgeChat(
		not_null<Main::Session*> session,
		PeerData *peer) {
	auto result = QJsonObject();
	result.insert(u"account_id"_q, AccountKey(session));
	if (!peer) {
		return result;
	}
	const auto id = peer->id;
	const auto type = peer->isUser()
		? u"user"_q
		: peer->isChat()
		? u"chat"_q
		: u"channel"_q;
	const auto bare = peer->isUser()
		? peerToUser(id).bare
		: peer->isChat()
		? peerToChat(id).bare
		: peerToChannel(id).bare;
	auto chat = QJsonObject();
	chat.insert(u"key"_q, QString::number(id.value));
	chat.insert(u"type"_q, type);
	chat.insert(u"id"_q, QString::number(bare));
	chat.insert(u"name"_q, peer->name());
	chat.insert(u"username"_q, peer->username());
	result.insert(u"peer"_q, chat);
	return result;
}

QJsonObject BridgeMessage(not_null<HistoryItem*> item) {
	auto result = QJsonObject();
	result.insert(u"id"_q, QString::number(item->id.bare));
	result.insert(u"text"_q, item->originalText().text);
	result.insert(
		u"out"_q,
		item->out() || item->history()->peer->isSelf());
	result.insert(u"forwarded"_q, item->Has<HistoryMessageForwarded>());
	result.insert(u"date"_q, item->date());
	return result;
}

} // namespace Telegator
