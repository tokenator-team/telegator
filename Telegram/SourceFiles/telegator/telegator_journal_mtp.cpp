/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_journal_mtp.h"

#include "core/application.h"
#include "data/data_message_reaction_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "mtproto/details/mtproto_serialized_request.h"
#include "telegator/telegator_journal.h"
#include "telegator/telegator_journal_store.h"

#include <QtCore/QJsonArray>
#include <QtCore/QThread>

namespace Telegator {
namespace {

using SerializedRequest = MTP::details::SerializedRequest;

[[nodiscard]] QJsonArray IdsArray(const QVector<MTPint> &ids) {
	auto result = QJsonArray();
	for (const auto &id : ids) {
		result.push_back(double(id.v));
	}
	return result;
}

[[nodiscard]] Main::Session *SessionOf(not_null<MTP::Instance*> instance) {
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (account->sessionExists() && (&account->mtp() == instance)) {
			return &account->session();
		}
	}
	return nullptr;
}

// Reads fields of a request in the order of the scheme.
class Reader final {
public:
	explicit Reader(const SerializedRequest &request)
	: _from(request->constData() + SerializedRequest::kMessageBodyPosition)
	, _end(request->constData() + request->size()) {
	}

	[[nodiscard]] bool ok() const {
		return _ok;
	}
	[[nodiscard]] mtpTypeId type() {
		return prime();
	}
	[[nodiscard]] uint32 flags() {
		return prime();
	}
	[[nodiscard]] int32 integer() {
		return int32(prime());
	}

	template <typename Type>
	[[nodiscard]] Type read() {
		auto result = Type();
		if (_ok && !result.read(_from, _end)) {
			_ok = false;
		}
		return result;
	}

private:
	[[nodiscard]] uint32 prime() {
		if (!_ok || _from >= _end) {
			_ok = false;
			return 0;
		}
		return uint32(*_from++);
	}

