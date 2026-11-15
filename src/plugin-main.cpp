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

#include <obs-module.h>

#include <obs-frontend-api.h>

#include "timer-dock.hpp"

#ifndef OBS_TIMER_NAME
#define OBS_TIMER_NAME "obs-timer"
#endif

#ifndef OBS_TIMER_VERSION
#define OBS_TIMER_VERSION "0.0.0"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(OBS_TIMER_NAME, "en-US")

bool obs_module_load(void)
{
	blog(LOG_INFO, "[%s] loaded, version %s", OBS_TIMER_NAME, OBS_TIMER_VERSION);

	auto *widget = new obstimer::TimerDock(nullptr);
	if (!obs_frontend_add_dock_by_id(OBS_TIMER_NAME, obs_module_text("Timer.Dock.Title"), widget)) {
		blog(LOG_WARNING, "[%s] failed to register the dock", OBS_TIMER_NAME);
		delete widget;
		return true;
	}

	blog(LOG_INFO, "[%s] dock registered", OBS_TIMER_NAME);
	return true;
}

void obs_module_unload(void)
{
	if (obstimer::TimerDock::instance()) {
		/* OBS deletes the dock (and with it our widget) here. */
		obs_frontend_remove_dock(OBS_TIMER_NAME);
	}

	blog(LOG_INFO, "[%s] unloaded", OBS_TIMER_NAME);
}
