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

#include "timer-config.hpp"

#include <QWidget>

#include <chrono>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace obstimer {

/* Dock content widget: a countdown timer that rewrites a plain text file
 * in lock-step with the countdown (once per whole second).  OBS wraps this
 * widget into its own QDockWidget via obs_frontend_add_dock_by_id(). */
class TimerDock : public QWidget {
	Q_OBJECT

public:
	explicit TimerDock(QWidget *parent = nullptr);
	~TimerDock() override;

	static TimerDock *instance() { return s_instance; }

public slots:
	/* Start button: starts the countdown, turns into Pause while running. */
	void startOrPause();
	/* Stop button: stops the countdown and restores the chosen duration. */
	void reset();

private slots:
	void onTick();
	void onDurationChanged();
	void onFormatChanged(int index);
	void onFilePathEdited();
	void onBrowseClicked();

private:
	void buildUi();
	void applyConfig();
	void persistSettings();
	void updateDisplay();
	void updateStartPauseButton();
	void writeFile();
	void finish();
	void scheduleNextTick();
	void setError(const QString &text);

	qint64 configuredMs() const;
	qint64 elapsedMs() const;
	qint64 shownMs() const;
	QString formatDuration(qint64 milliseconds) const;

	static TimerDock *s_instance;

	bool m_running = false;
	bool m_updatingUi = false;
	qint64 m_remainingMs = 0;
	qint64 m_phaseMs = 0;
	std::chrono::steady_clock::time_point m_phaseStart;
	Config m_config;

	QSpinBox *m_hoursBox = nullptr;
	QSpinBox *m_minutesBox = nullptr;
	QSpinBox *m_secondsBox = nullptr;
	QComboBox *m_formatCombo = nullptr;
	QLabel *m_timeLabel = nullptr;
	QPushButton *m_startPauseButton = nullptr;
	QPushButton *m_resetButton = nullptr;
	QLineEdit *m_pathEdit = nullptr;
	QPushButton *m_browseButton = nullptr;
	QLabel *m_errorLabel = nullptr;
};

} // namespace obstimer