	const mtpPrime *_from = nullptr;
	const mtpPrime *_end = nullptr;
	bool _ok = true;

};

[[nodiscard]] PeerData *Peer(
		not_null<Main::Session*> session,
		const MTPInputPeer &input) {
	return Data::PeerFromInputMTP(&session->data(), input);
}

[[nodiscard]] QString BytesText(const QByteArray &data) {
	const auto text = QString::fromUtf8(data);
	return (text.toUtf8() == data)
		? text
		: (u"base64:"_q + QString::fromLatin1(data.toBase64()));
}

[[nodiscard]] QString ReactionsText(const QVector<MTPReaction> &list) {
	auto result = QStringList();
	for (const auto &reaction : list) {
		const auto id = Data::ReactionFromMTP(reaction);
		if (const auto emoji = std::get_if<QString>(&id.data)) {
			result.push_back(id.paid() ? u"paid"_q : *emoji);
		} else if (const auto custom = std::get_if<DocumentId>(&id.data)) {
			result.push_back(u"custom:%1"_q.arg(*custom));
		}
	}
	return result.join(' ');
}

void BotButton(
		not_null<Main::Session*> session,
		PeerData *peer,
		MsgId msgId,
		const std::optional<QByteArray> &data) {
	using Type = HistoryMessageMarkupButton::Type;
	auto details = QJsonObject();
	auto button = QString();
	const auto item = peer
		? session->data().message(peer->id, msgId)
		: nullptr;
	if (item) {
		if (const auto bot = item->getMessageBot()) {
			details.insert(u"bot_id"_q, QString::number(bot->id.value));
			details.insert(u"bot_username"_q, bot->username());
		}
		if (const auto markup = item->Get<HistoryMessageReplyMarkup>()) {
			for (const auto &row : markup->data.rows) {
				for (const auto &entry : row) {
					const auto match = data
						? ((entry.type == Type::Callback
							|| entry.type == Type::CallbackWithPassword)
							&& entry.data == *data)
						: (entry.type == Type::Game);
					if (match && button.isEmpty()) {
						button = entry.text;
					}
				}
			}
		}
		details.insert(
			u"message_text"_q,
			JournalShort(item->originalText().text));
	}
	if (data) {
		details.insert(u"data"_q, BytesText(*data));
	}
	JournalWrite(session, u"bot_button"_q, peer, msgId, button, details);
}

// Bot's own start and mini app openings: the bot and where it was opened.
void WebApp(
		not_null<Main::Session*> session,
		const QString &action,
		PeerData *peer,
		const MTPInputUser &input,
		const QString &param) {
	auto details = QJsonObject();
	if (const auto bot = Data::UserFromInputMTP(&session->data(), input)) {
		details.insert(u"bot_id"_q, QString::number(bot->id.value));
		details.insert(u"bot_username"_q, bot->username());
	}
	if (!param.isEmpty()) {
		details.insert(u"param"_q, JournalShort(param));
	}
	JournalWrite(session, action, peer, 0, {}, details);
}

void Parse(
		not_null<Main::Session*> session,
		const SerializedRequest &request) {
	auto reader = Reader(request);
	const auto type = reader.type();
	const auto replyTo = [&](uint32 flags, uint32 flag) {
		if (flags & flag) {
			(void)reader.read<MTPInputReplyTo>();
		}
	};
	switch (type) {
	case mtpc_messages_sendMessage: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		replyTo(flags, 1U << 0);
		const auto text = reader.read<MTPstring>();
		if (reader.ok()) {
			JournalWrite(
				session,
				u"send_message"_q,
				Peer(session, peer),
				0,
				qs(text),
				{});
		}
	} break;
	case mtpc_messages_sendMedia: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		replyTo(flags, 1U << 0);
		(void)reader.read<MTPInputMedia>();
		const auto text = reader.read<MTPstring>();
		if (reader.ok()) {
			JournalWrite(
				session,
				u"send_media"_q,
				Peer(session, peer),
				0,
				qs(text),
				{});
		}
	} break;
	case mtpc_messages_sendMultiMedia: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		replyTo(flags, 1U << 0);
		const auto list = reader.read<MTPVector<MTPInputSingleMedia>>();
		if (reader.ok()) {
			auto text = QString();
			for (const auto &media : list.v) {
				const auto caption = qs(media.data().vmessage());
				if (!caption.isEmpty()) {
					text = caption;
				}
			}
			auto details = QJsonObject();
			details.insert(u"count"_q, int(list.v.size()));
			JournalWrite(
				session,
				u"send_album"_q,
				Peer(session, peer),
				0,
				text,
				details);
		}
	} break;
	case mtpc_messages_sendInlineBotResult: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		if (reader.ok()) {
			(void)flags;
			JournalWrite(
				session,
				u"send_inline"_q,
				Peer(session, peer),
				0,
				{},
				{});
		}
	} break;
	case mtpc_messages_forwardMessages: {
		(void)reader.flags();
		const auto from = reader.read<MTPInputPeer>();
		const auto ids = reader.read<MTPVector<MTPint>>();
		(void)reader.read<MTPVector<MTPlong>>();
		const auto to = reader.read<MTPInputPeer>();
		if (reader.ok()) {
			const auto source = Peer(session, from);
			auto details = QJsonObject();
			details.insert(u"from"_q, JournalChat(source));
			details.insert(u"ids"_q, IdsArray(ids.v));
			const auto first = ids.v.isEmpty() ? MsgId() : MsgId(ids.v[0].v);
			const auto item = (source && first)
				? session->data().message(source->id, first)
				: nullptr;
			JournalWrite(
				session,
				u"forward"_q,
				Peer(session, to),
				0,
				item ? item->originalText().text : QString(),
				details);
		}
	} break;
	case mtpc_messages_editMessage: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto id = reader.integer();
		const auto text = (flags & (1U << 11))
			? qs(reader.read<MTPstring>())
			: QString();
		if (reader.ok()) {
			JournalWrite(session, u"edit"_q, Peer(session, peer), id, text, {});
		}
	} break;
	case mtpc_messages_deleteHistory: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		if (reader.ok()) {
			auto details = QJsonObject();
			details.insert(u"revoke"_q, bool(flags & (1U << 1)));
			details.insert(u"just_clear"_q, bool(flags & (1U << 0)));
			JournalWrite(
				session,
				u"delete_history"_q,
				Peer(session, peer),
				0,
				{},
				details);
		}
	} break;
	case mtpc_messages_sendReaction: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto id = reader.integer();
		const auto list = (flags & (1U << 0))
			? reader.read<MTPVector<MTPReaction>>()
			: MTPVector<MTPReaction>();
		if (reader.ok()) {
			const auto text = ReactionsText(list.v);
			JournalWrite(
				session,
				text.isEmpty() ? u"reaction_removed"_q : u"reaction"_q,
				Peer(session, peer),
				id,
				text,
				{});
		}
	} break;
	case mtpc_messages_updatePinnedMessage: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto id = reader.integer();
		if (reader.ok()) {
			JournalWrite(
				session,
				(flags & (1U << 1)) ? u"unpin"_q : u"pin"_q,
				Peer(session, peer),
				id,
				{},
				{});
		}
	} break;
	case mtpc_messages_unpinAllMessages: {
		(void)reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		if (reader.ok()) {
			JournalWrite(
				session,
				u"unpin_all"_q,
				Peer(session, peer),
				0,
				{},
				{});
		}
	} break;
	case mtpc_messages_startBot: {
		const auto bot = reader.read<MTPInputUser>();
		const auto peer = reader.read<MTPInputPeer>();
		(void)reader.read<MTPlong>();
		const auto param = reader.read<MTPstring>();
		if (reader.ok()) {
			WebApp(
				session,
				u"bot_start"_q,
				Peer(session, peer),
				bot,
				qs(param));
		}
	} break;
	case mtpc_messages_requestWebView: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto bot = reader.read<MTPInputUser>();
		const auto url = (flags & (1U << 1))
			? qs(reader.read<MTPstring>())
			: QString();
		if (reader.ok()) {
			WebApp(session, u"bot_webapp"_q, Peer(session, peer), bot, url);
		}
	} break;
	case mtpc_messages_requestSimpleWebView: {
		const auto flags = reader.flags();
		const auto bot = reader.read<MTPInputUser>();
		const auto url = (flags & (1U << 3))
			? qs(reader.read<MTPstring>())
			: QString();
		if (reader.ok()) {
			WebApp(session, u"bot_webapp"_q, nullptr, bot, url);
		}
	} break;
	case mtpc_messages_requestMainWebView: {
		(void)reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto bot = reader.read<MTPInputUser>();
		if (reader.ok()) {
			WebApp(session, u"bot_webapp"_q, Peer(session, peer), bot, {});
		}
	} break;
	case mtpc_messages_sendVote: {
		const auto peer = reader.read<MTPInputPeer>();
		const auto id = reader.integer();
		const auto options = reader.read<MTPVector<MTPbytes>>();
		if (reader.ok()) {
			auto list = QJsonArray();
			for (const auto &option : options.v) {
				list.push_back(BytesText(option.v));
			}
			auto details = QJsonObject();
			details.insert(u"options"_q, list);
			JournalWrite(
				session,
				u"vote"_q,
				Peer(session, peer),
				id,
				{},
				details);
		}
	} break;
	case mtpc_messages_getBotCallbackAnswer: {
		const auto flags = reader.flags();
		const auto peer = reader.read<MTPInputPeer>();
		const auto id = reader.integer();
		auto data = std::optional<QByteArray>();
		if (flags & (1U << 0)) {
			data = reader.read<MTPbytes>().v;
		}
		if (reader.ok()) {
			BotButton(session, Peer(session, peer), id, data);
		}
	} break;
	}
}

