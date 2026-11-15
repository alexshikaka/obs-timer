# obs-timer

A **countdown timer dock** for [OBS Studio](https://obsproject.com) that rewrites a plain text
file **in lock-step with the countdown** (once per whole second), so a *Text (GDI+)* source
reading that file shows a real, live countdown on stream.

Built and tested against **OBS Studio 33.0.1** on Windows x64.

---

## The panel

Top to bottom:

```
┌ Timer ──────────────────────┐
│        00:05:00             │  1. timer window
│                             │
│   [ ▶ Start ]  [ ■ Stop ]   │  2. Start (turns into Pause) / Stop
│                             │
│   [ 0 ]   [ 5 ]   [ 0 ]     │  3. hours / minutes / seconds
│   часы    минуты  секунды   │
│  [1][2][3][4][5][6][7][8][9]│  4. quick buttons: set the minutes
│  [        10min        ]    │
│                             │
│ [ Часы:минуты:секунды ▾ ]   │  5. display format
│                             │
│ [ ...\obs-timer.txt ] [ 📂 ]│  6. file + folder/name picker
└─────────────────────────────┘
```

* **Timer window** — the countdown itself.
* **Start** starts the countdown and turns into **Pause**; **Stop** (square) stops it and
  restores the chosen duration, so the next Start begins from the full time again.
* **Hours / minutes / seconds** spin boxes, with quick buttons `1`–`9` and `10min` that set
  the minutes.
* **Format** — `Hours:minutes:seconds` or `Minutes:seconds`, the separator is always `:`.
  The same format is used in the panel and in the file.
* **File** — where the value goes, plus a folder/name picker button. The default is
  `obs-timer.txt` **inside the plugin folder**:
  `C:\ProgramData\obs-studio\plugins\obs-timer\obs-timer.txt`.

The file is rewritten exactly when the displayed second changes, so the overlay never lags
behind the panel. At `00:00:00` the countdown stops.

## Using it on stream

1. Set the duration and press **Start** once, so the file appears.
2. In OBS add a **Text (GDI+)** source, enable **Read from file** and point it at the same file.
3. Press **Start** when the countdown should run — the overlay follows the file.

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

**obs-timer** — док-панель с обратным отсчётом для OBS. Сверху вниз: окно таймера, кнопки
**Старт** (превращается в **Паузу**) и **Стоп** (квадратик, сбрасывает к заданной
длительности), поля часы/минуты/секунды с быстрыми кнопками `1`–`9` и `10мин`, выбор формата
(`Часы:минуты:секунды` или `Минуты:секунды`, разделитель всегда `:`) и внизу строка файла с
кнопкой выбора папки и имени.

Файл перезаписывается ровно в момент смены секунды на панели — по умолчанию
`C:\ProgramData\obs-studio\plugins\obs-timer\obs-timer.txt`, то есть в папке самого плагина.
Его можно подключить к источнику «Текст (GDI+)» с опцией «Читать из файла» и получить живой
отсчёт в эфире. На `00:00:00` таймер останавливается.

Установка: распакуйте папку `obs-timer` из zip в `C:\ProgramData\obs-studio\plugins\` и
перезапустите OBS. Панель — **Вид → Док-панели → Таймер**.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
