# obs-timer

A small **dock panel timer** for [OBS Studio](https://obsproject.com) that continuously
writes its current value into a plain text file.

Typical use case: drop the produced `obs-timer.txt` into a **Text (GDI+)** source and you get
a live stream/recording duration overlay without any script or third-party service. The file
is updated while the timer runs, so the overlay follows the clock.

Built and tested against **OBS Studio 33.0.1** on Windows x64.

---

## Features

* Large monospace `HH:MM:SS` display, optional milliseconds.
* Start / Pause / Reset / Save-now buttons.
* Output file is configurable (`Documents/obs-timer.txt` by default).
* Two write modes:
  * **overwrite** – the file always contains the current value (best for text sources);
  * **append** – a `YYYY-MM-DD HH:mm:ss<TAB>HH:MM:SS` line is appended when the timer stops
    or when *Save now* is pressed (best for keeping a log of sessions).
* Configurable write interval (10×/s … once per 10 s, or only when stopped).
* Automatic start/stop together with streaming or recording.
* Optional reset when the timer starts.
* Frontend hotkeys (bind them in *Settings → Hotkeys*):
  * *Start or pause the timer*,
  * *Reset the timer*,
  * *Save the timer value now*.
* Settings are stored in the plugin config folder and survive restarts.
* Localised UI: English and Russian.

## Installation

OBS Studio 33 changed where third-party plugins live. This plugin uses the new layout:

```
C:\ProgramData\obs-studio\plugins\obs-timer\obs-timer.dll
C:\ProgramData\obs-studio\plugins\obs-timer\data\locale\...
```

1. Download `obs-timer-<version>-windows-x64.zip` from the releases page.
2. Extract the `obs-timer` folder into `C:\ProgramData\obs-studio\plugins\`.
3. Restart OBS Studio.

The dock is then available in **View → Docks → Timer** (it starts floating the first time so it
is easy to spot).

> Plugins installed to the legacy `C:\Program Files\obs-studio\obs-plugins\64bit` or to
> `C:\ProgramData\obs-studio\plugins\<name>\bin\64bit` locations are still loaded by OBS 33,
> but are flagged as *Legacy* in the plugin manager and will stop working in OBS 34.
> Portable OBS installations use `<obs-folder>\plugins\obs-timer\` instead.

## Usage

1. Open the dock and pick the output file (the default location is fine for a quick test).
2. Choose the write interval. For a text-source overlay *Once per second* is a good default.
3. Press **Start**. The file gets the current timer value.
4. Add a *Text (GDI+)* source, enable **Read from file** and point it to the same file.
5. Optionally enable *Start with streaming or recording* so the timer runs only while you
   are live.

## Building from source

Requirements: Windows 10/11, Visual Studio 2022 (MSVC, Windows SDK 10.0.22621), CMake 3.28+
and PowerShell 7. Dependencies (OBS Studio 33.0.1 sources, pre-built OBS deps and Qt 6) are
downloaded automatically into `.deps/` during the CMake configure step.

```powershell
# Configure + build (downloads OBS sources/deps on the first run, builds libobs)
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo --parallel

# Stage the plugin into release/RelWithDebInfo/obs-timer/
cmake --install build_x64 --prefix release/RelWithDebInfo --config RelWithDebInfo
```

The first configure takes a while: it builds `libobs` and `obs-frontend-api` from the OBS
sources so the plugin links against the exact 33.0.1 ABI.

CI (`.github/workflows/build.yaml`) runs the same steps on `windows-2022` and publishes a
zip with the `C:\ProgramData\obs-studio\plugins` layout.

## Notes

* Only Windows x64 builds are currently supported and tested.
* The plugin does not create any sources, filters or outputs; it is purely a frontend dock.
* Writing to a file 10 times per second is a lot of I/O for an overlay – prefer 1 s unless
  you really need sub-second accuracy.

## Русский

**obs-timer** — док-панель с таймером для OBS Studio, которая записывает текущее значение
в обычный текстовый файл. Удобно для оверлея длительности стрима: подключаете
`obs-timer.txt` в источник «Текст (GDI+)» с опцией «Читать из файла».

* Установка: распакуйте папку `obs-timer` из релизного zip в `C:\ProgramData\obs-studio\plugins\`
  и перезапустите OBS.
* Панель открывается через **Вид → Док-панели → Таймер**.
* Интервал записи, режим перезаписи/дописывания, автостарт вместе с трансляцией,
  сброс при старте и горячие клавиши настраиваются прямо в панели и в *Настройки → Горячие клавиши*.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
