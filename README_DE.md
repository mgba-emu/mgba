mGBA
====

mGBA ist ein Emulator für Game Boy Advance-Spiele. Das Ziel von mGBA ist, schneller und genauer als viele existierende Game Boy Advance-Emulatoren zu sein. Außerdem verfügt mGBA über Funktionen, die anderen Emulatoren fehlen. Zusätzlich werden auch Game Boy- und Game Boy Color-Spiele unterstützt.

Aktuelle Neuigkeiten und Downloads findest Du auf [mgba.io](https://mgba.io).

[![Build-Status](https://travis-ci.org/mgba-emu/mgba.svg?branch=master)](https://travis-ci.org/mgba-emu/mgba)

Features
--------

- Sehr genaue Unterstützung der Game Boy Advance-Hardware[<sup>[1]</sup>](#missing).
- Unterstützung der Game Boy-/Game Boy Color-Hardware.
- Schnelle Emulation. mGBA ist dafür bekannt, auch auf schwacher Hardware wie Netbooks mit voller Geschwindigkeit zu laufen.
- Qt- und SDL-Portierungen für eine vollwertige und eine "leichtgewichtige" Benutzeroberfläche.
- Lokale (gleicher Computer) Unterstützung für Link-Kabel.
- Erkennung des Speichertypes, einschließlich der Größe des Flash-Speichers[<sup>[2]</sup>](#flashdetect).
- Unterstützung für Spielmodule mit Bewegungssensoren und Rüttel-Effekten (nur verwendbar mit Spiele-Controllern).
- Unterstützung für Echtzeituhren, selbst ohne Konfiguration.
- Unterstützung für den Lichtsensor in Boktai-Spielen
- Unterstützung für Game Boy Printer und Game Boy Camera.
- Eingebaute BIOS-Implementierung mit der Möglichkeit, externe BIOS-Dateien zu laden.
- Turbo/Vorlauf-Unterstützung durch drücken der Tab-Taste.
- Rücklauf-Unterstützung durch drücken der Akzent-Taste.
- Frameskip von bis zu 10 Bildern.
- Unterstützung für Screenshots.
- Unterstützung für Cheat-Codes.
- 9 Speicherstände für Savestates/Spielzustände. Savestates können auch als Screenshots dargestellt werden.
- Video- und GIF-Aufzeichnung.
- Frei wählbare Tastenbelegungen für Tastaturen und Controller.
- Unterstützung für ZIP- und 7z-Archive.
- Unterstützung für Patches im IPS-, UPS- und BPS-Format.
- Spiele-Debugging über ein Kommandozeilen-Interface und IDA Pro-kompatible GDB-Unterstützung.
- Einstellbare Rücklauf-Funktion.
- Unterstützung für das Laden und Exportieren von GameShark- und Action Replay-Abbildern.
- Verfügbare Cores für RetroArch/Libretro und OpenEmu.
- Viele, viele kleinere Dinge.

### Game Boy-Mapper

Die folgenden Mapper werden vollständig unterstützt:

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC (MBC3+Echtzeituhr)
- MBC5
- MBC5+Rumble (MBC5+Rüttel-Modul)
- MBC7

Die folgenden Mapper werden teilweise unterstützt:

- Pocket Cam
- TAMA5
- HuC-3

Die folgenden Mapper werden derzeit nicht unterstützt:

- MBC6
- HuC-1
- MMM01

### Geplante Features

- Unterstützung für Link-Kabel-Multiplayer über ein Netzwerk.
- Unterstützung für Link-Kabel über Dolphin/JOY-Bus.
- M4A-Audio-Abmischung für höhere Audio-Qualität.
- Unterstützung für Tool-Assisted Speedruns.
- Lua-Unterstützung für Scripting.
- Eine umfangreiche Debugging-Suite.
- e-Reader-Unterstützung.
- Unterstützung für Drahtlosadapter.

Unterstützte Plattformen
------------------------

- Windows Vista oder neuer
- OS X 10.7 (Lion)[<sup>[3]</sup>](#osxver) oder neuer
- Linux
- FreeBSD
- Nintendo 3DS
- Wii
- PlayStation Vita

Andere Unix-ähnliche Plattformen wie OpenBSD sind ebenfalls dafür bekannt, mit mGBA kompatibel zu sein. Sie sind jedoch nicht getestet und werden nicht voll unterstützt.

### Systemvoraussetzungen

Die Systemvoraussetzungen sind minimal. Jeder Computer, der mit Windows Vista oder neuer läuft, sollte in der Lage sein, die Emulation zu bewältigen. Unterstützung für OpenGL 1.1 oder neuer ist ebenfalls voraussgesetzt.

Downloads
---------

Download-Links befinden sich in der [Downloads][downloads]-Sektion auf der offizielle Website. Der Quellcode befindet sich auf [GitHub][source].

Steuerung
---------

Die Steuerung kann im Einstellungs-Menü konfiguriert werden. Viele Spiele-Controller werden automatisch erkannt und entsprechend belegt. Für Tastaturen wird standardmäßig folgende Belegung verwendet:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Enter
- **Select**: Rücktaste

Kompilieren
-----------

Um mGBA kompilieren zu können, wird CMake 2.8.11 oder neuer benötigt. GCC und Clang sind beide dafür bekannt, mGBA kompilieren zu können. Visual Studio 2013 und älter funktionieren nicht. Unterstützung für Visual Studio 2015 und neuer wird bald hinzugefügt. Um CMake auf einem Unix-basierten System zu verwenden, werden folgende Kommandos empfohlen:

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

Damit wird mGBA gebaut und in `/usr/bin` und `/usr/lib` installiert. Installierte Abhängigkeiten werden automatisch erkannt. Features, die aufgrund fehlender Abhängigkeiten deaktiviert werden, werden nach dem `cmake`-Kommando aufgelistet.

Wenn Du macOS verwendest, sind die einzelnen Schritte etwas anders. Angenommen, dass Du eine Homebrew-Paketverwaltung verwendest, werden folgende Schritte zum installieren der Abhängigkeiten und anschließenden bauen von mGBA empfohlen:

	brew install cmake ffmpeg imagemagick libzip qt5 sdl2 libedit
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH='brew --prefix qt5' ..
	make

Bitte beachte, dass Du unter macOS nicht 'make install' verwenden solltest, da dies nicht korrekt funktionieren wird.

### Für Entwickler: Kompilieren unter Windows

Um mGBA auf Windows zu kompilieren, wird MSYS2 empfohlen. Befolge die Installationsschritte auf der [MSYS2-Website](https://msys2.github.io). Stelle sicher, dass Du die 32-Bit-Version ("MSYS2 MinGW 32-bit") (oder die 64-Bit-Version "MSYS2 MinGW 64-bit", wenn Du mGBA für x86_64 kompilieren willst) verwendest und führe folgendes Kommando (einschließlich der Klammern) aus, um alle benötigten Abhängigkeiten zu installieren. Bitte beachte, dass dafür über 1100MiB an Paketen heruntergeladen werden, was eine Weile dauern kann:

Für x86 (32 Bit):

	pacman -Sy mingw-w64-i686-{cmake,ffmpeg,gcc,gdb,imagemagick,libelf,libepoxy,libzip,pkg-config,qt5,SDL2,ntldd-git}

Für x86_64 (64 Bit):

	pacman -Sy mingw-w64-x86_64-{cmake,ffmpeg,gcc,gdb,imagemagick,libelf,libepoxy,libzip,pkg-config,qt5,SDL2,ntldd-git}

Lade den aktuellen mGBA-Quellcode mithilfe des folgenden Kommandos herunter:

	git clone https://github.com/mgba-emu/mgba.git

Abschließend wird mGBA über folgende Kommandos kompiliert:

	cd mgba
	mkdir build
	cd build
	cmake .. -G "MSYS Makefiles"
	make

Bitte beachte, dass mGBA für Windows aufgrund der Vielzahl an benötigten DLLs nicht für die weitere Verteilung geeignet ist, wenn es auf diese Weise gebaut wurde. Es ist jedoch perfekt für Entwickler geeignet. Soll mGBA dennoch weiter verteilt werden (beispielsweise zu Testzwecken auf Systemen, auf denen keine MSYS2-Umgebung installiert ist), kann mithilfe des Befehls 'cpack -G ZIP' ein ZIP-Archiv mit allen benötigten DLLs erstellt werden.

### Abhängigkeiten

mGBA hat keine "harten" Abhängigkeiten. Dennoch werden die folgenden optionalen Abhängigkeiten für einige Features benötigt. Diese Features werden automatisch deaktiviert, wenn die benötigten Abhängigkeiten nicht gefunden werden.

- Qt 5: Für die Benutzeroberfläche. Qt Multimedia oder SDL werden für Audio-Ausgabe benötigt.
- SDL: Für eine einfachere Benutzeroberfläche und Spiele-Controller-Unterstützung in der Qt-Oberfläche. SDL 2 ist empfohlen, SDL 1.2 wird jedoch auch unterstützt.
- zlib und libpng: Für die Unterstützung von Bildschirmfotos und Savestates-in-PNG-Unterstützung.
- libedit: Für die Unterstützung des Kommandozeilen-Debuggers.
- ffmpeg oder libav: Für Videoaufzeichnungen.
- libzip oder zlib: Um ROMs aus ZIP-Dateien zu laden.
- ImageMagick: Für GIF-Aufzeichnungen.
- SQLite3: Für Spiele-Datenbanken.
- libelf: Für das Laden von ELF-Dateien.

SQLite3, libpng und zlib werden mit dem Emulator mitgeliefert, sodass sie nicht zuerst kompiliert werden müssen.

Fußnoten
--------

<a name="missing">[1]</a> Zurzeit fehlende Features sind

- OBJ-Fenster für die Modi 3, 4 und 5 ([Bug #5](http://mgba.io/b/5))
- Mosaik-Effekt für umgewandelte OBJs ([Bug #9](http://mgba.io/b/9))

<a name="flashdetect">[2]</a> In manchen Fällen ist es nicht möglich, die Größe des Flash-Speichers automatisch zu ermitteln. Diese kann dann zur Laufzeit konfiguriert werden, es wird jedoch empfohlen, den Fehler zu melden.

<a name="osxver">[3]</a> 10.7 wird nur für die Qt-Portierung benötigt. Die SDL-Portierung ist dafür bekannt, mit 10.5 und möglicherweise auf älteren Versionen zu funktionieren.

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

Copyright
---------

Copyright für mGBA © 2013 – 2018 Jeffrey Pfau. mGBA wird unter der [Mozilla Public License version 2.0](https://www.mozilla.org/MPL/2.0/) veröffentlicht. Eine Kopie der Lizenz ist in der mitgelieferten Datei LICENSE verfügbar.

mGBA beinhaltet die folgenden Bibliotheken von Drittanbietern:

- [inih](https://github.com/benhoyt/inih), Copyright © 2009 Ben Hoyt, verwendet unter einer BSD 3-clause-Lizenz.
- [blip-buf](https://code.google.com/archive/b/blip-buf), Copyright © 2003 - 2009 Shay Green, verwendet unter einer Lesser GNU Public License.
- [LZMA SDK](http://www.7-zip.org/sdk.html), Public Domain.
- [MurmurHash3](https://github.com/aappleby/smhasher), Implementierung von Austin Appleby, Public Domain.
- [getopt fot MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/), Public Domain.
- [SQLite3](https://www.sqlite.org), Public Domain.

Wenn Du ein Spiele-Publisher bist und mGBA für kommerzielle Verwendung lizenzieren möchtest, schreibe bitte eine e-Mail an [licensing@mgba.io](mailto:licensing@mgba.io) für weitere Informationen.
