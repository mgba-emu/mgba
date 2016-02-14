#!/bin/sh
if [ $TRAVIS_OS_NAME = "osx" ]; then
	brew update
	brew install qt5 ffmpeg imagemagick sdl2 libzip libpng
else
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
	sudo apt-get update -qq
	sudo apt-get purge cmake -qq
	sudo apt-get install -y -qq cmake libedit-dev libmagickwand-dev \
		libpng-dev libsdl2-dev libzip-dev qtbase5-dev \
		libqt5opengl5-dev qtmultimedia5-dev libavcodec-dev \
		libavutil-dev libavformat-dev libavresample-dev libswscale-dev
fi
