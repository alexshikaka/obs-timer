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

#include "timer-config.hpp"

#include <obs-module.h>

#include <util/bmem.h>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace obstimer {

namespace {

const char *const kFileName = "obs-timer.txt";

QString configFilePath()
{
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return QString();

	QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

/* Directory the plugin DLL was loaded from: <obs>/plugins/obs-timer.
 * obs_find_module_file() hands out the module data directory,
 * <obs>/plugins/obs-timer/data, so the folder itself is its parent. */
QString pluginFolder()
{
	char *dataPath = obs_find_module_file(obs_current_module(), nullptr);
	if (!dataPath)
		return QString();

	QString path = QString::fromUtf8(dataPath);
	bfree(dataPath);

	while (path.endsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('\\')))
		path.chop(1);

	if (path.isEmpty())
		return QString();

	return QFileInfo(path).absolutePath();
}

int clampInt(int value, int minimum, int maximum)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

} // namespace

QString defaultFilePath()
{
	const QString folder = pluginFolder();
	if (!folder.isEmpty())
		return QDir(folder).filePath(QString::fromLatin1(kFileName));

	QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	if (documents.isEmpty())
		documents = QDir::homePath();

	return QDir(documents).filePath(QString::fromLatin1(kFileName));
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

	const QString storedPath = QString::fromUtf8(obs_data_get_string(data, "file_path"));
	if (!storedPath.isEmpty())
		config.filePath = storedPath;

	config.hours = clampInt((int)obs_data_get_int(data, "hours"), 0, 99);
	config.minutes = clampInt((int)obs_data_get_int(data, "minutes"), 0, 59);
	config.seconds = clampInt((int)obs_data_get_int(data, "seconds"), 0, 59);

	const int format = (int)obs_data_get_int(data, "format");
	if (format == (int)DisplayFormat::MinutesSeconds)
		config.format = DisplayFormat::MinutesSeconds;
	else
		config.format = DisplayFormat::HoursMinutesSeconds;

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
	obs_data_set_int(data, "hours", config.hours);
	obs_data_set_int(data, "minutes", config.minutes);
	obs_data_set_int(data, "seconds", config.seconds);
	obs_data_set_int(data, "format", (int)config.format);

	const QByteArray file = path.toUtf8();
	obs_data_save_json_safe(data, file.constData(), "tmp", "bak");

	obs_data_release(data);
}

} // namespace obstimer
