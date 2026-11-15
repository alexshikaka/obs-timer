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

#include "timer-dock.hpp"

#include <obs-module.h>

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace obstimer {

TimerDock *TimerDock::s_instance = nullptr;

namespace {
/* How often the countdown is polled.  The file itself is written once per
 * whole second, the finer poll is only there to keep the switch accurate. */
constexpr int kTickIntervalMs = 100;
constexpr int kMaxHours = 99;
constexpr int kMaxMinutes = 59;
constexpr int kMaxSeconds = 59;
} // namespace

TimerDock::TimerDock(QWidget *parent) : QWidget(parent)
{
	m_config = loadConfig();

	buildUi();
	applyConfig();

	m_remainingMs = configuredMs();

	m_tickTimer = new QTimer(this);
	m_tickTimer->setInterval(kTickIntervalMs);
	connect(m_tickTimer, &QTimer::timeout, this, &TimerDock::onTick);

	updateDisplay();
	updateStartPauseButton();

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

	/* --- hours / minutes / seconds ------------------------------------- */
	auto *fieldsRow = new QHBoxLayout();
	fieldsRow->setSpacing(6);

	m_hoursBox = new QSpinBox(this);
	m_hoursBox->setRange(0, kMaxHours);
	m_hoursBox->setAlignment(Qt::AlignCenter);
	m_minutesBox = new QSpinBox(this);
	m_minutesBox->setRange(0, kMaxMinutes);
	m_minutesBox->setAlignment(Qt::AlignCenter);
	m_secondsBox = new QSpinBox(this);
	m_secondsBox->setRange(0, kMaxSeconds);
	m_secondsBox->setAlignment(Qt::AlignCenter);

	const struct {
		QSpinBox *box;
		const char *label;
	} fields[] = {
		{m_hoursBox, "Timer.Label.Hours"},
		{m_minutesBox, "Timer.Label.Minutes"},
		{m_secondsBox, "Timer.Label.Seconds"},
	};

	for (const auto &field : fields) {
		auto *column = new QVBoxLayout();
		column->setSpacing(2);
		column->addWidget(field.box);

		auto *label = new QLabel(QString::fromUtf8(obs_module_text(field.label)), this);
		label->setAlignment(Qt::AlignCenter);
		column->addWidget(label);

		connect(field.box, &QSpinBox::valueChanged, this, &TimerDock::onDurationChanged);
		fieldsRow->addLayout(column);
	}

	root->addLayout(fieldsRow);

	/* --- quick minute buttons ------------------------------------------ */
	auto *presetsRow = new QHBoxLayout();
	presetsRow->setSpacing(3);

	for (int minutes = 1; minutes <= 9; minutes++) {
		auto *button = new QPushButton(QString::number(minutes), this);
		button->setFixedWidth(26);
		connect(button, &QPushButton::clicked, this, [this, minutes]() { m_minutesBox->setValue(minutes); });
		presetsRow->addWidget(button);
	}

	auto *tenButton = new QPushButton(QString::fromUtf8(obs_module_text("Timer.Preset.TenMinutes")), this);
	tenButton->setToolTip(QString::fromUtf8(obs_module_text("Timer.Preset.TenMinutes.Hint")));
	connect(tenButton, &QPushButton::clicked, this, [this]() { m_minutesBox->setValue(10); });
	presetsRow->addWidget(tenButton, 1);

	root->addLayout(presetsRow);

	/* --- display format ------------------------------------------------ */
	m_formatCombo = new QComboBox(this);
	m_formatCombo->addItem(QString::fromUtf8(obs_module_text("Timer.Format.HoursMinutesSeconds")));
	m_formatCombo->addItem(QString::fromUtf8(obs_module_text("Timer.Format.MinutesSeconds")));
	m_formatCombo->addItem(QString::fromUtf8(obs_module_text("Timer.Format.SecondsOnly")));
	m_formatCombo->setToolTip(QString::fromUtf8(obs_module_text("Timer.Format.Hint")));
	connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &TimerDock::onFormatChanged);
	root->addWidget(m_formatCombo);

	/* --- the timer itself ----------------------------------------------- */
	m_timeLabel = new QLabel(QStringLiteral("00:00:00"), this);
	m_timeLabel->setAlignment(Qt::AlignCenter);
	m_timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	monoFont.setPointSize(24);
	monoFont.setBold(true);
	m_timeLabel->setFont(monoFont);
	m_timeLabel->setMinimumHeight(56);
	root->addWidget(m_timeLabel);

	/* --- start / stop --------------------------------------------------- */
	auto *buttonsRow = new QHBoxLayout();
	buttonsRow->setSpacing(6);

	m_startPauseButton = new QPushButton(this);
	m_startPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	connect(m_startPauseButton, &QPushButton::clicked, this, &TimerDock::startOrPause);
	buttonsRow->addWidget(m_startPauseButton, 1);

	m_resetButton = new QPushButton(QString::fromUtf8(obs_module_text("Timer.Button.Stop")), this);
	m_resetButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	connect(m_resetButton, &QPushButton::clicked, this, &TimerDock::reset);
	buttonsRow->addWidget(m_resetButton, 1);

	root->addLayout(buttonsRow);

	/* --- output file ---------------------------------------------------- */
	m_pathEdit = new QLineEdit(this);
	m_pathEdit->setPlaceholderText(QString::fromUtf8(obs_module_text("Timer.Label.File")));
	connect(m_pathEdit, &QLineEdit::editingFinished, this, &TimerDock::onFilePathEdited);
	root->addWidget(m_pathEdit);

	m_errorLabel = new QLabel(this);
	m_errorLabel->setWordWrap(true);
	m_errorLabel->setStyleSheet(QStringLiteral("color: #e0a030;"));
	m_errorLabel->hide();
	root->addWidget(m_errorLabel);

	root->addStretch(1);
}

