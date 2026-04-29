#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
android_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
source_dir="${1:-$android_dir/app/src/main}"

if [ ! -d "$source_dir" ]; then
	echo "Android source directory not found: $source_dir" >&2
	exit 2
fi

forbidden="$(
	find "$source_dir" -type f \( \
		-iname "*.gba" -o \
		-iname "*.agb" -o \
		-iname "*.gb" -o \
		-iname "*.gbc" -o \
		-iname "*.sgb" -o \
		-iname "*.zip" -o \
		-iname "*.7z" -o \
		-iname "*.rar" -o \
		-iname "*.sav" -o \
		-iname "*.srm" -o \
		-iname "*.ss" -o \
		-iname "*.ss[0-9]" -o \
		-iname "*.state" -o \
		-iname "*.savestate" -o \
		-iname "*.bios" -o \
		-iname "*bios*.bin" \
	\) -print
)"

if [ -n "$forbidden" ]; then
	echo "Forbidden ROM/archive/save/state/BIOS artifacts found in Android app sources:" >&2
	printf '%s\n' "$forbidden" >&2
	exit 1
fi

echo "No forbidden ROM/archive/save/state/BIOS artifacts found under $source_dir"
