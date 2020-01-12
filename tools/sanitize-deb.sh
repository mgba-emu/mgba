#!/bin/sh
# Clean up the caveats that CPack leaves behind
BINARY=mgba

rmdep () {
    local DEP=$1
    echo Removing dependency $DEP
    sed -i~ "s/[^, ]*$DEP[^,]*//g" deb-temp/DEBIAN/control
}

adddep() {
    local DEP=$1
    echo Adding dependency $DEP
    sed -i~ "s/^Depends: /&$DEP,/" deb-temp/DEBIAN/control
}

while [ $# -gt 0 ]; do
    DEB=$1
    dpkg-deb -R $DEB deb-temp
    PKG=`grep Package deb-temp/DEBIAN/control | cut -f2 -d ' '`
    echo Found package $PKG

    case $PKG in
    *-base)
        PKG=lib$BINARY
        rmdep sdl
        rmdep qt
    ;;
    *-qt)
        PKG=$BINARY-qt
        rmdep libav
        rmdep libedit
        rmdep libelf
        rmdep libgl
        rmdep libpng
        rmdep libzip
        rmdep libmagickwand
        rmdep libsqlite3
        rmdep libswresample
        rmdep libswscale
        rmdep zlib
        adddep lib$BINARY
    ;;
    *-sdl)
        PKG=$BINARY-sdl
        rmdep libav
        rmdep libedit
        rmdep libelf
        rmdep libgl
        rmdep libpng
        rmdep qt
        rmdep libzip
        rmdep libmagickwand
        rmdep libsqlite3
        rmdep libswresample
        rmdep libswscale
        rmdep zlib
        adddep lib$BINARY
    ;;
    *)
        echo Unknown package!
    esac

    sed -i~ "s/,,*/,/g" deb-temp/DEBIAN/control
    sed -i~ "s/,$//g" deb-temp/DEBIAN/control
    sed -i~ "/^[^:]*: $/d" deb-temp/DEBIAN/control
    sed -i~ "s/^Package: .*$/Package: $PKG/" deb-temp/DEBIAN/control
    rm deb-temp/DEBIAN/control~
    chmod 644 deb-temp/DEBIAN/md5sums
    chown -R root:root deb-temp
    dpkg-deb -b deb-temp $PKG.deb
    rm -rf deb-temp
    shift
done
