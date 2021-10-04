DDNet PvP
===

Info are added

What is it?这是嘛玩意儿？
------------------------

这是一个基于DDNet制作的PVP mod

拥有在同一地图中切换玩家位置和团队达到换房间的强大功能

以此可以衍生出更多的优质mod

克隆编译
--------
Ubuntu:
    
    sudo apt install build-essential cmake git libcurl4-openssl-dev libssl-dev libfreetype6-dev libglew-dev libnotify-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libsqlite3-dev libwavpack-dev python google-mock bam git libfreetype6-dev libsdl2-dev python3 libicu-dev libmaxminddb-dev mysql-server mysql-client
    
    git clone https://github.com/teeworldsCN/ddnet-pvp.git
    
    cd ddnet-pvp
    
    mkdir -p build
    
    cd build
    
    cmake ..
    
    make

之后的过程如果修改了代码或者改了代码重复此动作:

    cd build
    
    cmake ..

    make

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/teeworldsCN/ddnet-pvp

To clone this repository with full history when you have the necessary libraries on your system already (~220 MB):

    git clone https://github.com/teeworldsCN/ddnet-pvp

To clone this repository with history since we moved the libraries to https://github.com/ddnet/ddnet-libs (~40 MB):

    git clone --shallow-exclude=included-libs https://github.com/teeworldsCN/ddnet-pvp

To clone the libraries if you have previously cloned DDNet without them:

    git submodule update --init --recursive

Dependencies on Linux
---------------------

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install build-essential cmake git libcurl4-openssl-dev libssl-dev libfreetype6-dev libglew-dev libnotify-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libsqlite3-dev libwavpack-dev python

Or on Arch Linux like this:

    sudo pacman -S --needed base-devel cmake curl freetype2 git glew libnotify opusfile python sdl2 sqlite wavpack

There is an [AUR package for pnglite](https://aur.archlinux.org/packages/pnglite/). For instructions on installing it, see [AUR packages installation instructions on ArchWiki](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages).
