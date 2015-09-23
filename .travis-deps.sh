#!/bin/sh
if [ $TRAVIS_OS_NAME = "osx" ]; then
	brew update
	brew install qt5 ffmpeg imagemagick sdl2 libzip libpng
else
	sudo add-apt-repository ppa:smspillaz/cmake-2.8.12 -y
	sudo add-apt-repository ppa:zoogie/sdl2-snapshots -y
	sudo add-apt-repository ppa:immerrr-k/qt5-backport -y
	sudo add-apt-repository ppa:spvkgn/ffmpeg+mpv -y
	sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
	sudo apt-get update -qq
	sudo apt-get purge cmake -qq
	sudo apt-get install -y -qq cmake libedit-dev libmagickwand-dev \
		g++-4.8 libpng-dev libsdl2-dev libzip-dev qtbase5-dev \
		libqt5opengl5-dev qtmultimedia5-dev libavcodec-dev \
		libavutil-dev libavformat-dev libavresample-dev libswscale-dev
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 100
	sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 100
fi
