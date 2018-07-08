#!/bin/sh
if [ $TRAVIS_OS_NAME = "osx" ]; then
	brew update
	brew install qt5 ffmpeg imagemagick sdl2 libedit libelf libpng libzip
fi
