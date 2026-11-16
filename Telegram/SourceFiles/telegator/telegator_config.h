/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonObject>

namespace Main {
class Session;
} // namespace Main

namespace Telegator {

// Local settings of this installation, never stored in the repository.
// Read once from <working dir>/telegator.json; without "panel" there, the
// panel comes from telegator.json packaged with the program (the app
// bundle's Resources on macOS, the program folder elsewhere):
// {
//   "panel": {
//     "accounts": [ 123456789 ],        // Telegram user ids with the panel
//     "url": "https://example.com/panel" // empty or missing: no page
//   },
//   "journal": {
//     "url": "https://example.com/journal", // empty or missing: local only
//     "key": "..."                          // sent as a bearer token
//   }
// }
struct PanelConfig {
	std::vector<uint64> accounts;
	QString url;
};

struct JournalConfig {
	QString url;
	QByteArray key;
};

[[nodiscard]] const PanelConfig &Panel();
[[nodiscard]] const JournalConfig &Journal();

// Replaces the file only after the whole JSON is written.
void WriteJsonFile(const QString &path, const QJsonObject &object);
[[nodiscard]] bool PanelAllowed(not_null<Main::Session*> session);

} // namespace Telegator
