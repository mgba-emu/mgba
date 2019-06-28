medusa
======

medusa is an emulator for running Nintendo DS, Game Boy Advance and Game Boy games. It aims to be faster and more accurate than many existing Nintendo DS and Game Boy Advance emulators, as well as adding features that other emulators lack. It also supports Game Boy and Game Boy Color games.

Up-to-date news and downloads can be found at [mgba.io](https://mgba.io/).

[![Build status](https://travis-ci.org/mgba-emu/mgba.svg?branch=medusa)](https://travis-ci.org/mgba-emu/mgba)

Features
--------

- Near full Game Boy Advance hardware support[<sup>[1]</sup>](#missing).
- Partial DS hardware support[<sup>[1]</sup>](#missing).
- Game Boy/Game Boy Color hardware support.
- Fast emulation for Game Boy and Game Boy Advance. Known to run at full speed even on low end hardware, such as netbooks[<sup>[2]</sup>](#dscaveat).
- Qt and SDL ports for a heavy-weight and a light-weight frontend.
- Local (same computer) link cable support.
- Save type detection, even for flash memory size[<sup>[3]</sup>](#flashdetect).
- Support for cartridges with motion sensors and rumble (only usable with game controllers)[<sup>[2]</sup>](#dscaveat).
- Real-time clock support, even without configuration.
- Game Boy Camera and Game Boy Printer support.
- A built-in GBA BIOS implementation, and ability to load external BIOS files. DS currently requires BIOS and firmware dumps[<sup>[2]</sup>](#dscaveat).
- Turbo/fast-forward support by holding Tab.
- Rewind by holding Backquote.
- Frameskip, configurable up to 10.
- Screenshot support.
- Cheat code support[<sup>[2]</sup>](#dscaveat).
- 9 savestate slots. Savestates are also viewable as screenshots[<sup>[2]</sup>](#dscaveat).
- Video and GIF recording.
- Remappable controls for both keyboards and gamepads.
- Loading from ZIP and 7z files.
- IPS, UPS and BPS patch support.
- Game debugging via a command-line interface and GDB remote support, compatible with IDA Pro.
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
- OpenGL renderer.
- HLE support for DS BIOS and DS ARM7 processor.
- Synthesizing a customizable DS firmware to avoid needing a  dump.

Supported Platforms
-------------------

- Windows Vista or newer
- OS X 10.7 (Lion)[<sup>[4]</sup>](#osxver) or newer
- Linux
- FreeBSD

The following platforms are supported for everything except DS:

- Nintendo 3DS
- Wii
- PlayStation Vita

Other Unix-like platforms, such as OpenBSD, are known to work as well, but are untested and not fully supported.

### System requirements

Requirements are minimal[<sup>[2]</sup>](#dscaveat). Any computer that can run Windows Vista or newer should be able to handle emulation. Support for OpenGL 1.1 or newer is also required.

Downloads
---------

Downloads can be found on the official website, in the [Downloads][downloads] section. The source code can be found on [GitHub][source].

Controls
--------

Controls are configurable in the settings menu. Many game controllers should be automatically mapped by default. The default keyboard controls are as follows for GB and GBA:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Enter
- **Select**: Backspace

DS default controls are slightly different:

- **A**: X
- **B**: Z
- **X**: S
- **Y**: A
- **L**: Q
- **R**: W
- **Start**: Enter
- **Select**: Backspace

Compiling
---------

Compiling requires using CMake 2.8.11 or newer. GCC and Clang are both known to work to compile medusa, but Visual Studio 2013 and older are known not to work. Support for Visual Studio 2015 and newer is coming soon. To use CMake to build on a Unix-based system, the recommended commands are as follows:

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

This will build and install medusa into `/usr/bin` and `/usr/lib`. Dependencies that are installed will be automatically detected, and features that are disabled if the dependencies are not found will be shown after running the `cmake` command after warnings about being unable to find them.

If you are on macOS, the steps are a little different. Assuming you are using the homebrew package manager, the recommended commands to obtain the dependencies and build are:

	brew install cmake ffmpeg imagemagick libzip qt5 sdl2 libedit
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
	make

Note that you should not do a `make install` on macOS, as it will not work properly.

#### Windows developer building

To build on Windows for development, using MSYS2 is recommended. Follow the installation steps found on their [website](https://msys2.github.io). Make sure you're running the 32-bit version ("MSYS2 MinGW 32-bit") (or the 64-bit version "MSYS2 MinGW 64-bit" if you want to build for x86_64) and run this additional command (including the braces) to install the needed dependencies (please note that this involves downloading over 1100MiB of packages, so it will take a long time):

For x86 (32 bit) builds:

	pacman -Sy mingw-w64-i686-{cmake,ffmpeg,gcc,gdb,imagemagick,libzip,pkg-config,qt5,SDL2,ntldd-git}

For x86_64 (64 bit) builds:

	pacman -Sy mingw-w64-x86_64-{cmake,ffmpeg,gcc,gdb,imagemagick,libzip,pkg-config,qt5,SDL2,ntldd-git}

Check out the source code by running this command:

	git clone https://github.com/mgba-emu/mgba.git -b medusa medusa

Then finally build it by running these commands:

	cd medusa
	mkdir build
	cd build
	cmake .. -G "MSYS Makefiles"
	make

Please note that this build of medusa for Windows is not suitable for distribution, due to the scattering of DLLs it needs to run, but is perfect for development. However, if distributing such a build is desired (e.g. for testing on machines that don't have the MSYS2 environment installed), running `cpack -G ZIP` will prepare a zip file with all of the necessary DLLs.

### Dependencies

medusa has no hard dependencies, however, the following optional dependencies are required for specific features. The features will be disabled if the dependencies can't be found.

- Qt 5: for the GUI frontend. Qt Multimedia or SDL are required for audio.
- SDL: for a more basic frontend and gamepad support in the Qt frontend. SDL 2 is recommended, but 1.2 is supported.
- zlib and libpng: for screenshot support and savestate-in-PNG support.
- libedit: for command-line debugger support.
- ffmpeg or libav: for video recording.
- libzip or zlib: for loading ROMs stored in zip files.
- ImageMagick: for GIF recording.
- SQLite3: for game databases.
- libelf: for ELF loading.

SQLite3, libpng, and zlib are included with the emulator, so they do not need to be externally compiled first.

Footnotes
---------

<a name="missing">[1]</a> Currently missing features on GBA  are

- OBJ window for modes 3, 4 and 5 ([Bug #5](http://mgba.io/b/5))
- Mosaic for transformed OBJs ([Bug #9](http://mgba.io/b/9))

Missing features on DS are

- Audio:
	- Master audio settings
	- Sound output capture
	- Microphone
- Graphics:
	- Edge marking/wireframe
	- Highlight shading
	- Fog
	- Anti-aliasing
	- Alpha test
	- Position test
	- Vector test
	- Bitmap rear plane
	- Large bitmap mode 6
	- 1-dot depth clipping
	- Vector matrix memory mapping
	- Polygon/vertex RAM entry count memory mapping
	- Rendered line count memory mapping
	- Horizontal scrolling on 3D background
	- Some bitmap OBJ mappings
	- DMA FIFO backgrounds
- Other:
	- Cache emulation/estimation
	- Slot-2 access/RAM/rumble
	- BIOS protection
	- Display start DMAs
	- Most of Wi-Fi
	- RTC interrupts
	- Manual IPC sync IRQs
	- Lid switch
	- Power management
	- Touchscreen temperature/pressure support
	- Various MMIO registers
	- DSi cart protections

<a name="dscaveat">[2]</a> Many feature are still missing on the DS, including savestates, cheats, rumble, HLE BIOS, and more.

<a name="flashdetect">[3]</a> Flash memory size detection does not work in some cases. These can be configured at runtime, but filing a bug is recommended if such a case is encountered.

<a name="osxver">[3]</a> 10.7 is only needed for the Qt port. The SDL port is known to work on 10.5, and may work on older.

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

Copyright
---------

medusa is Copyright © 2013 – 2019 Jeffrey Pfau. It is distributed under the [Mozilla Public License version 2.0](https://www.mozilla.org/MPL/2.0/). A copy of the license is available in the distributed LICENSE file.

medusa contains the following third-party libraries:

- [inih](https://github.com/benhoyt/inih), which is copyright © 2009 Ben Hoyt and used under a BSD 3-clause license.
- [blip-buf](https://code.google.com/archive/p/blip-buf), which is copyright © 2003 – 2009 Shay Green and used under a Lesser GNU Public License.
- [LZMA SDK](http://www.7-zip.org/sdk.html), which is public domain.
- [MurmurHash3](https://github.com/aappleby/smhasher) implementation by Austin Appleby, which is public domain.
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/), which is public domain.
- [SQLite3](https://www.sqlite.org), which is public domain.

If you are a game publisher and wish to license medusa for commercial usage, please email [licensing@mgba.io](mailto:licensing@mgba.io) for more information.
