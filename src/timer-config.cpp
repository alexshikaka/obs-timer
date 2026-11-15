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

#include "timer-config.hpp"

#include <obs-module.h>

#include <util/bmem.h>

#include <QDir>
#include <QStandardPaths>

#include <cstring>

namespace obstimer {

namespace {

struct IntervalInfo {
	const char *key;
	const char *label;
	int milliseconds;
};

const IntervalInfo kIntervals[] = {
	{"100ms", "Timer.Interval.100ms", 100},
	{"250ms", "Timer.Interval.250ms", 250},
	{"500ms", "Timer.Interval.500ms", 500},
	{"1s", "Timer.Interval.1s", 1000},
	{"5s", "Timer.Interval.5s", 5000},
	{"10s", "Timer.Interval.10s", 10000},
	{"on_stop", "Timer.Interval.OnStop", 0},
};

const size_t kIntervalCount = sizeof(kIntervals) / sizeof(kIntervals[0]);

QString configFilePath()
{
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return QString();

	QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

SaveInterval intervalFromKey(const char *key)
{
	if (!key)
		return SaveInterval::Sec1;

	for (size_t i = 0; i < kIntervalCount; i++) {
		if (strcmp(kIntervals[i].key, key) == 0)
			return static_cast<SaveInterval>(i);
	}

	return SaveInterval::Sec1;
}

const char *keyFromInterval(SaveInterval interval)
{
	const int index = static_cast<int>(interval);
	if (index < 0 || static_cast<size_t>(index) >= kIntervalCount)
		return kIntervals[3].key;

	return kIntervals[index].key;
}

} // namespace

QString defaultFilePath()
{
	QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	if (folder.isEmpty())
		folder = QDir::homePath();

	return QDir(folder).filePath(QStringLiteral("obs-timer.txt"));
}

QString intervalLabel(SaveInterval interval)
{
	const int index = static_cast<int>(interval);
	if (index < 0 || static_cast<size_t>(index) >= kIntervalCount)
		return QString::fromUtf8(obs_module_text(kIntervals[3].label));

	return QString::fromUtf8(obs_module_text(kIntervals[index].label));
}

int intervalMilliseconds(SaveInterval interval)
{
	const int index = static_cast<int>(interval);
	if (index < 0 || static_cast<size_t>(index) >= kIntervalCount)
		return kIntervals[3].milliseconds;

	return kIntervals[index].milliseconds;
}

Config loadConfig()
{
	Config config;
	config.filePath = defaultFilePath();

	const QString path = configFilePath();
	if (path.isEmpty())
		return config;

	const QByteArray file = path.toUtf8();
	obs_data_t *data = obs_data_create_from_json_file_safe(file.constData(), "bak");
	if (!data)
		return config;

	config.filePath = QString::fromUtf8(obs_data_get_string(data, "file_path"));
	if (config.filePath.isEmpty())
		config.filePath = defaultFilePath();

	config.interval = intervalFromKey(obs_data_get_string(data, "interval"));
	config.overwrite = obs_data_get_bool(data, "overwrite");
	config.showMilliseconds = obs_data_get_bool(data, "show_milliseconds");
	config.autoStartWithOutput = obs_data_get_bool(data, "auto_start");
	config.autoStopWithOutput = obs_data_get_bool(data, "auto_stop");
	config.resetOnStart = obs_data_get_bool(data, "reset_on_start");

	obs_data_release(data);
	return config;
}

void saveConfig(const Config &config)
{
	const QString path = configFilePath();
	if (path.isEmpty())
		return;

	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "file_path", config.filePath.toUtf8().constData());
	obs_data_set_string(data, "interval", keyFromInterval(config.interval));
	obs_data_set_bool(data, "overwrite", config.overwrite);
	obs_data_set_bool(data, "show_milliseconds", config.showMilliseconds);
	obs_data_set_bool(data, "auto_start", config.autoStartWithOutput);
	obs_data_set_bool(data, "auto_stop", config.autoStopWithOutput);
	obs_data_set_bool(data, "reset_on_start", config.resetOnStart);

	const QByteArray file = path.toUtf8();
	obs_data_save_json_safe(data, file.constData(), "tmp", "bak");

	obs_data_release(data);
}

} // namespace obstimer
