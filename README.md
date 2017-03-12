mGBA
====

mGBA is an emulator for running Game Boy Advance games. It aims to be faster and more accurate than many existing Game Boy Advance emulators, as well as adding features that other emulators lack. It also supports Game Boy and Game Boy Color games.

Up-to-date news and downloads can be found at [mgba.io](https://mgba.io/).

[![Build status](https://travis-ci.org/mgba-emu/mgba.svg?branch=master)](https://travis-ci.org/mgba-emu/mgba)

Features
--------

- Near full Game Boy Advance hardware support[<sup>[1]</sup>](#missing).
- Game Boy/Game Boy Color hardware support.
- Fast emulation. Known to run at full speed even on low end hardware, such as netbooks.
- Qt and SDL ports for a heavy-weight and a light-weight frontend.
- Local (same computer) link cable support.
- Save type detection, even for flash memory size[<sup>[2]</sup>](#flashdetect).
- Support for cartridges with motion sensors and rumble (only usable with game controllers).
- Real-time clock support, even without configuration.
- A built-in BIOS implementation, and ability to load external BIOS files.
- Turbo/fast-forward support by holding Tab.
- Rewind by holding Backquote.
- Frameskip, configurable up to 10.
- Screenshot support.
- Cheat code support.
- 9 savestate slots. Savestates are also viewable as screenshots.
- Video and GIF recording.
- Remappable controls for both keyboards and gamepads.
- Loading from ZIP and 7z files.
- IPS, UPS and BPS patch support.
- Game debugging via a command-line interface (not available with Qt port) and GDB remote support, compatible with IDA Pro.
- Configurable emulation rewinding.
- Support for loading and exporting GameShark and Action Replay snapshots.
- Cores available for RetroArch/Libretro and OpenEmu.
- Many, many smaller things.

### Planned features

- Networked multiplayer link cable support.
- Dolphin/JOY bus link cable support.
- M4A audio mixing, for higher quality sound than hardware.
- Re-recording support for tool-assist runs.
- Lua support for scripting.
- A comprehensive debug suite.
- e-Reader support.
- Wireless adapter support.
- Game Boy Printer support.

Supported Platforms
-------------------

- Windows Vista or newer
- OS X 10.7 (Lion)[<sup>[3]</sup>](#osxver) or newer
- Linux
- FreeBSD
- Nintendo 3DS
- Wii
- PlayStation Vita

Other Unix-like platforms, such as OpenBSD, are known to work as well, but are untested and not fully supported.

### System requirements

Requirements are minimal. Any computer that can run Windows Vista or newer should be able to handle emulation. Support for OpenGL 1.1 or newer is also required.

Downloads
---------

Downloads can be found on the official website, in the [Downloads][downloads] section. The source code can be found on [GitHub][source].

Controls
--------

Controls are configurable in the settings menu. Many game controllers should be automatically mapped by default. The default keyboard controls are as follows:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Enter
- **Select**: Backspace

Compiling
---------

Compiling requires using CMake 2.8.11 or newer. GCC and Clang are both known to work to compile mGBA, but Visual Studio 2013 and older are known not to work. Support for Visual Studio 2015 and newer is coming soon. To use CMake to build on a Unix-based system, the recommended commands are as follows:

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

This will build and install mGBA into `/usr/bin` and `/usr/lib`. Dependencies that are installed will be automatically detected, and features that are disabled if the dependencies are not found will be shown after running the `cmake` command after warnings about being unable to find them.

#### Windows developer building

To build on Windows for development, using MSYS2 is recommended. Follow the installation steps found on their [website](https://msys2.github.io). Make sure you're running the 32-bit version ("MSYS2 MinGW 32-bit") (or the 64-bit version "MSYS2 MinGW 64-bit" if you want to build for x86_64) and run this additional command (including the braces) to install the needed dependencies (please note that this involves downloading over 500MiB of packages, so it will take a long time):

For x86 (32 bit) builds:

	pacman -Sy mingw-w64-i686-{cmake,ffmpeg,gcc,gdb,imagemagick,libzip,pkg-config,qt5,SDL2}

For x86_64 (64 bit) builds:

	pacman -Sy mingw-w64-x86_64-{cmake,ffmpeg,gcc,gdb,imagemagick,libzip,pkg-config,qt5,SDL2}

Check out the source code by running this command:

	git clone https://github.com/mgba-emu/mgba.git

Then finally build it by running these commands:

	cd mgba
	mkdir build
	cd build
	cmake .. -G "MSYS Makefiles"
	make

Please note that this build of mGBA for Windows is not suitable for distribution, due to the scattering of DLLs it needs to run, but is perfect for development. However, if distributing such a build is desired (e.g. for testing on machines that don't have the MSYS2 environment installed), a tool called "[Dependency Walker](http://dependencywalker.com)" can be used to see which additional DLL files need to be shipped with the mGBA executable.

### Dependencies

mGBA has no hard dependencies, however, the following optional dependencies are required for specific features. The features will be disabled if the dependencies can't be found.

- Qt 5: for the GUI frontend. Qt Multimedia or SDL are required for audio.
- SDL: for a more basic frontend and gamepad support in the Qt frontend. SDL 2 is recommended, but 1.2 is supported.
- zlib and libpng: for screenshot support and savestate-in-PNG support.
- libedit: for command-line debugger support.
- ffmpeg or libav: for video recording.
- libzip or zlib: for loading ROMs stored in zip files.
- ImageMagick: for GIF recording.
- SQLite3: for game databases.

Both libpng and zlib are included with the emulator, so they do not need to be externally compiled first.

Footnotes
---------

<a name="missing">[1]</a> Currently missing features are

- OBJ window for modes 3, 4 and 5 ([Bug #5](http://mgba.io/b/5))
- Mosaic for transformed OBJs ([Bug #9](http://mgba.io/b/9))

<a name="flashdetect">[2]</a> Flash memory size detection does not work in some cases. These can be configured at runtime, but filing a bug is recommended if such a case is encountered.

<a name="osxver">[3]</a> 10.7 is only needed for the Qt port. The SDL port is known to work on 10.6, and may work on older.

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

Copyright
---------

mGBA is Copyright © 2013 – 2016 Jeffrey Pfau. It is distributed under the [Mozilla Public License version 2.0](https://www.mozilla.org/MPL/2.0/). A copy of the license is available in the distributed LICENSE file.

mGBA contains the following third-party libraries:

- [inih](https://github.com/benhoyt/inih), which is copyright © 2009 Ben Hoyt and used under a BSD 3-clause license.
- [blip-buf](https://code.google.com/archive/p/blip-buf), which is copyright © 2003 – 2009 Shay Green and used under a Lesser GNU Public License.
- [LZMA SDK](http://www.7-zip.org/sdk.html), which is public domain.
- [MurmurHash3](https://github.com/aappleby/smhasher) implementation by Austin Appleby, which is public domain.
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/), which is public domain.
- [SQLite3](https://www.sqlite.org), which is public domain.

If you are a game publisher and wish to license mGBA for commercial usage, please email [licensing@mgba.io](mailto:licensing@mgba.io) for more information.
