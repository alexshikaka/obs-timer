# obs-timer

A **countdown timer dock** for [OBS Studio](https://obsproject.com) that rewrites a plain text
file **every second** with the remaining time, so a *Text (GDI+)* source reading that file
shows a real, live countdown on stream.

Built and tested against **OBS Studio 33.0.1** on Windows x64.

---

## How it works

```
┌ Timer ──────────────────────┐
│  [ 0 ]   [ 5 ]   [ 0 ]      │  hours / minutes / seconds
│  hours   minutes  seconds   │
│                             │
│ [1][2][3][4][5][6][7][8][9] │  quick buttons: set the minutes
│ [          10min          ] │
│                             │
│ [ Hours:minutes:seconds ▾ ] │  what to show / write
│                             │
│        00:05:00             │  the timer
│                             │
│   [ ▶ Start ]  [ ⟲ Stop ]   │
│                             │
│ C:\Users\...\obs-timer.txt  │  output file
└─────────────────────────────┘
```

* Set **hours / minutes / seconds** (or hit `1`–`9` / `10min` to set the minutes).
* Pick the **format**: `HH:MM:SS`, `MM:SS` or plain seconds. The separator is always `:`.
* Press **Start** — the button turns into **Pause**, and the countdown begins.
* **Stop** cancels the countdown and restores the chosen duration, so you can start again
  without re-entering it.
* While counting down, the file is rewritten once per second with the remaining time.
  At zero the timer stops on `00:00:00`.

## Using it on stream

1. Set the duration in the dock and press **Start** once to make sure the file appears.
2. In OBS add a **Text (GDI+)** source, enable **Read from file** and point it at the same
   file shown in the dock (`Documents/obs-timer.txt` by default).
3. Press **Start** when you want the countdown to run — the overlay follows the file.

## Installation

OBS Studio 33 changed where third-party plugins live; this plugin uses the new layout:

```
C:\ProgramData\obs-studio\plugins\obs-timer\obs-timer.dll
C:\ProgramData\obs-studio\plugins\obs-timer\data\locale\...
```

1. Download `obs-timer-<version>-windows-x64.zip` from the releases page.
2. Extract the `obs-timer` folder into `C:\ProgramData\obs-studio\plugins\`.
3. Restart OBS Studio.
4. Open the panel with **View → Docks → Timer** (it starts floating and hidden).

To uninstall, delete the `obs-timer` folder and restart OBS.

## Building from source

Requirements: Windows 10/11, Visual Studio 2022 (MSVC, Windows SDK 10.0.22621), CMake 3.28+,
PowerShell 7. Dependencies (OBS Studio 33.0.1 sources, pre-built OBS deps and Qt 6) are
downloaded into `.deps/` automatically during the CMake configure step.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel
cmake --install build_x64 --prefix release/RelWithDebInfo --config RelWithDebInfo
```

The first configure builds `libobs` and `obs-frontend-api` from the OBS sources so the plugin
links against the exact 33.0.1 ABI. CI (`.github/workflows/build.yaml`) runs the same steps on
`windows-2022`.

## Notes

* Windows x64 only for now.
* Changing the duration while the countdown is running stops it and loads the new value.
* The plugin adds no sources, filters or outputs — it is a frontend dock only.

## Русский

**obs-timer** — док-панель с обратным отсчётом. Задаёте часы/минуты/секунды (или жмёте `1`–`9`
и `10min`), выбираете формат (`Ч:М:С`, `М:С`, секунды), жмёте **Старт**. Кнопка превращается в
**Паузу**, а в текстовый файл каждую секунду пишется остаток — его можно подключить к источнику
«Текст (GDI+)» с опцией «Читать из файла» и получить живой отсчёт в эфире. **Стоп** сбрасывает
таймер к заданной длительности.

Установка: распакуйте папку `obs-timer` из zip в `C:\ProgramData\obs-studio\plugins\` и
перезапустите OBS. Панель — **Вид → Док-панели → Таймер**.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
