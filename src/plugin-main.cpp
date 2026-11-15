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

#include <obs-module.h>

#include <obs-frontend-api.h>

#include "timer-dock.hpp"

#include <QMetaObject>

#ifndef OBS_TIMER_NAME
#define OBS_TIMER_NAME "obs-timer"
#endif

#ifndef OBS_TIMER_VERSION
#define OBS_TIMER_VERSION "0.0.0"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(OBS_TIMER_NAME, "en-US")

static bool s_dockRegistered = false;

static obs_hotkey_id s_hotkeyStartPause = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id s_hotkeyReset = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id s_hotkeySave = OBS_INVALID_HOTKEY_ID;

static obstimer::TimerDock *timerDock()
{
	return obstimer::TimerDock::instance();
}

/* ------------------------------------------------------------------------- */
/* Dock                                                                      */
/* ------------------------------------------------------------------------- */

static bool registerDock()
{
	if (s_dockRegistered)
		return true;

	auto *widget = new obstimer::TimerDock(nullptr);
	if (!obs_frontend_add_dock_by_id(OBS_TIMER_NAME, obs_module_text("Timer.Dock.Title"), widget)) {
		blog(LOG_WARNING, "[%s] failed to register dock '%s'", OBS_TIMER_NAME, OBS_TIMER_NAME);
		delete widget;
		return false;
	}

	s_dockRegistered = true;
	blog(LOG_INFO, "[%s] dock registered", OBS_TIMER_NAME);
	return true;
}

/* ------------------------------------------------------------------------- */
/* Hotkeys                                                                   */
/* ------------------------------------------------------------------------- */

static void hotkeyStartPause(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;

	if (auto *widget = timerDock())
		QMetaObject::invokeMethod(widget, "startOrPause", Qt::QueuedConnection);
}

static void hotkeyReset(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;

	if (auto *widget = timerDock())
		QMetaObject::invokeMethod(widget, "resetTimer", Qt::QueuedConnection);
}

static void hotkeySave(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;

	if (auto *widget = timerDock())
		QMetaObject::invokeMethod(widget, "saveNow", Qt::QueuedConnection);
}

static void registerHotkeys()
{
	s_hotkeyStartPause = obs_hotkey_register_frontend(
		"obs-timer.start_pause", obs_module_text("Timer.Hotkey.StartPause"), hotkeyStartPause, nullptr);
	s_hotkeyReset = obs_hotkey_register_frontend("obs-timer.reset", obs_module_text("Timer.Hotkey.Reset"),
						     hotkeyReset, nullptr);
	s_hotkeySave = obs_hotkey_register_frontend("obs-timer.save", obs_module_text("Timer.Hotkey.Save"),
						    hotkeySave, nullptr);
}

static void unregisterHotkeys()
{
	if (s_hotkeyStartPause != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(s_hotkeyStartPause);
		s_hotkeyStartPause = OBS_INVALID_HOTKEY_ID;
	}
	if (s_hotkeyReset != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(s_hotkeyReset);
		s_hotkeyReset = OBS_INVALID_HOTKEY_ID;
	}
	if (s_hotkeySave != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(s_hotkeySave);
		s_hotkeySave = OBS_INVALID_HOTKEY_ID;
	}
}

/* ------------------------------------------------------------------------- */
/* Persistence of hotkey bindings (stored in the scene collection)            */
/* ------------------------------------------------------------------------- */

static void saveHotkeyArray(obs_data_t *data, const char *name, obs_hotkey_id id)
{
	if (id == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_array_t *array = obs_hotkey_save(id);
	if (!array)
		return;

	obs_data_set_array(data, name, array);
	obs_data_array_release(array);
}

static void loadHotkeyArray(obs_data_t *data, const char *name, obs_hotkey_id id)
{
	if (id == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_array_t *array = obs_data_get_array(data, name);
	if (!array)
		return;

	obs_hotkey_load(id, array);
	obs_data_array_release(array);
}

static void hotkeysSaveLoadCallback(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *data = obs_data_create();
		saveHotkeyArray(data, "start_pause", s_hotkeyStartPause);
		saveHotkeyArray(data, "reset", s_hotkeyReset);
		saveHotkeyArray(data, "save", s_hotkeySave);
		obs_data_set_obj(save_data, OBS_TIMER_NAME, data);
		obs_data_release(data);
		return;
	}

	obs_data_t *data = obs_data_get_obj(save_data, OBS_TIMER_NAME);
	if (!data)
		return;

	loadHotkeyArray(data, "start_pause", s_hotkeyStartPause);
	loadHotkeyArray(data, "reset", s_hotkeyReset);
	loadHotkeyArray(data, "save", s_hotkeySave);
	obs_data_release(data);
}

/* ------------------------------------------------------------------------- */
/* Frontend events                                                           */
/* ------------------------------------------------------------------------- */

static void frontendEventCallback(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		registerDock();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED: {
		auto *widget = timerDock();
		if (!widget)
			break;

		/* Both outputs can be active at the same time, so resolve the
		 * combined state instead of trusting the event alone. */
		const bool active = obs_frontend_streaming_active() || obs_frontend_recording_active();
		QMetaObject::invokeMethod(widget, "handleOutputStateChanged", Qt::QueuedConnection,
					  Q_ARG(bool, active));
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		if (auto *widget = timerDock())
			widget->shutdown();
		break;
	default:
		break;
	}
}

/* ------------------------------------------------------------------------- */
/* Module entry points                                                       */
/* ------------------------------------------------------------------------- */

bool obs_module_load(void)
{
	blog(LOG_INFO, "[%s] loaded, version %s", OBS_TIMER_NAME, OBS_TIMER_VERSION);

	registerDock();
	registerHotkeys();

	obs_frontend_add_event_callback(frontendEventCallback, nullptr);
	obs_frontend_add_save_callback(hotkeysSaveLoadCallback, nullptr);

	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(hotkeysSaveLoadCallback, nullptr);
	obs_frontend_remove_event_callback(frontendEventCallback, nullptr);

	unregisterHotkeys();

	if (auto *widget = timerDock()) {
		widget->persistSettings();
		/* OBS deletes the dock (and with it our widget) here. */
		obs_frontend_remove_dock(OBS_TIMER_NAME);
	}

	s_dockRegistered = false;
	blog(LOG_INFO, "[%s] unloaded", OBS_TIMER_NAME);
}