void TimerDock::applyConfig()
{
	m_updatingUi = true;

	m_hoursBox->setValue(m_config.hours);
	m_minutesBox->setValue(m_config.minutes);
	m_secondsBox->setValue(m_config.seconds);
	m_formatCombo->setCurrentIndex((int)m_config.format);
	m_pathEdit->setText(m_config.filePath);

	m_updatingUi = false;
}

void TimerDock::persistSettings()
{
	saveConfig(m_config);
}

qint64 TimerDock::configuredMs() const
{
	const qint64 total = (qint64)m_config.hours * 3600 + (qint64)m_config.minutes * 60 + m_config.seconds;
	return total * 1000;
}

qint64 TimerDock::remainingMs() const
{
	if (!m_running)
		return m_remainingMs;

	const auto now = std::chrono::steady_clock::now();
	const qint64 left = std::chrono::duration_cast<std::chrono::milliseconds>(m_deadline - now).count();
	return left > 0 ? left : 0;
}

void TimerDock::onDurationChanged()
{
	if (m_updatingUi)
		return;

	m_config.hours = m_hoursBox->value();
	m_config.minutes = m_minutesBox->value();
	m_config.seconds = m_secondsBox->value();

	persistSettings();

	/* Changing the duration always brings the timer back to the new
	 * starting value so the next Start uses it. */
	m_running = false;
	if (m_tickTimer)
		m_tickTimer->stop();

	m_remainingMs = configuredMs();
	m_lastWrittenSeconds = -1;

	updateDisplay();
	updateStartPauseButton();
}

void TimerDock::onFormatChanged(int index)
{
	if (m_updatingUi)
		return;

	m_config.format = static_cast<DisplayFormat>(index);
	persistSettings();

	updateDisplay();

	if (m_running)
		writeFile();
}

void TimerDock::onFilePathEdited()
{
	const QString path = m_pathEdit->text().trimmed();
	if (path == m_config.filePath)
		return;

	m_config.filePath = path;
	persistSettings();
}

