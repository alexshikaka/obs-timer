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

#include "timer-dock.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace obstimer {

TimerDock *TimerDock::s_instance = nullptr;

namespace {
constexpr int kTickIntervalMs = 100;
constexpr int kConfigSaveDelayMs = 400;
} // namespace

TimerDock::TimerDock(QWidget *parent) : QWidget(parent)
{
	m_config = loadConfig();
	m_clock.start();

	buildUi();
	applyConfig();

	m_tickTimer = new QTimer(this);
	m_tickTimer->setInterval(kTickIntervalMs);
	connect(m_tickTimer, &QTimer::timeout, this, &TimerDock::onTick);
	m_tickTimer->start();

	m_configSaveTimer = new QTimer(this);
	m_configSaveTimer->setSingleShot(true);
	m_configSaveTimer->setInterval(kConfigSaveDelayMs);
	connect(m_configSaveTimer, &QTimer::timeout, this, [this]() { persistSettings(); });

	s_instance = this;
}

TimerDock::~TimerDock()
{
	if (s_instance == this)
		s_instance = nullptr;
}

void TimerDock::buildUi()
{
	setMinimumWidth(300);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	m_timeLabel = new QLabel(QStringLiteral("00:00:00"), this);
	m_timeLabel->setAlignment(Qt::AlignCenter);
	m_timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	monoFont.setPointSize(24);
	monoFont.setBold(true);
	m_timeLabel->setFont(monoFont);
	m_timeLabel->setMinimumHeight(56);
	root->addWidget(m_timeLabel);

	auto *buttonRow = new QHBoxLayout();
	buttonRow->setSpacing(6);

	m_startPauseButton = new QPushButton(this);
	m_startPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	connect(m_startPauseButton, &QPushButton::clicked, this, &TimerDock::startOrPause);
	buttonRow->addWidget(m_startPauseButton);

	m_resetButton = new QPushButton(this);
	m_resetButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	connect(m_resetButton, &QPushButton::clicked, this, &TimerDock::resetTimer);
	buttonRow->addWidget(m_resetButton);

	m_saveButton = new QPushButton(this);
	m_saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
	connect(m_saveButton, &QPushButton::clicked, this, &TimerDock::saveNow);
	buttonRow->addWidget(m_saveButton);

	root->addLayout(buttonRow);

	auto *pathRow = new QHBoxLayout();
	pathRow->setSpacing(6);
	m_pathEdit = new QLineEdit(this);
	m_pathEdit->setPlaceholderText(QStringLiteral("obs-timer.txt"));
	connect(m_pathEdit, &QLineEdit::textEdited, this, &TimerDock::onConfigChanged);
	pathRow->addWidget(m_pathEdit, 1);

	m_browseButton = new QPushButton(QStringLiteral("..."), this);
	m_browseButton->setFixedWidth(32);
	connect(m_browseButton, &QPushButton::clicked, this, &TimerDock::onBrowseClicked);
	pathRow->addWidget(m_browseButton);
	root->addLayout(pathRow);

	auto *intervalRow = new QHBoxLayout();
	intervalRow->setSpacing(6);
	m_intervalCombo = new QComboBox(this);
	for (int i = 0; i <= static_cast<int>(SaveInterval::OnStop); i++)
		m_intervalCombo->addItem(intervalLabel(static_cast<SaveInterval>(i)));
	connect(m_intervalCombo, &QComboBox::currentIndexChanged, this, &TimerDock::onConfigChanged);
	intervalRow->addWidget(m_intervalCombo, 1);
	root->addLayout(intervalRow);

	m_overwriteBox = new QCheckBox(this);
	m_millisecondsBox = new QCheckBox(this);
	m_autoStartBox = new QCheckBox(this);
	m_autoStopBox = new QCheckBox(this);
	m_resetOnStartBox = new QCheckBox(this);

	for (QCheckBox *box : {m_overwriteBox, m_millisecondsBox, m_autoStartBox, m_autoStopBox, m_resetOnStartBox}) {
		connect(box, &QCheckBox::toggled, this, &TimerDock::onConfigChanged);
		root->addWidget(box);
	}

	m_statusLabel = new QLabel(this);
	m_statusLabel->setWordWrap(true);
	m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	root->addWidget(m_statusLabel);

	root->addStretch(1);
}

