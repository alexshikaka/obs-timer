/*
obs-timer - countdown timer for OBS Studio
Copyright (C) 2026 alexshikaka

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <QString>

namespace obstimer {

/* Which units the timer shows (and writes to the file).
 * The separator is always a colon. */
enum class DisplayFormat : int {
	HoursMinutesSeconds = 0,
	MinutesSeconds,
	SecondsOnly,
};

struct Config {
	QString filePath;
	int hours = 0;
	int minutes = 5;
	int seconds = 0;
	DisplayFormat format = DisplayFormat::HoursMinutesSeconds;
};

/* Default target file: Documents/obs-timer.txt */
QString defaultFilePath();

Config loadConfig();
void saveConfig(const Config &config);

} // namespace obstimer
