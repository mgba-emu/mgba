#!/bin/sh
if [ $TRAVIS_OS_NAME = "osx" ]; then
	brew update
	brew install qt5 ffmpeg imagemagick sdl2 libedit libelf libpng libzip
else
	sudo apt-get clean
	sudo add-apt-repository -y ppa:beineri/opt-qt542-trusty
	sudo add-apt-repository -y ppa:george-edison55/cmake-3.x
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
	sudo apt-get update
	sudo apt-get install -y -q cmake libedit-dev libelf-dev libmagickwand-dev \
		libpng-dev libsdl2-dev libzip-dev qt54base qt54multimedia \
		libavcodec-dev libavutil-dev libavformat-dev libavresample-dev \
		libswscale-dev
	if [ "$CC" == "gcc" ]; then
		sudo apt-get install -y -q gcc-5 g++-5
		export CC=gcc-5
		export CXX=g++-5
	fi
fi
