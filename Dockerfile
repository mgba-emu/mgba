FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    zipcmp \
    zipmerge \
    libzip-dev \
    minizip \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libzip-dev \
    libpng-dev \
    libsqlite3-dev \
    zlib1g-dev \
    libedit-dev \
    libelf-dev \
    libepoxy-dev \
    libegl1-mesa-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxi-dev \
    libxcursor-dev \
    libasound2-dev \
    libpulse-dev \
    libudev-dev \
    qt5-qmake \
    qtbase5-dev \
    qtbase5-dev-tools \
    libqt5opengl5-dev \
    qtmultimedia5-dev \
    libsdl2-dev \
    libpulse-dev \
    libfreetype6-dev \
    qttools5-dev \
    qttools5-dev-tools \
    && rm -rf /var/lib/apt/lists/*
        
#Install Python and link it to the different package names
RUN apt install python3 && ln -s /usr/bin/python3 /usr/bin/python

RUN ln -s /usr/bin/false /usr/bin/ziptool

WORKDIR /workspace
RUN mkdir -p /workspace/build

#bash
CMD ["/bin/bash"]