void TimerDock::startOrPause()
{
	if (m_running) {
		m_remainingMs = remainingMs();
		m_running = false;

		if (m_tickTimer)
			m_tickTimer->stop();

		updateStartPauseButton();
		updateDisplay();
		return;
	}

	qint64 startValue = m_remainingMs;
	if (startValue <= 0)
		startValue = configuredMs();

	/* Nothing to count down: the duration is zero. */
	if (startValue <= 0)
		return;

	m_remainingMs = startValue;
	m_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(startValue);
	m_running = true;
	m_lastWrittenSeconds = -1;

	updateStartPauseButton();
	updateDisplay();
	writeFile();

	if (m_tickTimer)
		m_tickTimer->start();
}

void TimerDock::reset()
{
	m_running = false;
	if (m_tickTimer)
		m_tickTimer->stop();

	m_remainingMs = configuredMs();
	m_lastWrittenSeconds = -1;

	updateStartPauseButton();
	updateDisplay();
	writeFile();
}

void TimerDock::onTick()
{
	if (!m_running)
		return;

	const qint64 left = remainingMs();
	if (left <= 0) {
		m_remainingMs = 0;
		m_running = false;

		if (m_tickTimer)
			m_tickTimer->stop();

		updateStartPauseButton();
		updateDisplay();
		writeFile();
		return;
	}

	const qint64 seconds = (left + 999) / 1000;
	if (seconds == m_lastWrittenSeconds)
		return;

	m_lastWrittenSeconds = seconds;
	m_remainingMs = left;

	updateDisplay();
	writeFile();
}

void TimerDock::updateDisplay()
{
	m_timeLabel->setText(formatDuration(remainingMs()));
}

void TimerDock::updateStartPauseButton()
{
	m_startPauseButton->setText(
		QString::fromUtf8(obs_module_text(m_running ? "Timer.Button.Pause" : "Timer.Button.Start")));
	m_startPauseButton->setIcon(
		style()->standardIcon(m_running ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

QString TimerDock::formatDuration(qint64 milliseconds) const
{
	qint64 total = (milliseconds + 999) / 1000;
	if (total < 0)
		total = 0;

	const qint64 hours = total / 3600;
	const qint64 minutes = (total / 60) % 60;
	const qint64 seconds = total % 60;

	switch (m_config.format) {
	case DisplayFormat::HoursMinutesSeconds:
		return QString::asprintf("%02lld:%02lld:%02lld", (long long)hours, (long long)minutes,
					 (long long)seconds);
	case DisplayFormat::MinutesSeconds:
		return QString::asprintf("%02lld:%02lld", (long long)(total / 60), (long long)seconds);
	case DisplayFormat::SecondsOnly:
		return QString::asprintf("%lld", (long long)total);
	}

	return QStringLiteral("00:00:00");
}

void TimerDock::writeFile()
{
	const QString path = m_config.filePath.trimmed();
	if (path.isEmpty()) {
		setError(QString::fromUtf8(obs_module_text("Timer.Error.NoFile")));
		return;
	}

	const QDir folder = QFileInfo(path).absoluteDir();
	if (!folder.exists() && !folder.mkpath(QStringLiteral("."))) {
		setError(QString::fromUtf8(obs_module_text("Timer.Error.NoDirectory")).arg(folder.absolutePath()));
		return;
	}

	const QString text = formatDuration(remainingMs());

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		setError(QString::fromUtf8(obs_module_text("Timer.Error.Write")).arg(path, file.errorString()));
		return;
	}

	const QByteArray payload = text.toUtf8();
	const bool written = file.write(payload) == payload.size();
	file.close();

	if (!written) {
		setError(QString::fromUtf8(obs_module_text("Timer.Error.Write")).arg(path, file.errorString()));
		return;
	}

	setError(QString());
}

void TimerDock::setError(const QString &text)
{
	if (text.isEmpty()) {
		m_errorLabel->clear();
		m_errorLabel->hide();
		return;
	}

	m_errorLabel->setText(text);
	m_errorLabel->show();
}

} // namespace obstimer
