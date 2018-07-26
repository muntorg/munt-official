Note
-----

Please note that in order to achieve quality and consistency Gulden is written to modern software standards, this means that we use the latest languages (C++17) and recent libraries (boost) which may not be present on older distributions.

In order to build Gulden you will therefor require a recent compiler (GCC 7.2 or newer) and recent libraries which means you should either use a recent distro or manually install these.

These instructions do not deal with this, however we will provide distro specific instructions in future to assist with this.

Please read all the below instructions before attempting to build the software or asking for assistance with building the software.

Binaries
-----
There are perfectly good binaries for every release, please reconsider your need to build and instead download the latest linux*.tar.gz extract it and simply copy GuldenD out of it instead of going through the unnecessary hassle of building.
Latest binaries can always be found here: https://github.com/Gulden/gulden-official/releases

Gitian instructions
-----

Note the gitian build system is temporarily broken and will be fixed in the next release, check back in the next few days.

When to use gitian for your builds:
* You are on an older distribution
* You want to build with minimal manual fuss
* You are not in a rush - the downside of gitian builds is that they are slow.

Instructions:
* ./contrib/gitian-build.sh --setup --lxc  x.x.x.x    (substitute x.x.x.x for the latest version number)
* ./contrib/gitian-build.sh --build -o -l x.x.x.x

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
* sudo apt-get install curl build-essential libtool autotools-dev autoconf pkg-config libssl-dev
* ./autogen.sh  
* cd depends  
* make NO_QT=1 NO_UPNP=1
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
> dbus fontconfig freetype icu libevent libX11 libXau libxcb libXext libXrender  qrencode &gt;=qt-5.6.1 renderproto xcb_proto xextproto xproto xtrans

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
|Qt wallet|src/qt/Gulden|
|Gulden Daemon/RPC server|src/GuldenD|
|Gulden RPC client|src/Gulden-cli|
|Gulden tx utility|src/Gulden-tx|
