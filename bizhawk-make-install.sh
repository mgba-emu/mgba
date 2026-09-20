#!/bin/sh

BASEDIR=$(dirname $0)
if [ "$(uname -s)" = "Linux" ]; then
	cd "$BASEDIR/src/platform/bizhawk/linux"
else
	cd "$BASEDIR/src/platform/bizhawk/mingw"
fi
make clean
make -j4
make install