void TimerDock::applyConfig()
{
	m_updatingUi = true;

	m_pathEdit->setText(m_config.filePath);
	m_pathEdit->setToolTip(m_config.filePath);
	m_intervalCombo->setCurrentIndex(static_cast<int>(m_config.interval));
	m_overwriteBox->setChecked(m_config.overwrite);
	m_millisecondsBox->setChecked(m_config.showMilliseconds);
	m_autoStartBox->setChecked(m_config.autoStartWithOutput);
	m_autoStopBox->setChecked(m_config.autoStopWithOutput);
	m_resetOnStartBox->setChecked(m_config.resetOnStart);

	/* Labels have to be refreshed explicitly: OBS translates module strings
	 * through obs_module_text() and the language may change at runtime. */
	m_startPauseButton->setToolTip(localised("Timer.Button.StartPause.Hint"));
	m_resetButton->setText(localised("Timer.Button.Reset"));
	m_saveButton->setText(localised("Timer.Button.Save"));
	m_browseButton->setToolTip(localised("Timer.Button.Browse"));
	m_pathEdit->setToolTip(m_config.filePath.isEmpty() ? localised("Timer.Label.File") : m_config.filePath);
	m_intervalCombo->setToolTip(localised("Timer.Label.Interval"));
	m_overwriteBox->setText(localised("Timer.Check.Overwrite"));
	m_millisecondsBox->setText(localised("Timer.Check.Milliseconds"));
	m_autoStartBox->setText(localised("Timer.Check.AutoStart"));
	m_autoStopBox->setText(localised("Timer.Check.AutoStop"));
	m_resetOnStartBox->setText(localised("Timer.Check.ResetOnStart"));

	m_updatingUi = false;

	updateStartPauseButton();
	updateDisplay();
}

Config TimerDock::collectConfig() const
{
	Config config;
	config.filePath = m_pathEdit->text().trimmed();
	config.interval = static_cast<SaveInterval>(m_intervalCombo->currentIndex());
	config.overwrite = m_overwriteBox->isChecked();
	config.showMilliseconds = m_millisecondsBox->isChecked();
	config.autoStartWithOutput = m_autoStartBox->isChecked();
	config.autoStopWithOutput = m_autoStopBox->isChecked();
	config.resetOnStart = m_resetOnStartBox->isChecked();
	return config;
}

void TimerDock::onConfigChanged()
{
	if (m_updatingUi)
		return;

	m_config = collectConfig();
	m_pathEdit->setToolTip(m_config.filePath.isEmpty() ? localised("Timer.Label.File") : m_config.filePath);
	updateDisplay();

	if (m_configSaveTimer)
		m_configSaveTimer->start();
}

void TimerDock::persistSettings()
{
	saveConfig(m_config);
}

void TimerDock::onBrowseClicked()
{
	const QString start = m_config.filePath.isEmpty() ? defaultFilePath() : m_config.filePath;
	const QString chosen = QFileDialog::getSaveFileName(this, localised("Timer.Dialog.ChooseFile"), start,
							    localised("Timer.Dialog.FileFilter"));
	if (chosen.isEmpty())
		return;

	m_pathEdit->setText(chosen);
	m_config = collectConfig();
	persistSettings();
}

void TimerDock::startOrPause()
{
	if (m_running)
		pauseTimer();
	else
		startTimer(false);
}

void TimerDock::startTimer(bool reset)
{
	if (reset) {
		m_baseMs = 0;
		m_lastWriteMs = -1;
	}

	if (m_running) {
		updateDisplay();
		return;
	}

	m_clock.restart();
	m_running = true;
	updateStartPauseButton();
	updateDisplay();

	if (m_config.overwrite) {
		QString error;
		if (!writeCurrentValue(&error))
			setStatus(error, true);
	}
}

void TimerDock::pauseTimer()
{
	if (!m_running)
		return;

	m_baseMs += m_clock.elapsed();
	m_running = false;
	m_lastWriteMs = -1;

	updateStartPauseButton();
	updateDisplay();

	QString error;
	const bool ok = m_config.overwrite ? writeCurrentValue(&error) : appendSessionLine(&error);
	if (!ok)
		setStatus(error, true);
}

void TimerDock::resetTimer()
{
	m_baseMs = 0;
	m_lastWriteMs = -1;
	if (m_running)
		m_clock.restart();

	updateDisplay();

	if (m_config.overwrite && !m_config.filePath.trimmed().isEmpty()) {
		QString error;
		if (!writeCurrentValue(&error))
			setStatus(error, true);
	}
}

void TimerDock::saveNow()
{
	QString error;
	const bool ok = m_config.overwrite ? writeCurrentValue(&error) : appendSessionLine(&error);
	if (!ok)
		setStatus(error, true);
}

