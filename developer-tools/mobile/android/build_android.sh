#! /bin/bash
set -e
set -x

while :; do
  case $1 in
    --nodepends)
      SKIP_DEPENDS=1
      ;;
    --noconfig)
      SKIP_CONFIG=1
      ;;
    *) # Default case: No more options, so break out of the loop.
      break
  esac
  shift
done

mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs | true

NUM_PROCS=$(getconf _NPROCESSORS_ONLN)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
ASSETS=src/frontend/android/unity_wallet/app/src/main/assets

mkdir ${ASSETS} | true
mkdir ${ASSETS}Mainnet | true
mkdir ${ASSETS}Testnet | true

cp src/data/staticfiltercp ${ASSETS}Mainnet/staticfiltercp
cp src/data/staticfiltercptestnet ${ASSETS}Testnet/staticfiltercp

source $DIR/../thirdparty.licenses.sh > ${ASSETS}/core-packages.licenses

source `dirname $0`/ndk_definitions.sh

NDK_ROOT=${NDK_ROOT:-${PWD}/developer-tools/android-ndk-gulden/${NDK_VERSION}}

case "$OSTYPE" in
  darwin*)
      BUILD_PLATFORM=darwin-x86_64
      ;;
  *)
      BUILD_PLATFORM=linux-x86_64
      ;;
esac

PREBUILT=${NDK_ROOT}/toolchains/llvm/prebuilt/${BUILD_PLATFORM}
LIB_DIR=${PREBUILT}/sysroot/usr/lib
TOOLS=${PREBUILT}/bin

if [ -z "$SKIP_CONFIG" ]
then
    ./autogen.sh
else
    echo Skipping autogen
fi

for i in $( dirname ${BASH_SOURCE[0]} )/build_targets/*
do
  source ${i}
  export AR=${TOOLS}/$target_host-ar
  export AS=${TOOLS}/$target_host-clang
  export CC=${TOOLS}/${clang_prefix}${ANDROID_LEVEL}-clang
  export CXX=${TOOLS}/${clang_prefix}${ANDROID_LEVEL}-clang++
  export LD=${TOOLS}/$target_host-ld
  export STRIP=${TOOLS}/$target_host-strip
  export RANLIB=${TOOLS}/$target_host-ranlib
  export LIBTOOL=libtool
  export CXXFLAGS="-fPIC -fdata-sections -ffunction-sections -fomit-frame-pointer ${march_flags} -DEXPERIMENTAL_AUTO_CPP_THREAD_ATTACH"
  #visibility=hidden
  export CFLAGS=${CXXFLAGS}
  export LDFLAGS="-fPIC -Bsymbolic -Wl,--no-undefined -Wl,--gc-sections"

  if [ -z "$SKIP_DEPENDS" ]
  then
    cd depends
    make HOST=$target_host NO_QT=1 NO_UPNP=1 EXTRA_PACKAGES='qrencode protobufunity' -j ${NUM_PROCS}
    cd ..
    ${RANLIB} ../depends/$target_host/lib/*.a
  else
    echo Skipping depends
  fi

  mkdir build_android_${target_host} | true
  cd build_android_${target_host}
  if [ -z "$SKIP_CONFIG" ]
  then
    ../configure --prefix=$PWD/../depends/$target_host ac_cv_c_bigendian=no ac_cv_sys_file_offset_bits=$target_bits --host=$target_host --disable-bench --enable-experimental-asm --disable-tests --disable-man --disable-zmq --without-utils --with-libs --without-daemon --with-jni-libs --with-qrencode
  else
    echo Skipping explicit configure
  fi
  make -j ${NUM_PROCS}
  cd ..

  mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib} | true
  cp build_android_${target_host}/src/.libs/libgulden_unity_jni.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  cp ${LIB_DIR}/${target_host}/libc++_shared.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  ${STRIP} --strip-unneeded src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/*.so
done
