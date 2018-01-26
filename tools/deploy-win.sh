#!/bin/bash
BINARY=$1
INSTALLPATH="$2"
WORKDIR="$3"


if [ -z "$DESTDIR" ]; then
	OUTDIR="$INSTALLPATH"
else
	if [ -n $(echo ${INSTALLPATH} | grep "^[A-Z]:") ]; then
		INSTALLPATH="${INSTALLPATH:3}"
	fi
	OUTDIR="$WORKDIR/$DESTDIR/$INSTALLPATH"
fi

IFS=$'\n'
if [ -n $(which ntldd 2>&1 | grep /ntldd) ]; then
	DLLS=$(ntldd -R "$BINARY" | grep -i mingw | cut -d">" -f2 | sed -e 's/(0x[0-9a-f]\+)//' -e 's/^ \+//' -e 's/ \+$//' -e 's,\\,/,g')
elif [ -n $(which gdb 2>&1 | grep /gdb) ]; then
	DLLS=$(gdb "$BINARY" --command=$(dirname $0)/dlls.gdb | grep -i mingw | cut -d" " -f7- | sed -e 's/^ \+//' -e 's/ \+$//' -e 's,\\,/,g')
else
	echo "Please install gdb or ntldd for deploying DLLs"
fi
cp -vu $DLLS "$OUTDIR"
if [ -n $(which windeployqt 2>&1 | grep /windeployqt) ]; then
	windeployqt --no-opengl-sw --no-svg --release --dir "$OUTDIR" "$BINARY"
fi
