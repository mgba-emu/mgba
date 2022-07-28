mGBA
====

mGBA es un emulador para juegos de Game Boy Advance. Su objetivo es ser más rápido y más preciso que muchos emuladores de Game Boy Advance existentes, además de añadir funciones que otros emuladores no tienen. También es compatible con juegos de Game Boy y Game Boy Color.

Las noticias actualizadas y las descargas se encuentran en [mgba.io](https://mgba.io/).

[![Estado de la compilación](https://travis-ci.org/mgba-emu/mgba.svg?branch=master)](https://travis-ci.org/mgba-emu/mgba)
[![Estado de la traducción](https://hosted.weblate.org/widgets/mgba/-/svg-badge.svg)](https://hosted.weblate.org/engage/mgba)

Características
--------

- Soporte de hardware Game Boy Advance altamente preciso[<sup>[1]</sup>](#missing).
- Soporte de hardware Game Boy/Game Boy Color.
- Emulación rápida. Corre a velocidad completa en hardware de gama baja, como los netbooks.
- Interfaz gráfica en SDL y Qt.
- Soporte para cable de enlace (link cable) local (en la misma computadora).
- Detección de tipos de guardado, incluso para tamaños de memoria flash[<sup>[2]</sup>](#flashdetect).
- Soporte para cartuchos con sensores de movimiento y vibración (solo usable con mandos).
- Soporte para reloj en tiempo real, incluso sin configuración.
- Soporte para sensor solar, para juegos Boktai.
- Soporta la Cámara y la Impresora Game Boy.
- Implementación interna de BIOS, y opción para usar una BIOS externa.
- Modo turbo/avance rápido al mantener Tab presionado.
- Retroceder al presionar "`".
- Salto de cuadros de hasta 10 cuadros por vez.
- Captura de pantalla (pantallazo).
- Soporta códigos de truco.
- 9 espacios para estados de guardado. Estos tambien pueden ser vistos como pantallazos.
- Grabación de video, GIF, WebP, y APNG.
- Soporte para e-Reader.
- Controles modificables para teclado y mandos.
- Cargar desde archivos ZIP y 7z.
- Soporta parches IPS, UPS y BPS.
- Depuración de juegos a través de una interfaz de línea de comandos y soporte remoto GDB, compatible con IDA Pro.
- Retroceso configurable.
- Soporte para cargar y exportar instantáneas de GameShark y Action Replay.
- Núcleos disponibles para RetroArch/Libretro y OpenEmu.
- Traducciones de la comunidad a través de [Weblate](https://hosted.weblate.org/engage/mgba).
- Otras cosas más pequeñas.

#### Mappers (controladores de memoria) soportados

Estos mappers tienen soporte completo:

- MBC1
- MBC1M
- MBC2
- MBC3
- MBC3+RTC
- MBC5
- MBC5+Rumble
- MBC7
- Wisdom Tree (sin licencia)
- Pokémon Jade/Diamond (sin licencia)
- BBD (sin licencia, similar a MBC5)
- Hitek (sin licencia, similar a MBC5)

Estos mappers tienen soporte parcial:

- MBC6 (sin soporte para escribir a la memoria flash)
- MMM01
- Pocket Cam
- TAMA5 (sin soporte para RTC)
- HuC-1 (sin soporte para IR)
- HuC-3 (sin soporte para RTC e IR)

### Características planeadas

- Soporte para cable de enlace por red.
- Soporte para cable de enlace por Joybus para Dolphin.
- Mezcla de audio MP2k, para mayor calidad de sonido.
- Soporte de regrabación para speedruns asistidos por herramientas (TAS).
- Soporte de Lua para prog.
- Un completo paquete de depuración.
- Compatibilidad con adaptadores inalámbricos.

Plataformas soportadas
-------------------

- Windows 7 o más reciente
- OS X 10.9 (Mavericks)[<sup>[3]</sup>](#osxver) o más reciente
- Linux
- FreeBSD
- Nintendo 3DS
- Nintendo Switch
- Wii
- PlayStation Vita

Otras plataformas Unix-like, como OpenBSD, funcionan también, pero no han sido probadas.

### Requisitos de sistema

Los requisitos son mínimos. Cualquier computadora que pueda ejecutar Windows Vista o más reciente debería ser capaz de emular. También se requiere soporte para OpenGL 1.1 o más reciente, con OpenGL 3.2 o más reciente para los shaders y las funciones avanzadas.

Descargas
---------

Las descargas se pueden encontrar en la página web oficial, en la sección [Descargas][downloads]. El código fuente se puede encontrar en [GitHub][source].

Controles
--------

Los controles son configurables en el menú de configuración. Many game controllers should be automatically mapped by default. The default keyboard controls are as follows:

- **A**: X
- **B**: Z
- **L**: A
- **R**: S
- **Start**: Entrar
- **Select**: Retroceso

Compilar
---------

La compilación requiere el uso de CMake 3.1 o más reciente. GCC y Clang funcionan para compilar mGBA, pero Visual Studio 2013 y posteriores no funcionan. El soporte para Visual Studio 2015 y más recientes llegará pronto.

#### Compilación por Docker

Recomendamos usar Docker para compilar en la mayoría de las plataformas. Proporcionamos varias imágenes Docker que contienen la cadena de herramientas y las dependencias necesarias para compilar mGBA a través de varias plataformas. 

Para usar una imagen Docker para compilar mGBA, ejecuta este comando mientras estés en el directorio donde hayas desplegado (checkout) el código fuente de mGBA:

	docker run --rm -t -v $PWD:/home/mgba/src mgba/windows:w32

Esto producirá un directorio `build-win32` con los ejecutables compilados. Reemplaza `mgba/windows:w32` con otro nombre de imagen Docker para otras plataformas, lo cual creará un directorio correspondiente. Las siguientes imágenes están disponibles en Docker Hub:

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

#### Compilación en *nix

Si quieres usar CMake para compilar mGBA en un sistema Unix-like, recomendamos los siguientes comandos:

	mkdir build
	cd build
	cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr ..
	make
	sudo make install

Esto compilará e instalará mGBA en `/usr/bin` y `/usr/lib`. Las dependencias que estén instaladas serán detectadas automáticamente, y las características que estén desactivadas si las dependencias no se encuentran serán mostradas después de que el comando `cmake` muestre las advertencias.

Si estás en macOS, los pasos son un poco diferentes. Asumiendo que usas el gestor de paquetes Homebrew, los comandos recomendados para obtener las dependencias y compilar mGBA son:

	brew install cmake ffmpeg libzip qt5 sdl2 libedit pkg-config
	mkdir build
	cd build
	cmake -DCMAKE_PREFIX_PATH=`brew --prefix qt5` ..
	make

Toma nota de que no debes usar `make install` en macOS, ya que no funcionará correctamente.

#### Compilación en Windows para desarrolladores

##### MSYS2

Para desarrollar en Windows, recomendamos MSYS2. Sigue las instrucciones en su [sitio web](https://msys2.github.io). Asegúrate de que estés ejecutando la versión de 32 bits ("MSYS2 MinGW 32-bit") (o la versión de 64 bits "MSYS2 MinGW 64-bit" si quieres compilar para x86_64) y ejecuta estos comandos adicionales para instalar las dependencias necesarias (toma nota de que esto descargará más de 1100 MB en paquetes, así que puede demorarse un poco):

	pacman -Sy --needed base-devel git ${MINGW_PACKAGE_PREFIX}-{cmake,ffmpeg,gcc,gdb,libelf,libepoxy,libzip,pkgconf,qt5,SDL2,ntldd-git}

Despliega (haz check out en) el código fuente ejecutando este comando:

	git clone https://github.com/mgba-emu/mgba.git

Luego, compílalo usando estos comandos:

	mkdir -p mgba/build
	cd mgba/build
	cmake .. -G "MSYS Makefiles"
	make -j$(nproc --ignore=1)

Ten en cuenta de que esta versión de mGBA para Windows no es adecuada para distribuirse, debido a la dispersión de las DLL que necesita para funcionar, pero es perfecta para el desarrollo. Sin embargo, si quieres distribuir tal compilación (por ejemplo, para pruebas en máquinas que no tienen el entorno MSYS2 instalado), al ejecutar `cpack -G ZIP` se preparará un archivo zip con todas las DLLs necesarias.

##### Visual Studio

Construir usando Visual Studio requiere una configuración igualmente complicada. Para empezar, necesitarás instalar [vcpkg](https://github.com/Microsoft/vcpkg). Después de instalar vcpkg necesitarás instalar varios paquetes adicionales:

    vcpkg install ffmpeg[vpx,x264] libepoxy libpng libzip sdl2 sqlite3

Toma nota de que esta instalación no soportará la codificación de video acelerada por hardware en Nvidia. Si te preocupa esto, necesitarás instalar CUDA, y luego sustituir `ffmpeg[vpx,x264,nvcodec]` en el comando anterior.

También necesitarás instalar Qt. Desafortunadamente, debido a que Qt pertenece y es administrado por una empresa en problemas en lugar de una organización razonable, ya no existe un instalador de la edición de código abierto sin conexión para la última versión, por lo que deberás recurrir a un [instalador de una versión anterior](https://download.qt.io/official_releases/qt/5.12/5.12.9/qt-opensource-windows-x86-5.12.9.exe) (que quiere que crees una cuenta que de otro modo sería inútil, pero puedes omitir esto al configurar temporalmente un proxy inválido o deshabilitar la red), usa el instalador en línea (que requiere una cuenta de todos modos) o usa vcpkg para construirlo (lentamente). Ninguna de estas son buenas opciones. Si usas el instalador, querrás instalar las versiones de MSVC correspondientes. Ten en cuenta que los instaladores sin conexión no son compatibles con MSVC 2019. Para vcpkg, querrás instalarlo así, lo que llevará bastante tiempo, especialmente en computadoras de cuatro núcleos o menos:

    vcpkg install qt5-base qt5-multimedia

Luego, abre Visual Studio, selecciona Clonar repositorio, e ingresa `https://github.com/mgba-emu/mgba.git`. Cuando Visual Studio termine de clonar, ve a Archivo > CMake y abre el archivo CMakeLists.txt en la raíz del repositorio desplegado. Desde allí, puedes trabajar en MGBA en Visual Studio de manera similar a otros proyectos CMake de Visual Studio.

#### Compilación con cadenas de herramientas (toolchain)

Si tienes devkitARM (para 3DS), devkitPPC (para Wii), devkitA64 (para Switch), o vitasdk (para PS Vita), puedes usar los siguientes comandos para compilar:

	mkdir build
	cd build
	cmake -DCMAKE_TOOLCHAIN_FILE=../src/platform/3ds/CMakeToolchain.txt ..
	make

Reemplaza el parámetro `-DCMAKE_TOOLCHAIN_FILE` para las plataformas:

- 3DS: `../src/platform/3ds/CMakeToolchain.txt`
- Switch: `../src/platform/switch/CMakeToolchain.txt`
- Vita: `../src/platform/psp2/CMakeToolchain.vitasdk`
- Wii: `../src/platform/wii/CMakeToolchain.txt`

### Dependencies

mGBA no tiene dependencias duras, sin embargo, se requieren las siguientes dependencias opcionales para características específicas. Las características se desactivarán si no se pueden encontrar las dependencias.

- Qt 5: para la interfaz gráfica. Qt Multimedia o SDL se requieren para el audio.
- SDL: para un frontend más básico y soporte de gamepad en el frontend de Qt. Se recomienda SDL 2, pero se admite 1.2.
- zlib y libpng: para soporte de capturas de pantalla y soporte de estados de guardado embebidos en PNG.
- libedit: para soporte del depurador de línea de comandos.
- ffmpeg o libav: para grabación de video, GIF, WebP y APNG.
- libzip o zlib: para cargar ROMs almacenadas en archivos zip.
- SQLite3: para la bases de datos de juegos.
- libelf: para cargar ELF.

SQLite3, libpng y zlib están incluidos en el emulador, por lo que no necesitan ser compilados externamente primero.

Notas a pie
---------

<a name="missing">[1]</a> Las características faltantes actualmente son

- OBJ window para los modos 3, 4 y 5 ([Bug #5](http://mgba.io/b/5))

<a name="flashdetect">[2]</a> La detección del tamaño de la memoria flash no funciona en algunos casos. Se pueden configurar en tiempo de ejecución, pero se recomienda ingresar un bug si se encuentra un caso así.

<a name="osxver">[3]</a> 10.9 sólo se necesita para la versión con Qt. Puede ser posible compilar o hacer funcionar la versión Qt en 10.7 o versiones más antigas, pero esto no está oficialmente soportado. La versión SDL funciona en 10.5, y puede funcionar en versiones anteriores.

[downloads]: http://mgba.io/downloads.html
[source]: https://github.com/mgba-emu/mgba/

Copyright
---------

mGBA es Copyright © 2013 – 2021 Jeffrey Pfau. Es distribuído bajo la [licencia pública de Mozilla (Mozilla Public License) version 2.0](https://www.mozilla.org/MPL/2.0/). Una copia de la licencia está disponible en el archivo LICENSE.

mGBA contiene las siguientes bibliotecas de terceros:

- [inih](https://github.com/benhoyt/inih), que es copyright © 2009 - 2020 Ben Hoyt y se utiliza bajo licencia de la cláusula 3 de BSD.
- [blip-buf](https://code.google.com/archive/p/blip-buf), que es  copyright © 2003 - 2009 Shay Green y se usa bajo LGPL.
- [LZMA SDK](http://www.7-zip.org/sdk.html), la cual está en el dominio público.
- [MurmurHash3](https://github.com/aappleby/smhasher), implementación por Austin Appleby, la cual está en el dominio público.
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/), la cual está en el dominio público.
- [SQLite3](https://www.sqlite.org), la cual está en el dominio público.

Si usted es un editor de juegos y desea obtener una licencia de mGBA para uso comercial, por favor envíe un correo electrónico a [licensing@mgba.io](mailto:licensing@mgba.io) para obtener más información.
