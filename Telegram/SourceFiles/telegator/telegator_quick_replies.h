/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Data {
struct Shortcut;
} // namespace Data

namespace Ui::Text {
class String;
} // namespace Ui::Text

namespace Telegator {

// Quick replies in the order set on the phone, the way Telegram shows them
// on iPhone and Android. Desktop Telegram sorts them by id instead.
[[nodiscard]] std::vector<Data::Shortcut> OrderedShortcuts(
	not_null<Main::Session*> session);

// Text of the first message in one line, empty until the messages load.
[[nodiscard]] QString ShortcutPreview(
	not_null<Main::Session*> session,
	BusinessShortcutId id);

// The same preview with premium emoji, for the list after "/".
// Empty until the messages load.
[[nodiscard]] Ui::Text::String ShortcutPreviewText(
	not_null<Main::Session*> session,
	BusinessShortcutId id,
	Fn<void()> repaint);

// Calls callback whenever quick replies or their messages change, so the
// list after "/" gets the texts once they load.
void WatchQuickReplies(
	not_null<Main::Session*> session,
	Fn<void()> callback,
	rpl::lifetime &lifetime);

class QuickReplies final {
public:
	explicit QuickReplies(not_null<Main::Session*> session);

	[[nodiscard]] rpl::producer<> changes() const;

private:
	void loadMessages();

	const not_null<Main::Session*> _session;
	base::flat_map<BusinessShortcutId, rpl::lifetime> _loading;
	rpl::event_stream<> _changes;
	rpl::lifetime _lifetime;

};

} // namespace Telegator
