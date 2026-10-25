/*
This file is part of Telegator, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QJsonObject>

namespace Telegator {

// Records of the journal: a file queue next to the data, uploaded to the
// server of the owner in batches. The record gets its id, time and device.

// WHY: the first call starts the queue, it uploads what the previous run
// left in the files.
void JournalStoreStart();

void JournalStoreWrite(QJsonObject record);

} // namespace Telegator
