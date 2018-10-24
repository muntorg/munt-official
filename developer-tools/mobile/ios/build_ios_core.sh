#! /bin/bash
set -e

NPROC=`sysctl -n hw.physicalcpu`

target_host="aarch64-ios"

cd depends
make HOST=$target_host NO_QT=1 NO_UPNP=1 -j $NPROC
cd ..

mkdir -p build/${target_host}
cd build/${target_host}
../../autogen.sh
../../configure --prefix=$PWD/../../depends/$target_host --host=$target_host --disable-bench --enable-experimental-asm --disable-tests --disable-man --disable-zmq --without-utils --without-daemon --with-objc-libs --with-qrencode
make -j $NPROC
cd ..
