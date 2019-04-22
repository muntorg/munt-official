#! /bin/bash
set -e
set -x

ELECTRON_VERSION=4.1.4

NUM_PROCS=$(getconf _NPROCESSORS_ONLN)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PLATFORM=`./depends/config.guess`
PLATFORM_VENDOR=`./depends/config.guess | cut -d- -f2`

export CXXFLAGS="-fPIC -fdata-sections -ffunction-sections -fomit-frame-pointer"
export CFLAGS=${CXXFLAGS}
if [ ${PLATFORM_VENDOR} = "apple" ]; then
    export LDFLAGS="-fPIC -Bsymbolic"
else
    export LDFLAGS="-fPIC -Bsymbolic -Wl,--gc-sections"
fi


cd depends
make NO_QT=1 NO_UPNP=1 EXTRA_PACKAGES='qrencode' -j ${NUM_PROCS}
cd ..

mkdir build_electron | true
cd build_electron

wget -c https://atom.io/download/electron/v${ELECTRON_VERSION}/iojs-v${ELECTRON_VERSION}.tar.gz
mkdir electron-${ELECTRON_VERSION} | true
tar -xvf iojs-v${ELECTRON_VERSION}.tar.gz -C electron-${ELECTRON_VERSION}

wget -c https://github.com/nodejs/nan/archive/v2.13.2.tar.gz
tar -xvf  v2.13.2.tar.gz

export CXXFLAGS="${CXXFLAGS} -I${DIR}/../build_electron/electron-${ELECTRON_VERSION}/node_headers/include/node/ -I${DIR}/../build_electron/nan-2.13.2"
export CFLAGS=${CXXFLAGS}


../autogen.sh
../configure --prefix=$PWD/../depends/${PLATFORM} --disable-bench --disable-tests --disable-man --disable-zmq --without-utils  --without-daemon --with-node-js-libs --with-qrencode --with-gui=no --enable-debug
make V=1 -j ${NUM_PROCS}
cd ..

