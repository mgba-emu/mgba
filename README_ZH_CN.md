mGBA
====

mGBA 是一个运行 Game Boy Advance 游戏的模拟器。mGBA 的目标是比众多现有的 Game Boy Advance 模拟器更快、更准确，并增加其他模拟器所缺少的功能。mGBA 还支持 Game Boy 和 Game Boy Color 游戏。

可在以下网址找到最新新闻和下载：[mgba.io](https://mgba.io/)。

[![Build status](https://travis-ci.org/mgba-emu/mgba.svg?branch=master)](https://travis-ci.org/mgba-emu/mgba)

功能
--------

- 支持高精确的 Game Boy Advance 硬件[<sup>[1]</sup>](#missing)。
- 支持 Game Boy/Game Boy Color 硬件。
- 快速模拟：已知即使在低端硬件（例如上网本）上也能够全速运行。
- 用于重型和轻型前端的 Qt 和 SDL 端口。
- 支持本地（同一台计算机）链接电缆。
- 存档类型检测，即使是闪存大小也可检测[<sup>[2]</sup>](#flashdetect)。
- 支持附带有运动传感器和振动机制的卡带（仅适用于游戏控制器）。
- 支持实时时钟（RTC），甚至无需配置。
- 支持《我们的太阳》系列游戏的太阳能传感器。
- 支持 Game Boy 相机和 Game Boy 打印机。
- 内置 BIOS 执行，并具有加载外部 BIOS 文件的功能。
- 支持 Turbo/快进功能（按住 Tab 键）。
- 支持倒带（按住反引号键）。
- 支持跳帧，最多可配置 10 级。
- 支持截图。
- 支持作弊码。
- 支持 9 个即时存档插槽。还能够以屏幕截图的形式查看即时存档。
- 支持视频、GIF、WebP 和 APNG 录制。
- 支持 e-Reader。
- 可重新映射键盘和游戏手柄的控制键。
- 支持从 ZIP 和 7z 文件中加载。
- 支持 IPS、UPS 和 BPS 补丁。
- 支持通过命令行界面和 GDB 远程支持进行游戏调试，兼容 IDA Pro。
- 支持可配置的模拟倒带。
- 支持载入和导出 GameShark 和 Action Replay 快照。
- 适用于 RetroArch/Libretro 和 OpenEmu 的内核。
- 许许多多的小玩意。

#### Game Boy 映射器（mapper）

完美支持以下 mapper：

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC
- MBC5
- MBC5+振动
- MBC7
- Wisdom Tree（未授权）
- Pokémon Jade/Diamond（未授权）
- BBD（未授权、类 MBC5）
- Hitek（未授权、类 MBC5）

部分支持以下 mapper：

- MBC6（缺少闪存写入支持）
- MMM01
- Pocket Cam
- TAMA5（缺少 RTC 支持）
- HuC-1（缺少 IR 支持）
- HuC-3（缺少 IR 和 RTC 支持）

### 计划加入的功能

- 支持联网多人链接电缆。
- 支持 Dolphin/JOY 总线链接电缆。
- MP2k 音频混合，获得比硬件更高质量的声音。
- 支持针对工具辅助竞速（Tool-Assisted Speedrun）的重录功能。
- 支持 Lua 脚本。
- 全方位的调试套件。
- 支持无线适配器。

支持平台
-------------------

- Windows 7 或更新
- OS X 10.9（Mavericks）[<sup>[3]</sup>](#osxver) 或更新
- Linux
- FreeBSD
- Nintendo 3DS
- Nintendo Switch
- Wii
- PlayStation Vita

已知其他类 Unix 平台（如 OpenBSD）也可以使用，但未经测试且不完全受支持。

### 系统需求

系统需求很低。任何可以运行 Windows Vista 或更高版本的计算机都应该能够处理模拟机制，还需要支持 OpenGL 1.1 或更高版本。而对于着色器和高级功能，则需要支持 OpenGL 3.2 或更高版本。

下载
---------

可在官方网站的[下载（Downloads）][downloads]区域找到下载地址。可在 [GitHub][source] 找到源代码。

控制键位
--------

可在设置菜单中进行控制键位的配置。许多游戏控制器应该会在默认情况下自动映射。键盘的默认控制键位如下：

- **A**：X
- **B**：Z
- **L**：A
- **R**：S
- **Start**：回车键
- **Select**：退格键

编译
---------

编译需要使用 CMake 3.1 或更新版本。已知 GCC 和 Clang 都可以编译 mGBA，而 Visual Studio 2013 和更旧的版本则无法编译。我们即将实现对 Visual Studio 2015 或更新版本的支持。

#### Docker 构建

对于大多数平台来说，建议使用 Docker 进行构建。我们提供了多个 Docker 映像，其中包含在多个平台上构建 mGBA 所需的工具链和依赖项。

要使用 Docker 映像构建 mGBA，只需在 mGBA 的签出（checkout）根目录中运行以下命令：

	docker run --rm -t -v $PWD:/home/mgba/src mgba/windows:w32

此命令将生成 `build-win32` 目录。将 `mgba/windows:w32` 替换为其他平台上的 Docker 映像，会生成相应的其他目录。Docker Hub 上提供了以下 Docker 映像：

- mgba/3ds
- mgba/switch
- mgba/ubuntu:xenial
- mgba/ubuntu:bionic
- mgba/ubuntu:focal
- mgba/ubuntu:groovy
- mgba/vita
- mgba/wii
- mgba/windows:w32
- mgba/windows:w64

#### *nix 构建

要在基于 Unix 的系统上使用 CMake 进行构建，推荐执行以下命令：

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

这些命令将构建 mGBA 并将其安装到 `/usr/bin` 和 `/usr/lib` 中。系统会自动检测已安装的依赖项，如果未找到依赖项，则会在提示找不到依赖项的情况下运行 `cmake` 命令，并显示已被禁用的功能。

如果您使用的是 MacOS，则步骤略有不同。假设您使用的是自制软件包管理器，建议使用以下命令来获取依赖项并进行构建：

	brew install cmake ffmpeg libzip qt5 sdl2 libedit pkg-config
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
	make

请注意，您不能在 MacOS 上执行 `make install`，因为此命令不能正常工作。

#### Windows 开发者构建

##### MSYS2

如果要在 Windows 上进行构建，建议使用 MSYS2。请按照 MSYS2 [网站](https://msys2.github.io)上的安装步骤操作。请确保您运行的是 32 位版本的 MSYS2（“MSYS2 MinGW 32-bit”）。如果想要构建 x86_64 版本，则运行 64 位版本的 MSYS2（“MSYS2 MinGW 64-bit”） ，并执行以下额外命令（包括花括号）来安装所需的依赖项（请注意，此命令涉及下载超过 1100MiB 的包，因此会需要很长一段时间）：

	pacman -Sy --needed base-devel git ${MINGW_PACKAGE_PREFIX}-{cmake,ffmpeg,gcc,gdb,libelf,libepoxy,libzip,pkgconf,qt5,SDL2,ntldd-git}

运行以下命令检查源代码：

	git clone https://github.com/mgba-emu/mgba.git

最后运行以下命令进行构建：

	mkdir -p mgba/build
	cd mgba/build
	cmake .. -G "MSYS Makefiles"
	make -j$(nproc --ignore=1)

请注意，此版本的 mGBA for Windows 不适合分发，因为运行此版本所需的 DLL 非常分散，但非常适合开发。但是，如果需要分发此类版本（例如用于在未安装 MSYS2 环境的计算机上进行测试），请运行 `cpack-G ZIP`，准备一个包含所有必要 DLL 的压缩文件。

##### Visual Studio

使用 Visual Studio 进行构建需要同样复杂的设置。首先需要安装 [vcpkg](https://github.com/Microsoft/vcpkg)。安装 vcpkg 后，还需要安装数个额外的软件包：

    vcpkg install ffmpeg[vpx,x264] libepoxy libpng libzip sdl2 sqlite3

请注意，此安装将不支持 Nvidia 硬件上的硬件加速视频编码。如果对此非常在意，则需要预先安装 CUDA，然后用 `ffmpeg[vpx,x264,nvcodec]` 替换前面命令中的 `ffmpeg[vpx,x264]`。

您还需要安装 Qt。但不幸的是，由于 Qt 已被一家境况不佳的公司而不是合理的组织所拥有并运营，所以不再存在针对最新版本的离线开源版本安装程序，需要退回到[旧版本的安装程序](https://download.qt.io/official_releases/qt/5.12/5.12.9/qt-opensource-windows-x86-5.12.9.exe) （会要求创建一个原本已无用的帐号，但可以通过临时设置无效代理或以其他方式禁用网络来绕过这一机制。）、使用在线安装程序（无论如何都需要一个帐号），或使用 vcpkg 进行构建（速度很慢）。这些都不是很好的选择。需要针对安装程序安装适用的 MSVC 版本。请注意，离线安装程序不支持 MSVC 2019。若使用 vcpkg，您需要花费相当一段时间将其安装，尤其是在四核或更少内核的计算机上花费时间更久：

    vcpkg install qt5-base qt5-multimedia

下一步打开 Visual Studio，选择“克隆仓库”, 输入 `https://github.com/mgba-emu/mgba.git`。在 Visual Studio 完成克隆后，转到“文件”>“CMake”，然后打开已签出（checked out）仓库的 CMakeLists.txt 文件。在此基础上便可像其他 Visual Studio CMake 项目一样在 Visual Studio 中开发 mGBA。

#### 工具链构建

如果您拥有 devkitARM（3DS）、devkitPPC（Wii）、devkitA64（Switch）或 vitasdk（PS Vita），您可以使用以下命令进行构建：

	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../src/platform/3ds/CMakeToolchain.txt ..
	make

将 `-DCMAKE_TOOLCHAIN_FILE` 参数替换为以下不同平台的参数：

- 3DS：`../src/platform/3ds/CMakeToolchain.txt`
- Switch：`../src/platform/switch/CMakeToolchain.txt`
- Vita：`../src/platform/psp2/CMakeToolchain.vitasdk`
- Wii：`../src/platform/wii/CMakeToolchain.txt`

### 依赖项

mGBA 没有硬性的依赖项，但是特定功能需要以下可选的依赖项。如果找不到依赖项，则这些可选功能将会被禁用。

- Qt 5：GUI 前端的所需依赖项。音频需要 Qt Multimedia 或 SDL。
- SDL：更基本的前端以及在 Qt 前端中支持游戏手柄的所需依赖项。推荐使用 SDL 2、但也支持 1.2。
- zlib 和 libpng：截图与 PNG 即时存档支持的所需依赖项
- libedit：命令行调试器的所需依赖项
- ffmpeg 或 libav：录制视频、GIF、WebP 和 APNG 的所需依赖项
- libzip 或 zlib：载入储存在 ZIP 文件中的 ROM 的所需依赖项。
- SQLite3：游戏数据库的所需依赖项
- libelf：ELF 载入的所需依赖项

SQLite3、libpng 以及 zlib 已包含在模拟器中，因此不需要先对这些依赖项进行外部编译。

Footnotes
---------

<a name="missing">[1]</a> 目前缺失的功能有

- 模式 3、4 和 5 的 OBJ 窗口 ([Bug #5](http://mgba.io/b/5))

<a name="flashdetect">[2]</a> 闪存大小检测在某些情况下不起作用。 这些可以在运行时中进行配置，但如果遇到此类情况，建议提交错误。

<a name="osxver">[3]</a> 仅 Qt 端口需要 10.9。应该可以在 10.7 或更早版本上构建或运行 Qt 端口，但这类操作不受官方支持。已知 SDL 端口可以在 10.5 上运行，并且可能能够在旧版本上运行。

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

版权
---------

mGBA 版权 © 2013 – 2020 Jeffrey Pfau。基于 [Mozilla 公共许可证版本 2.0](https://www.mozilla.org/MPL/2.0/) 许可证分发。分发的 LICENSE 文件中提供了许可证的副本。

mGBA 包含以下第三方库：

- [inih](https://github.com/benhoyt/inih)：版权 © 2009 – 2020 Ben Hoyt，基于 BSD 3-clause 许可证使用。
- [blip-buf](https://code.google.com/archive/p/blip-buf)：版权 © 2003 – 2009 Shay Green，基于 Lesser GNU 公共许可证使用。
- [LZMA SDK](http://www.7-zip.org/sdk.html)：属公有领域使用。
- [MurmurHash3](https://github.com/aappleby/smhasher)：由 Austin Appleby 实施，属公有领域使用。
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/)：属公有领域使用。
- [SQLite3](https://www.sqlite.org)：属公有领域使用。

如果您是游戏发行商，并希望获得 mGBA 用于商业用途的许可，请发送电子邮件到 [licensing@mgba.io](mailto:licensing@mgba.io) 获取更多信息。
