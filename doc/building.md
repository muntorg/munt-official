Note
-----

Please note that in order to achieve quality and consistency Munt is written to modern software standards, this means that we use the latest languages (C++17) and recent libraries (boost) which may not be present on older distributions.

In order to build Munt you will therefore require a recent compiler (GCC 7.2 or newer) and recent libraries which means you should either use a recent distro or manually install these.

These instructions do not deal with this, however we will provide distro specific instructions in future to assist with this.

Please read all the below instructions before attempting to build the software or asking for assistance with building the software.

Binaries
-----
There are binaries for every release, please reconsider your need to build and unless you have a very good reason to do so rather just download these.
Latest binaries can always be found here: https://github.com/Gulden/gulden-official/releases
Download the latest linux*.tar.gz extract it and simply copy Munt-daemon out of it instead of going through the unnecessary hassle of building.

Generic docker instructions
-----

When to use docker for your builds:
* You are on an older distribution
* You want to build with minimal manual fuss and with minimal distruption/changes to your operating system

The easiest way to build Munt on any distribution with minimal work is to simply use the docker build script.
Instructions:
* Follow distro specific steps to install docker (e.g. `apt-get install docker.io && systemctl start docker`)
* Run the docker build script from the root of this repo `./developer-tools/build_docker.sh`
* Resulting binaries will be output inside the `build` directory


Platform specific instructions
-----

The platform specific instructions attempt to minimise compiling by using native system packages where possible. Note that installing these packages may have unintended consequences for other packages on your system; It is your responsibility to  understand package management and your system. If you cannot deal with the possibility of such side effects it is better to follow the guix instructions.

|Platform|Version|
|:-----------|:---------|
|Windows|[10](building_windows.md)|
|Ubuntu|[14.04.5](https://gist.github.com/mjmacleod/31ad31386fcb421a7ba04948e83ace76#file-ubuntu_14-04-5-txt)|
|Ubuntu|[16.04.4](https://gist.github.com/mjmacleod/a3562af661661ce6206e5950e406ff9d#file-ubuntu_16-04-4-txt)|
|Ubuntu|[18.04](https://gist.github.com/mjmacleod/c5be3d05d213317b7ae4cbc50324d5ee#file-ubuntu_18-04-txt)|


Generic instructions
-----


Munt is autotools based, and has a dependency management system capable of building all libraries it requires.

To build Munt-daemon from this repository please follow these steps which are valid for Ubuntu and Debian but can easily be modified for any distribution:
* sudo apt-get install curl build-essential libtool autotools-dev autoconf pkg-config libssl-dev
* ./autogen.sh
* cd depends
* make NO_QT=1 NO_UPNP=1
* cd ..
* mkdir build && cd build
* ../configure --prefix=$PWD/../depends/x86_64-pc-linux-gnu/
* make -j$(nproc)

To build the full UI version of Munt:
* sudo apt-get install curl build-essential libtool autotools-dev autoconf pkg-config libssl-dev libpcre++-dev
* ./autogen.sh  
* cd depends  
* make
* cd ..
* mkdir build && cd build
* ../configure --prefix=$PWD/../depends/x86_64-pc-linux-gnu/
* make -j$(nproc)

Note that it may take a while to build all the dependencies.
Note if you are using a shell other than bash or your system doesn't have the `nproc` command you will need to manually substitute a number (e.g. 4 for a 4 core cpu) for `$(nproc)` instead.



Details for advanced builders
-----

If you wish to attempt to use existing dependencies in your distribution instead of using the depends system please note the following.

Prerequisites for building:
> &gt;=g++-7 pkg-config autoconf libtool automake gperf bison flex

Required dependencies:
> &gt;=bdb-4.8.30 &gt;=boost-1_66_0 expat-2 openssl miniupnpc protobuf zeromq

Optional dependencies (depending on configure - e.g. qt only for GUI builds):
> dbus fontconfig freetype icu libevent libX11 libXau libxcb libXext libXrender libpcre qrencode &gt;=qt-5.6.1 renderproto xcb_proto xextproto xproto xtrans



guix instructions
-----

When to use guix for your builds:
* You want to do reproducible builds like the official builds the developers do

Instructions:
* Install guix
* Decide which host you want to target (e.g. `x86_64-linux-gnu`)
* `HOSTS="x86_64-linux-gnu" ./contrib/guix/guix_build`

For more detailed instructions see contrib/guix/README.md


MuntCore framework for iOS
-----

Prerequisites:

* Xcode 10
* [Java JDK](https://www.oracle.com/technetwork/java/javase/downloads/index.html), needed by Djinni to generate the ObjC - C++ bridging code.
* [Homebrew](https://brew.sh) to install required build tools

In Xcode preferences select the Xcode Command Line Tools.

Use Homebrew to install build tools:

> brew install cmake autoconf automake libtool pkgconfig

Build the framework library:

> ./developer-tools/mobile/ios/build_ios_core.sh


MuntCore framework for android
-----

Fetch build prerequisites:
* `./developer-tools/mobile/android/fetch_android.sh`

Build the framework library:
* `./developer-tools/mobile/android/build_android_core.sh`




Troubleshooting
-----

If your distro is missing Berkley DB 4.8 (error: Found Berkeley DB other than 4.8, required for portable wallets)
Either compile your own BDB, or configure with an incompatible bdb (Your wallet may not be portable to machines using older versions in this case):
> ./configure --with-incompatible-bdb <otherconfigureflagshere>

Binaries are output as follows by the build process:

|Binary|Location|
|:-----------|:---------|
|Qt wallet|build/src/qt/Munt|
|Munt Daemon/RPC server|build/src/Munt-daemon|
|Munt RPC client|build/src/Munt-cli|
|Munt tx utility|build/src/Munt-tx|
