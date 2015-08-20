#! /usr/bin/env bash
# vim: set ts=3 sw=3 noet ft=sh : bash


mkdir -p build_mgba

cp -f src/util/vfs/vfs-mem.c build_mgba
cp -f src/util/vfs/vfs-file.c build_mgba
cp -f src/util/vfs/vfs-dirent.c build_mgba
cp -f src/debugger/debugger.[ch] build_mgba
cp -f src/debugger/memory-debugger.[ch] build_mgba
cp -f src/third-party/blip_buf/blip_buf.[ch] build_mgba
cp -f src/platform/commandline.[ch] src/util/
c:/devkitPro/devkitPPC/bin/raw2c src/platform/wii/font.tpl
mv font.[ch] build_mgba

make -f Makefile.wii

rm -f src/util/commandline.[ch]