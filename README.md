<h1>
  <img src="https://raw.githubusercontent.com/birdie-github/QNetStats/main/dist/hicolor/32x32/apps/com.birdie-github.QNetStats.png" alt="QNetStats Icon">
  QNetStats
</h1>

---

<p>A simple network interface and statistics viewer for Linux and Windows 10/11 based on the <a href="https://knetstats.sourceforge.net/"> KDE 3 project KNetStats</a> by Hugo Parente Lima (hugo_pl).</p>
<p>QNetStats is a simple Qt network interface and statistics viewer with an associated tray icon to monitor activity.<br>It displays and graphs data transmission information for easy visualization. It also displays information such as associated IPs and MAC addresses.</p>

## Compact chart mode

Double-click the traffic chart to show only the chart, its current maximum-speed
scale at the top left, and the interface name at the top right. The compact window
has no title bar or border and stays on top. Hold the left mouse button and drag
the chart to move it.

Double-click again or press Escape to restore the normal window and its previous
geometry. Resize the normal window before entering compact mode to choose the
chart size. Tray hide/show preserves the current mode; compact mode and its
position are not saved between app sessions. Window positioning and always-on-top
behavior depend on the window manager. Native dragging uses Qt 5.15 or newer;
older Qt versions use manual positioning, which may be restricted on Wayland.

## Screenshots:

![Statistics Window](https://raw.githubusercontent.com/birdie-github/QNetStats/main/.github/screenshots/StatisticsWindow.png)
![Configure Window](https://raw.githubusercontent.com/birdie-github/QNetStats/main/.github/screenshots/ConfigureWindow.png)

<p>
  Preview of tray icons with different themes:
  <img align="center" src="https://raw.githubusercontent.com/birdie-github/QNetStats/main/.github/screenshots/TrayIcons.png" alt="Tray Icons Preview">
</p>

## Installation:

If you wish to install a prebuilt release, please check the <a href=https://github.com/birdie-github/QNetStats/releases>Releases page</a>. \
Otherwise, building instructions are provided below:

### Prerequisites:

An RPM SPEC file (untested) and an Arch PKGBUILD is provided within `dist/`. You can use those to install QNetStats on the appropriate distros.

QNetStats supports Qt 5.11 or newer and Qt 6 on Linux and Windows 10/11, using Qt Widgets and Qt Network. KDE Frameworks are not required.
CMake prefers Qt6 when available and otherwise uses Qt5. The provided packaging definitions select Qt6 explicitly.

Manually building QNetStats requires Git and the following packages to be installed
 - Ubuntu: `cmake g++ qt6-base-dev` (Qt5: `qtbase5-dev`)
 - Fedora: `cmake gcc-c++ qt6-qtbase-devel` (Qt5: `qt5-qtbase-devel`)
 - Arch Linux: `cmake qt6-base` (Qt5: `qt5-base`)

#### Procedure:

```bash
# Clone and enter this repository
$ git clone https://github.com/birdie-github/QNetStats && cd QNetStats
 
# Configure and build the program
$ cmake -B build
$ cmake --build build
 
# At this point, you can run QNetStats by executing the binary located at
# build/qnetstats
 
# Install the program to /usr/local (default)
$ sudo cmake --install build
```

To choose a Qt version explicitly, configure a separate build directory:

```bash
cmake -S . -B build-qt5 -DUSE_QT=5
cmake --build build-qt5

cmake -S . -B build-qt6 -DUSE_QT=6
cmake --build build-qt6
```

The default is `-DUSE_QT=AUTO`. An explicitly selected version must be installed; CMake will not fall back to another version.

### Windows 10 and 11

Install CMake, a matching Qt 5.11+ or Qt 6 desktop kit, and its compiler. For example, use
an MSVC 2022 x64 Qt kit with Visual Studio 2022 Build Tools, the C++ workload,
and the Windows SDK. Only Qt Core, Gui, Widgets, and Network are needed.

For Qt 6, from an x64 Visual Studio developer command prompt, substituting your
Qt kit path:

```bat
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DUSE_QT=6 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build-win --config Release
"C:/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe" --release --no-translations build-win/Release/qnetstats.exe
build-win\Release\qnetstats.exe
```

For Qt 5, use `-DUSE_QT=5`, point `CMAKE_PREFIX_PATH` at the Qt 5 kit, and use
that kit's compiler and `windeployqt`. The `AUTO` selection works on Windows too.

The compiler must match the Qt kit; an MSVC Qt kit cannot be used with MinGW.
`windeployqt` copies the Qt libraries and plugins needed to run the executable.
The Windows CI workflow builds an x64 Qt6 package on pushes and pull requests
to `main`, and can also be run manually. Download the
`QNetStats-Windows-x64-Qt6-<commit>` artifact from the workflow run, extract the
entire archive, and run `qnetstats.exe`. Keep the DLLs and plugin folders beside
the executable. Artifacts are retained for 14 days.

On Windows, traffic counters and connection status come from the system IP
Helper API. Adapter names shown in the UI are friendly names such as Wi-Fi or
Ethernet; settings use the internal adapter identifier. QSettings stores these
settings under `HKEY_CURRENT_USER\Software\QNetStats\QNetStats` by default.

---

## Notes / Caveats:

 - The supported platforms are Linux and Windows 10/11.
 - Without a system tray, a fallback window provides access to statistics, configuration, and Quit. Closing that window exits the app while no tray is available.
 - The original translations have not yet been incorporated, though the files remain in this repository

## Credits:

 - Hugo Parente Lima for the original KNetStats project that this is based on, without it this would not exist
 - [James](https://github.com/telans) for porting the app to KDE5.
 - @birdie-github for thorough testing during development, and for kindly sponsoring that work
 - All the original KNetStats contributors
