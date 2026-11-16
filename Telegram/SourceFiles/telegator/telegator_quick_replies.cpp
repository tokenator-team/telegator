/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telegator/telegator_quick_replies.h"

#include "core/ui_integration.h"
#include "data/business/data_shortcut_messages.h"
#include "data/data_messages.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "ui/text/text.h"
#include "ui/text/text_utilities.h"
#include "styles/style_basic.h"

namespace Telegator {
namespace {

constexpr auto kPreviewLength = 80;

[[nodiscard]] HistoryItem *FirstMessage(
		not_null<Main::Session*> session,
		BusinessShortcutId id) {
	const auto slice = session->data().shortcutMessages().list(id);
	return slice.ids.empty()
		? nullptr
		: session->data().message(slice.ids.front());
}

// One line of text with premium emoji only, other formatting dropped.
[[nodiscard]] TextWithEntities PreviewText(not_null<HistoryItem*> item) {
	auto result = item->originalText();
	if (result.text.isEmpty()) {
		return { u"(медиа)"_q };
	}
	result.entities.erase(
		ranges::remove_if(result.entities, [](const EntityInText &entity) {
			return (entity.type() != EntityType::CustomEmoji);
		}),
		result.entities.end());
	result.text.replace(QChar('\n'), QChar(' '));
	if (result.text.size() > kPreviewLength) {
		auto length = kPreviewLength - 1;
		if (result.text[length - 1].isHighSurrogate()) {
			--length;
		}
		result = Ui::Text::Mid(result, 0, length);
		result.text.append(QChar(0x2026));
	}
	return result;
}

} // namespace

std::vector<Data::Shortcut> OrderedShortcuts(
		not_null<Main::Session*> session) {
	auto result = session->data().shortcutMessages().shortcuts().list
		| ranges::views::values
		| ranges::to_vector;
	ranges::sort(result, [](const Data::Shortcut &a, const Data::Shortcut &b) {
		return (a.order != b.order) ? (a.order < b.order) : (a.id > b.id);
	});
	return result;
}

QString ShortcutPreview(
		not_null<Main::Session*> session,
		BusinessShortcutId id) {
	const auto item = FirstMessage(session, id);
	return item ? PreviewText(item).text : QString();
}

Ui::Text::String ShortcutPreviewText(
		not_null<Main::Session*> session,
		BusinessShortcutId id,
		Fn<void()> repaint) {
	auto result = Ui::Text::String();
	const auto item = FirstMessage(session, id);
	if (!item) {
		// Starts the load again if it failed or never started.
		(void)session->data().shortcutMessages().updates(id);
	} else {
		result.setMarkedText(
			st::defaultTextStyle,
			PreviewText(item),
			kMarkupTextOptions,
			Core::TextContext({
				.session = session,
				.repaint = std::move(repaint),
			}));
	}
	return result;
}

void WatchQuickReplies(
		not_null<Main::Session*> session,
		Fn<void()> callback,
		rpl::lifetime &lifetime) {
	const auto replies = lifetime.make_state<QuickReplies>(session);
	replies->changes() | rpl::on_next(std::move(callback), lifetime);
}

QuickReplies::QuickReplies(not_null<Main::Session*> session)
: _session(session) {
	const auto messages = &session->data().shortcutMessages();
	messages->shortcutsChanged() | rpl::on_next([=] {
		loadMessages();
		_changes.fire({});
	}, _lifetime);
	messages->preloadShortcuts();
	loadMessages();
}

rpl::producer<> QuickReplies::changes() const {
	return _changes.events();
}

void QuickReplies::loadMessages() {
	// Subscribing to updates loads the messages, their texts make previews.
	const auto messages = &_session->data().shortcutMessages();
	const auto &list = messages->shortcuts().list;
	for (auto i = begin(_loading); i != end(_loading);) {
		if (list.contains(i->first)) {
			++i;
		} else {
			i = _loading.erase(i);
		}
	}
	for (const auto &[id, shortcut] : list) {
		if (_loading.contains(id)) {
			continue;
		}
		messages->updates(id) | rpl::on_next([=] {
			_changes.fire({});
		}, _loading[id]);
	}
}

} // namespace Telegator
