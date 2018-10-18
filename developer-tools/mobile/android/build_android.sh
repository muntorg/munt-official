#! /bin/bash
set -e
set -x

mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs | true

NUM_PROCS=$(getconf _NPROCESSORS_ONLN)

for i in $( dirname ${BASH_SOURCE[0]} )/build_targets/*
do
  source ${i}
  export PATH=${PWD}/developer-tools/android-ndk-gulden/$toolchain/bin:${PATH}
  export AR=$target_host-ar
  export AS=$target_host-clang
  export CC=$target_host-clang
  export CXX=$target_host-clang++
  export LD=$target_host-ld
  export STRIP=$target_host-strip
  export RANLIB=$target_host-ranlib
  export CXXFLAGS="-fPIC -fdata-sections -ffunction-sections -fomit-frame-pointer ${march_flags} -DEXPERIMENTAL_AUTO_CPP_THREAD_ATTACH"
  #visibility=hidden
  export CFLAGS=${CXXFLAGS}
  export LDFLAGS="-fPIC -Bsymbolic -Wl,--no-undefined -Wl,--gc-sections"

  cd depends
  make HOST=$target_host NO_QT=1 NO_UPNP=1 EXTRA_PACKAGES=qrencode -j ${NUM_PROCS}
  cd ..

  mkdir build_android_${target_host} | true
  cd build_android_${target_host}
  ../autogen.sh
  ${RANLIB} ../depends/$target_host/lib/*.a
  ../configure --prefix=$PWD/../depends/$target_host ac_cv_c_bigendian=no ac_cv_sys_file_offset_bits=$target_bits --host=$target_host --disable-bench --enable-experimental-asm --disable-tests --disable-man --disable-zmq --without-utils --with-libs --without-daemon --with-jni-libs --with-qrencode
  make -j ${NUM_PROCS}
  cd ..

  mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib} | true
  cp build_android_${target_host}/src/.libs/libgulden_unity_jni.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  cp ${PWD}/developer-tools/android-ndk-gulden/${toolchain}/${target_host}/${lib_dir}/libc++_shared.so src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/
  ${PWD}/developer-tools/android-ndk-gulden/${toolchain}/${target_host}/bin/strip --strip-unneeded src/frontend/android/unity_wallet/app/src/main/jniLibs/${jni_lib}/*.so
done