[[nodiscard]] bool Interesting(mtpTypeId type) {
	switch (type) {
	case mtpc_messages_sendMessage:
	case mtpc_messages_sendMedia:
	case mtpc_messages_sendMultiMedia:
	case mtpc_messages_sendInlineBotResult:
	case mtpc_messages_forwardMessages:
	case mtpc_messages_editMessage:
	case mtpc_messages_deleteHistory:
	case mtpc_messages_sendReaction:
	case mtpc_messages_updatePinnedMessage:
	case mtpc_messages_unpinAllMessages:
	case mtpc_messages_getBotCallbackAnswer:
	case mtpc_messages_startBot:
	case mtpc_messages_requestWebView:
	case mtpc_messages_requestSimpleWebView:
	case mtpc_messages_requestMainWebView:
	case mtpc_messages_sendVote:
		return true;
	}
	return false;
}

} // namespace

void JournalRequest(
		not_null<MTP::Instance*> instance,
		const SerializedRequest &request) {
	const auto main = (QThread::currentThread() == qApp->thread());
	if (main) {
		// WHY: the first request starts the store, it uploads what the
		// previous run left queued.
		JournalStoreStart();
	}
	const auto position = SerializedRequest::kMessageBodyPosition;
	if (!request || request->size() <= position) {
		return;
	} else if (!Interesting(mtpTypeId((*request)[position]))) {
		return;
	}
	const auto parse = [=, copy = request] {
		if (const auto session = SessionOf(instance)) {
			Parse(session, copy);
		}
	};
	if (main) {
		parse();
	} else {
		crl::on_main(parse);
	}
}

} // namespace Telegator
