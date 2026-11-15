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

#include "timer-config.hpp"

#include <QElapsedTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

namespace obstimer {

/* Dock content widget: a stopwatch that continuously writes its value
 * into a plain text file.  OBS wraps this widget into its own QDockWidget
 * via obs_frontend_add_dock_by_id(). */
class TimerDock : public QWidget {
	Q_OBJECT

public:
	explicit TimerDock(QWidget *parent = nullptr);
	~TimerDock() override;

	static TimerDock *instance() { return s_instance; }

	/* Persist the current settings into the plugin config directory. */
	void persistSettings();

public slots:
	void startOrPause();
	void resetTimer();
	void saveNow();
	void handleOutputStateChanged(bool active);
	void shutdown();

private slots:
	void onTick();
	void onConfigChanged();
	void onBrowseClicked();

private:
	void buildUi();
	void applyConfig();
	Config collectConfig() const;
	void updateDisplay();
	void updateStartPauseButton();
	void startTimer(bool reset);
	void pauseTimer();
	bool writeCurrentValue(QString *error);
	bool appendSessionLine(QString *error);
	void setStatus(const QString &text, bool warning);
	QString localised(const char *key) const;

	qint64 elapsedMilliseconds() const;
	static QString formatDuration(qint64 milliseconds, bool withMilliseconds);
	QString targetFilePath() const;

	static TimerDock *s_instance;

	bool m_running = false;
	bool m_outputActive = false;
	bool m_updatingUi = false;
	qint64 m_baseMs = 0;
	qint64 m_lastWriteMs = -1;
	QElapsedTimer m_clock;
	Config m_config;

	QTimer *m_tickTimer = nullptr;
	QTimer *m_configSaveTimer = nullptr;

	QLabel *m_timeLabel = nullptr;
	QPushButton *m_startPauseButton = nullptr;
	QPushButton *m_resetButton = nullptr;
	QPushButton *m_saveButton = nullptr;
	QLineEdit *m_pathEdit = nullptr;
	QPushButton *m_browseButton = nullptr;
	QComboBox *m_intervalCombo = nullptr;
	QCheckBox *m_overwriteBox = nullptr;
	QCheckBox *m_millisecondsBox = nullptr;
	QCheckBox *m_autoStartBox = nullptr;
	QCheckBox *m_autoStopBox = nullptr;
	QCheckBox *m_resetOnStartBox = nullptr;
	QLabel *m_statusLabel = nullptr;
};

} // namespace obstimer
