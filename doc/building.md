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