void TimerDock::handleOutputStateChanged(bool active)
{
	if (m_outputActive == active)
		return;

	m_outputActive = active;

	if (active) {
		if (m_config.autoStartWithOutput)
			startTimer(m_config.resetOnStart);
		return;
	}

	if (m_config.autoStopWithOutput && m_running)
		pauseTimer();
}

void TimerDock::shutdown()
{
	if (m_running)
		pauseTimer();

	persistSettings();
}

void TimerDock::onTick()
{
	updateDisplay();

	if (!m_running)
		return;

	const int interval = intervalMilliseconds(m_config.interval);
	if (interval <= 0)
		return;

	const qint64 now = elapsedMilliseconds();
	if (m_lastWriteMs >= 0 && (now - m_lastWriteMs) < interval)
		return;

	QString error;
	if (!writeCurrentValue(&error))
		setStatus(error, true);
}

bool TimerDock::writeCurrentValue(QString *error)
{
	const QString path = targetFilePath();
	if (path.isEmpty()) {
		if (error)
			*error = localised("Timer.Status.NoFile");
		return false;
	}

	const QDir folder = QFileInfo(path).absoluteDir();
	if (!folder.exists() && !folder.mkpath(QStringLiteral("."))) {
		if (error)
			*error = localised("Timer.Status.NoDirectory").arg(folder.absolutePath());
		return false;
	}

	const QString text = formatDuration(elapsedMilliseconds(), m_config.showMilliseconds);

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		if (error)
			*error = localised("Timer.Status.WriteError").arg(path, file.errorString());
		return false;
	}

	const QByteArray payload = text.toUtf8();
	const bool written = file.write(payload) == payload.size();
	file.close();

	if (!written) {
		if (error)
			*error = localised("Timer.Status.WriteError").arg(path, file.errorString());
		return false;
	}

	m_lastWriteMs = elapsedMilliseconds();
	setStatus(localised("Timer.Status.Saved").arg(text, path), false);
	return true;
}

bool TimerDock::appendSessionLine(QString *error)
{
	const QString path = targetFilePath();
	if (path.isEmpty()) {
		if (error)
			*error = localised("Timer.Status.NoFile");
		return false;
	}

	const QDir folder = QFileInfo(path).absoluteDir();
	if (!folder.exists() && !folder.mkpath(QStringLiteral("."))) {
		if (error)
			*error = localised("Timer.Status.NoDirectory").arg(folder.absolutePath());
		return false;
	}

	const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
	const QString line = QStringLiteral("%1\t%2\n")
				     .arg(stamp, formatDuration(elapsedMilliseconds(), m_config.showMilliseconds));

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
		if (error)
			*error = localised("Timer.Status.WriteError").arg(path, file.errorString());
		return false;
	}

	const QByteArray payload = line.toUtf8();
	const bool written = file.write(payload) == payload.size();
	file.close();

	if (!written) {
		if (error)
			*error = localised("Timer.Status.WriteError").arg(path, file.errorString());
		return false;
	}

	setStatus(localised("Timer.Status.Appended").arg(path), false);
	return true;
}

void TimerDock::setStatus(const QString &text, bool warning)
{
	if (m_statusLabel->text() == text)
		return;

	m_statusLabel->setText(text);
	m_statusLabel->setToolTip(text);
	m_statusLabel->setStyleSheet(warning ? QStringLiteral("color: #e0a030;") : QString());
}

void TimerDock::updateDisplay()
{
	m_timeLabel->setText(formatDuration(elapsedMilliseconds(), m_config.showMilliseconds));
}

void TimerDock::updateStartPauseButton()
{
	const bool running = m_running;
	m_startPauseButton->setText(running ? localised("Timer.Button.Pause") : localised("Timer.Button.Start"));
	m_startPauseButton->setIcon(style()->standardIcon(running ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

qint64 TimerDock::elapsedMilliseconds() const
{
	return m_baseMs + (m_running ? m_clock.elapsed() : 0);
}

QString TimerDock::formatDuration(qint64 milliseconds, bool withMilliseconds)
{
	const qint64 total = milliseconds > 0 ? milliseconds : 0;
	const qint64 hours = total / 3600000;
	const qint64 minutes = (total / 60000) % 60;
	const qint64 seconds = (total / 1000) % 60;

	QString text = QString::asprintf("%02lld:%02lld:%02lld", static_cast<long long>(hours),
					 static_cast<long long>(minutes), static_cast<long long>(seconds));

	if (withMilliseconds)
		text += QString::asprintf(".%03lld", static_cast<long long>(total % 1000));

	return text;
}

QString TimerDock::targetFilePath() const
{
	return m_config.filePath.trimmed();
}

QString TimerDock::localised(const char *key) const
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace obstimer
