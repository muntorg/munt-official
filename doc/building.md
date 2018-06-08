Please read all the below instructions before attempting to build the software or asking for assistance with building the software.

Distro specific instructions:

|Distro|Version|
|:-----------|:-------|
|Ubuntu|[16.04.1](https://gist.github.com/mjmacleod/a3562af661661ce6206e5950e406ff9d) |
|Ubuntu|[14.04.4](https://gist.github.com/mjmacleod/31ad31386fcb421a7ba04948e83ace76) |


Generic instructions:

Gulden is autotools based, to build Gulden from this repository please follow these steps:
* ./autogen.sh
* automake
* ./configure
* make

Prerequisites:
> &gt;=g++-78 pkg-config autoconf libtool automake gperf bison flex

Required dependencies:
> &gt;=bdb-4.8.30 &gt;=boost-1_66_0 expat-2 openssl miniupnpc protobuf zeromq

Optional dependencies (depending on configure - e.g. qt only for GUI builds):
> dbus fontconfig freetype icu libevent libX11 libXau libxcb libXext libXrender  qrencode &gt;=qt-5.6.1 renderproto xcb_proto xextproto xproto xtrans

Troubleshooting:

If your distro  does not have boost 1.66 you can do the following to build it
cd depends
make boost

And then add it to your configure flags
> ./configure --with-boost=<path> LDFLAGS="-L<path>/lib/" CPPFLAGS="-I<path>/include" <otherconfigureflagshere>

To run after doing the above
> LD_LIBRARY_PATH=<path>/lib src/GuldenD 

If your distro is missing Berkley DB 4.8 (error: Found Berkeley DB other than 4.8, required for portable wallets)
Either configure with an incompatible bdb (Your wallet may not be portable to machines using older versions in this case):
> ./configure --with-incompatible-bdb <otherconfigureflagshere>

Or compile your own:
> sudo mkdir /db-4.8 && sudo chmod -R a+rwx /db-4.8 && wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz' && tar -xzvf db-4.8.30.NC.tar.gz && cd db-4.8.30.NC/build_unix/ && ../dist/configure --enable-cxx --disable-shared --with-pic --prefix=/db-4.8/ && make install

> ./configure LDFLAGS="-L/db-4.8/lib/" CPPFLAGS="-I/db-4.8/include"

Binaries are output as follows by the build process:

|Binary|Location|
|:-----------|:---------|
|Qt wallet|src/qt/Gulden|
|Gulden Daemon/RPC server|src/GuldenD|
|Gulden RPC client|src/Gulden-cli|
|Gulden tx utility|src/Gulden-tx|
