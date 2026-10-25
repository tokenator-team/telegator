/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QString>

#include <vector>

namespace Main {
class Session;
} // namespace Main

namespace Telegator {

// What the panel keeps between launches: tdata/telegator_state.json.

struct MenuItem {
	QString id;
	QString label;
};

[[nodiscard]] QString AccountKey(not_null<Main::Session*> session);

// Panel width set by dragging its left edge, 0 if never set.
[[nodiscard]] int SavedPanelWidth();
void SavePanelWidth(int width);

// Shown as it was left.
[[nodiscard]] bool SavedPanelShown();
void SavePanelShown(bool shown);

// Items of the message menu that the page set, per account.
[[nodiscard]] std::vector<MenuItem> ParseMenu(const QJsonArray &items);
[[nodiscard]] std::vector<MenuItem> SavedMenu(
	not_null<Main::Session*> session);
void SaveMenu(
	not_null<Main::Session*> session,
	const std::vector<MenuItem> &items);

} // namespace Telegator
