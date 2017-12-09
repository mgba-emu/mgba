#!/bin/sh
if [ $TRAVIS_OS_NAME = "osx" ]; then
	brew update
	brew install qt5 ffmpeg imagemagick sdl2 libzip libpng
else
	sudo apt-get clean
	sudo apt-get update
	sudo apt-get install -y -q cmake libedit-dev libmagickwand-dev \
		libpng-dev libsdl2-dev libzip-dev qtbase5-dev \
		libqt5opengl5-dev qtmultimedia5-dev libavcodec-dev \
		libavutil-dev libavformat-dev libavresample-dev libswscale-dev
fi
