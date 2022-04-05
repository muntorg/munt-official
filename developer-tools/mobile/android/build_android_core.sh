#! /bin/bash
set -e
set -x

#Load NDK config
source `dirname $0`/ndk_definitions.conf

#Load private config
GULDEN_DEVTOOLS_CONF=${GULDEN_DEVTOOLS_CONF:-developer-tools/private.conf}
if [ -f $GULDEN_DEVTOOLS_CONF ]; then
  source ${GULDEN_DEVTOOLS_CONF}
fi

TARGETS_PATH=$(dirname ${BASH_SOURCE[0]})/build_targets

while :; do
  case $1 in
    --nodepends)
      SKIP_DEPENDS=1
      ;;
    --noconfig)
      SKIP_CONFIG=1
      ;;
    --targets)
      shift
      delimiter=","
      s=$1$delimiter
      NDK_TARGETS=();
      while [[ $s ]]; do
        NDK_TARGETS+=( ${TARGETS_PATH}/"${s%%"$delimiter"*}" );
        s=${s#*"$delimiter"};
      done;
      declare -p NDK_TARGETS

      ;;
    *) # Default case: No more options, so break out of the loop.
      break
  esac
  shift
done


if [ -z "${NDK_TARGETS}" ]; then
  NDK_TARGETS=( ${TARGETS_PATH}/* )
  declare -p NDK_TARGETS
fi

mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs | true

NUM_PROCS=$(getconf _NPROCESSORS_ONLN)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
ASSETS=src/frontend/android/unity_wallet/app/src/main/assets

mkdir ${ASSETS} | true
mkdir ${ASSETS}Mainnet | true
mkdir ${ASSETS}Testnet | true

cp src/data/staticfiltercp ${ASSETS}Mainnet/staticfiltercp
cp src/data/staticfiltercptestnet ${ASSETS}Testnet/staticfiltercp
cp src/data/core-packages.licenses ${ASSETS}/core-packages.licenses

export NDK_ROOT=${NDK_ROOT:-${PWD}/developer-tools/android-ndk-gulden/${NDK_VERSION}}

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

for i in "${NDK_TARGETS[@]}"
do
  source ${i}
  export AR=${TOOLS}/llvm-ar
  export AS=${TOOLS}/$target_host-as
  export CC=${TOOLS}/${clang_prefix}${ANDROID_LEVEL}-clang
  export CXX=${TOOLS}/${clang_prefix}${ANDROID_LEVEL}-clang++
  export LD=${TOOLS}/llvm-ld
  export STRIP=${TOOLS}/llvm-strip
  export RANLIB=${TOOLS}/llvm-ranlib
  export LIBTOOL=libtool
  export CXXFLAGS="-O3 -fPIC -fdata-sections -ffunction-sections -fomit-frame-pointer ${march_flags} -DANDROID_STL=c++_shared -DEXPERIMENTAL_AUTO_CPP_THREAD_ATTACH ${target_opt_cflags}"
  #visibility=hidden
  export CFLAGS=${CXXFLAGS}
  export LDFLAGS="-fPIC -Bsymbolic -Wl,--no-undefined -Wl,--gc-sections"

  if [ -z "$SKIP_DEPENDS" ]
  then
    cd depends
    make HOST=$target_host NO_QT=1 NO_UPNP=1 EXTRA_PACKAGES='qrencode protobufunity' -j ${NUM_PROCS}
    cd ..
  else
    echo Skipping depends
  fi

  export TARGET_OS=android
  mkdir build_android_${target_host} | true
  cd build_android_${target_host}
  if [ -z "$SKIP_CONFIG" ]
  then
    ../configure --prefix=$PWD/../depends/$target_host ac_cv_c_bigendian=no ac_cv_sys_file_offset_bits=$target_bits --host=$target_host --disable-bench --enable-experimental-asm --disable-tests --disable-man --disable-zmq --without-utils --with-libs --without-daemon --with-jni-libs --with-qrencode
  else
    echo Skipping explicit configure
  fi
  make -j ${NUM_PROCS} V=1
  cd ..

  mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib} | true
  cp build_android_${target_host}/src/.libs/lib_unity_jni.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  cp ${LIB_DIR}/${target_host}/libc++_shared.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  ${STRIP} --enable-deterministic-archives --only-keep-debug src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/lib_unity_jni.so -o src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/lib_unity_jni.so.dbg
  ${STRIP} --enable-deterministic-archives --only-keep-debug src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/libc++_shared.so -o src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/libc++_shared.so.dbg
  ${STRIP} --enable-deterministic-archives --strip-unneeded src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/*.so
done
