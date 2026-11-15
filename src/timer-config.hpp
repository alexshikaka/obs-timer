/*
obs-timer - a simple dockable timer for OBS Studio
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

enum class SaveInterval : int {
	Ms100 = 0,
	Ms250,
	Ms500,
	Sec1,
	Sec5,
	Sec10,
	OnStop,
};

struct Config {
	QString filePath;
	SaveInterval interval = SaveInterval::Sec1;
	bool overwrite = true;
	bool showMilliseconds = false;
	bool autoStartWithOutput = true;
	bool autoStopWithOutput = true;
	bool resetOnStart = true;
};

/* Suggested default location of the output file (Documents/obs-timer.txt). */
QString defaultFilePath();

/* Localised label for the interval, e.g. "every second". */
QString intervalLabel(SaveInterval interval);

/* Milliseconds between writes, 0 means "only when the timer is stopped". */
int intervalMilliseconds(SaveInterval interval);

Config loadConfig();
void saveConfig(const Config &config);

} // namespace obstimer
