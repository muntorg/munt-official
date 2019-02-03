Note
-----

Please note that in order to achieve quality and consistency Gulden is written to modern software standards, this means that we use the latest languages (C++17) and recent libraries (boost) which may not be present on older distributions.

In order to build Gulden you will therefore require a recent compiler (GCC 7.2 or newer) and recent libraries which means you should either use a recent distro or manually install these.

These instructions do not deal with this, however we will provide distro specific instructions in future to assist with this.

Please read all the below instructions before attempting to build the software or asking for assistance with building the software.

Binaries
-----
There are binaries for every release, please reconsider your need to build and unless you have a very good reason to do so rather just download these.
Latest binaries can always be found here: https://github.com/Gulden/gulden-official/releases
Download the latest linux*.tar.gz extract it and simply copy GuldenD out of it instead of going through the unnecessary hassle of building.

Gitian instructions
-----

Note work on the automated gitian build script is temporarily ongoing. If the setup doesn't work for you it may be necessary to take manual steps, bug reports and pull requests to improve this are welcome.

When to use gitian for your builds:
* You are on an older distribution
* You want to build with minimal manual fuss
* You are not in a rush - the downside of gitian builds is that they are slow.

Instructions:
* ./contrib/gitian-build.sh --setup x.x.x.x    (substitute x.x.x.x for the latest version number e.g. 2.0.0.9)
* ./contrib/gitian-build.sh --build -o -l x.x.x.x

Distribution specific instructions
-----

The distribution specific instructions attempt to minimise compiling by using native system packages where possible. Note that installing these packages may have unintended consequences for other packages on your system; It is your responsibility to  understand package management and your system. If you cannot deal with the possibility of such side effects it is better to follow the Gitian instructions.

|Distribution|Version|
|:-----------|:---------|
|Ubuntu|[14.04.5](https://gist.github.com/mjmacleod/31ad31386fcb421a7ba04948e83ace76#file-ubuntu_14-04-5-txt)|
|Ubuntu|[16.04.4](https://gist.github.com/mjmacleod/a3562af661661ce6206e5950e406ff9d#file-ubuntu_16-04-4-txt)|
|Ubuntu|[18.04](https://gist.github.com/mjmacleod/c5be3d05d213317b7ae4cbc50324d5ee#file-ubuntu_18-04-txt)|


Generic instructions
-----


Gulden is autotools based, and has a dependency management system capable of building all libraries it requires.

To build GuldenD from this repository please follow these steps which are valid for Ubuntu and Debian but can easily be modified for any distribution:
* sudo apt-get install curl build-essential libtool autotools-dev autoconf pkg-config libssl-dev
* ./autogen.sh
* cd depends
* make NO_QT=1 NO_UPNP=1
* cd ..
* mkdir build && cd build
* ../configure --prefix=$PWD/../depends/x86_64-pc-linux-gnu/
* make -j$(nproc)

To build the full UI version of Gulden:
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

Troubleshooting
-----

Gulden dynamically links qt by default as a licensing requirement for distribution. If you have trouble running Gulden after building it you may need to tell it where to find the libraries:
> LD_LIBRARY_PATH=<path>/lib ./src/Gulden

If your distro is missing Berkley DB 4.8 (error: Found Berkeley DB other than 4.8, required for portable wallets)
Either compile your own BDB, or configure with an incompatible bdb (Your wallet may not be portable to machines using older versions in this case):
> ./configure --with-incompatible-bdb <otherconfigureflagshere>

Binaries are output as follows by the build process:

|Binary|Location|
|:-----------|:---------|
|Qt wallet|build/src/qt/Gulden|
|Gulden Daemon/RPC server|build/src/GuldenD|
|Gulden RPC client|build/src/Gulden-cli|
|Gulden tx utility|build/src/Gulden-tx|
