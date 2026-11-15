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
#include <QFileDialog>
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

	/* --- 1. the timer itself ------------------------------------------- */
	m_timeLabel = new QLabel(this);
	m_timeLabel->setAlignment(Qt::AlignCenter);
	m_timeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	monoFont.setPointSize(20);
	monoFont.setBold(true);
	m_timeLabel->setFont(monoFont);
	m_timeLabel->setContentsMargins(0, 0, 0, 0);
	m_timeLabel->setMinimumHeight(34);
	root->addWidget(m_timeLabel);

	/* --- 2. start / stop ------------------------------------------------ */
	auto *buttonsRow = new QHBoxLayout();
	buttonsRow->setSpacing(6);

	m_startPauseButton = new QPushButton(this);
	m_startPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	connect(m_startPauseButton, &QPushButton::clicked, this, &TimerDock::startOrPause);
	buttonsRow->addWidget(m_startPauseButton, 1);

	m_resetButton = new QPushButton(QString::fromUtf8(obs_module_text("Timer.Button.Stop")), this);
	m_resetButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
	connect(m_resetButton, &QPushButton::clicked, this, &TimerDock::reset);
	buttonsRow->addWidget(m_resetButton, 1);

	root->addLayout(buttonsRow);

	/* --- 3. hours / minutes / seconds ----------------------------------- */
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

	/* --- 4. quick minute buttons ---------------------------------------- */
	auto *presetsRow = new QHBoxLayout();
	presetsRow->setSpacing(3);

	/* OBS theme gives QPushButton a large padding; zero it out so the digits
	 * fit into these small square buttons. */
	const QString presetStyle = QStringLiteral("QPushButton { padding: 0px; margin: 0px; }");

	QFont presetFont = font();
	presetFont.setPointSize(9);

	for (int minutes = 1; minutes <= 9; minutes++) {
		auto *button = new QPushButton(QString::number(minutes), this);
		button->setFont(presetFont);
		button->setFixedSize(24, 22);
		button->setStyleSheet(presetStyle);
		connect(button, &QPushButton::clicked, this, [this, minutes]() { m_minutesBox->setValue(minutes); });
		presetsRow->addWidget(button);
	}

	auto *tenButton = new QPushButton(QString::fromUtf8(obs_module_text("Timer.Preset.TenMinutes")), this);
	tenButton->setFont(presetFont);
	tenButton->setFixedSize(40, 22);
	tenButton->setStyleSheet(presetStyle);
	tenButton->setToolTip(QString::fromUtf8(obs_module_text("Timer.Preset.TenMinutes.Hint")));
	connect(tenButton, &QPushButton::clicked, this, [this]() { m_minutesBox->setValue(10); });
	presetsRow->addWidget(tenButton);

	presetsRow->addStretch(1);
	root->addLayout(presetsRow);

	/* --- 5. display format ---------------------------------------------- */
	m_formatCombo = new QComboBox(this);
	m_formatCombo->addItem(QString::fromUtf8(obs_module_text("Timer.Format.HoursMinutesSeconds")));
	m_formatCombo->addItem(QString::fromUtf8(obs_module_text("Timer.Format.MinutesSeconds")));
	m_formatCombo->setToolTip(QString::fromUtf8(obs_module_text("Timer.Format.Hint")));
	connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &TimerDock::onFormatChanged);
	root->addWidget(m_formatCombo);

	/* --- 6. output file -------------------------------------------------- */
	auto *pathRow = new QHBoxLayout();
	pathRow->setSpacing(6);

	m_pathEdit = new QLineEdit(this);
	m_pathEdit->setPlaceholderText(QString::fromUtf8(obs_module_text("Timer.Label.File")));
	connect(m_pathEdit, &QLineEdit::editingFinished, this, &TimerDock::onFilePathEdited);
	pathRow->addWidget(m_pathEdit, 1);

	m_browseButton = new QPushButton(this);
	m_browseButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
	m_browseButton->setToolTip(QString::fromUtf8(obs_module_text("Timer.Button.Browse")));
	connect(m_browseButton, &QPushButton::clicked, this, &TimerDock::onBrowseClicked);
	pathRow->addWidget(m_browseButton);

	root->addLayout(pathRow);

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

qint64 TimerDock::elapsedMs() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
								    m_phaseStart)
		.count();
}

/* Remaining time snapped to whole seconds: the value the timer shows and
 * the value that goes into the file, so both change at the same moment. */
qint64 TimerDock::shownMs() const
{
	if (!m_running)
		return m_remainingMs;

	const qint64 left = m_phaseMs - (elapsedMs() / 1000) * 1000;
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

	/* Changing the duration brings the timer back to the new start value. */
	m_running = false;
	m_remainingMs = configuredMs();

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

void TimerDock::onBrowseClicked()
{
	const QString current = m_config.filePath.trimmed().isEmpty() ? defaultFilePath() : m_config.filePath;

	const QString chosen = QFileDialog::getSaveFileName(this, QString::fromUtf8(obs_module_text("Timer.Dialog.SaveFile")),
							   current,
							   QString::fromUtf8(obs_module_text("Timer.Dialog.FileFilter")));
	if (chosen.isEmpty())
		return;

	m_pathEdit->setText(chosen);
	m_config.filePath = chosen;
	persistSettings();

	if (m_running)
		writeFile();
}

void TimerDock::startOrPause()
{
	if (m_running) {
		m_remainingMs = shownMs();
		m_running = false;

		updateStartPauseButton();
		updateDisplay();
		return;
	}

	qint64 startValue = m_remainingMs;
	if (startValue <= 0)
		startValue = configuredMs();

	/* Nothing to count down. */
	if (startValue <= 0)
		return;

	m_remainingMs = startValue;
	m_phaseMs = startValue;
	m_phaseStart = std::chrono::steady_clock::now();
	m_running = true;

	updateStartPauseButton();
	updateDisplay();
	writeFile();

	scheduleNextTick();
}

void TimerDock::reset()
{
	m_running = false;
	m_remainingMs = configuredMs();

	updateStartPauseButton();
	updateDisplay();
	writeFile();
}

void TimerDock::finish()
{
	m_running = false;
	m_remainingMs = 0;

	updateStartPauseButton();
	updateDisplay();
	writeFile();
}

/* Wake up exactly on the next whole-second boundary of the countdown. */
void TimerDock::scheduleNextTick()
{
	if (!m_running)
		return;

	const qint64 elapsed = elapsedMs();
	const qint64 next = (elapsed / 1000 + 1) * 1000;
	qint64 delay = next - elapsed;
	if (delay < 1)
		delay = 1;

	QTimer::singleShot((int)delay, Qt::PreciseTimer, this, &TimerDock::onTick);
}

void TimerDock::onTick()
{
	if (!m_running)
		return;

	if (shownMs() <= 0) {
		finish();
		return;
	}

	updateDisplay();
	writeFile();

	scheduleNextTick();
}

void TimerDock::updateDisplay()
{
	m_timeLabel->setText(formatDuration(shownMs()));
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
	qint64 total = milliseconds / 1000;
	if (total < 0)
		total = 0;

	const qint64 hours = total / 3600;
	const qint64 minutes = (total / 60) % 60;
	const qint64 seconds = total % 60;

	if (m_config.format == DisplayFormat::MinutesSeconds)
		return QString::asprintf("%02lld:%02lld", (long long)(total / 60), (long long)seconds);

	return QString::asprintf("%02lld:%02lld:%02lld", (long long)hours, (long long)minutes, (long long)seconds);
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

	const QString text = formatDuration(shownMs());

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
