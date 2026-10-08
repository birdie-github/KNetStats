<h1>
  <img src="https://raw.githubusercontent.com/birdie-github/KNetStats/main/dist/hicolor/32x32/apps/com.birdie-github.KNetStats.png" alt="KNetStats Icon">
  KNetStats
</h1>

---

<p>A simple network interface and statistics viewer for Linux based on the <a href="https://knetstats.sourceforge.net/"> KDE 3 project of the same name</a> by Hugo Parente Lima (hugo_pl).</p>
<p>KNetStats is a simple Qt network interface and statistics viewer with an associated tray icon to monitor activity.<br>It displays and graphs data transmission information for easy visualization. It also displays information such as associated IPs and MAC addresses.</p>

## Screenshots:

![Statistics Window](https://raw.githubusercontent.com/birdie-github/KNetStats/main/.github/screenshots/StatisticsWindow.png)
![Configure Window](https://raw.githubusercontent.com/birdie-github/KNetStats/main/.github/screenshots/ConfigureWindow.png)

<p>
  Preview of tray icons with different themes:
  <img align="center" src="https://raw.githubusercontent.com/birdie-github/KNetStats/main/.github/screenshots/TrayIcons.png" alt="Tray Icons Preview">
</p>

## Installation:

If you wish to install a prebuilt release, please check the <a href=https://github.com/birdie-github/KNetStats/releases>Releases page</a>. An AUR package exists under the name <a href="https://aur.archlinux.org/packages/knetstats">knetstats</a>. \
Otherwise, building instructions are provided below:

### Prerequisites:

An RPM SPEC file (untested) and an Arch PKGBUILD is provided within `dist/`. You can use those to install KNetStats on the appropriate distros.

KNetStats supports Qt 5.11 or newer and Qt 6, using Qt Widgets and Qt Network. KDE Frameworks are not required. CMake prefers Qt6 when available and otherwise uses Qt5. The provided packaging definitions select Qt6 explicitly.

Manually building KNetStats requires Git and the following packages to be installed
 - Ubuntu: `cmake g++ qt6-base-dev` (Qt5: `qtbase5-dev`)
 - Fedora: `cmake gcc-c++ qt6-qtbase-devel` (Qt5: `qt5-qtbase-devel`)
 - Arch Linux: `cmake qt6-base` (Qt5: `qt5-base`)

#### Procedure:

```bash
# Clone and enter this repository
$ git clone https://github.com/birdie-github/KNetStats && cd KNetStats
 
# Configure and build the program
$ cmake -B build
$ cmake --build build
 
# At this point, you can run KNetStats by executing the binary located at
# build/knetstats
 
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

---

## Notes / Caveats:

 - This project is new and a lot of changes had to be made to make it compatible with Qt 5
   - New features have been added, and I'm not the best programmer, so I expect there will be a couple bugs here and there
 - For the time being only Linux is supported
 - The original translations have not yet been incorporated, though the files remain in this repository
 - Any contributions would be greatly appreciated

## Credits:

 - Hugo Parente Lima for the original KNetStats project that this is based on, without it this would not exist
 - [James](https://github.com/telans) for porting the app to KDE5.
 - @birdie-github for thorough testing during development, and for kindly sponsoring that work
 - All the original KNetStats contributors
