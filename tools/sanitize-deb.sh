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
    mv $DEB $DEB~
    sed -i~ s/mgba-// deb-temp/DEBIAN/control
    PKG=`head -n1 deb-temp/DEBIAN/control | cut -f2 -d ' '`
    echo Found pacakge $PKG

    case $PKG in
    lib$BINARY)
        rmdep sdl
        rmdep qt
    ;;
    $BINARY-qt)
        rmdep libav
        rmdep libedit
        rmdep libpng
        rmdep libzip
        rmdep libmagickwand
        rmdep libswscale
        rmdep zlib
        adddep lib$BINARY
    ;;
    $BINARY-sdl)
        rmdep libav
        rmdep libedit
        rmdep libpng
        rmdep qt
        rmdep libzip
        rmdep libmagickwand
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
    rm deb-temp/DEBIAN/control~
    chmod 644 deb-temp/DEBIAN/md5sums
    chown -R root:root deb-temp
    dpkg-deb -b deb-temp $DEB
    rm -rf deb-temp
    shift
done
