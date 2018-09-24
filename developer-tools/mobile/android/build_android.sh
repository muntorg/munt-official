#! /bin/bash
set -e
set -x


declare -A BUILDMAP
#BUILDMAP["aarch64-linux-android-clang"]="aarch64-linux-android"
#BUILDMAP["x86_64-clang"]="x86_64-linux-android"
BUILDMAP["arm-linux-androideabi-clang"]="arm-linux-androideabi"
#BUILDMAP["x86-clang"]="i686-linux-android"

for i in "${!BUILDMAP[@]}"
do
  toolchain=$i
  target_host=${BUILDMAP[$i]}
  bits=32
  if [[ $toolchain = *"64"* ]]; then
    bits=64
  fi

  export PATH=/opt/android-ndk-gulden/$toolchain/bin:${PATH}
  export AR=$target_host-ar
  export AS=$target_host-clang
  export CC=$target_host-clang
  export CXX=$target_host-clang++
  export LD=$target_host-ld
  export STRIP=$target_host-strip
  export CXXFLAGS="-fPIC -fdata-sections -ffunction-sections -fomit-frame-pointer -mthumb -DEXPERIMENTAL_AUTO_CPP_THREAD_ATTACH"
  #visibility=hidden
  export CFLAGS=${CXXFLAGS}
  export LDFLAGS="-fPIC -pie -Bsymbolic -Wl,--no-undefined -Wl,--gc-sections"

  cd depends
  make HOST=$target_host NO_QT=1 NO_UPNP=1 EXTRA_PACKAGES=qrencode -j $(nproc)
  cd ..

  mkdir build_android_${target_host} | true
  cd build_android_${target_host}
  ../autogen.sh
  ../configure --prefix=$PWD/../depends/$target_host ac_cv_c_bigendian=no ac_cv_sys_file_offset_bits=$bits --host=$target_host --disable-bench --enable-experimental-asm --disable-tests --disable-man --disable-zmq --without-utils --with-libs --without-daemon --with-jni-libs --with-qrencode
  make -j $(nproc) V=1
  cd ..
done


mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs | true
mkdir src/frontend/android/unity_wallet/app/src/main/jniLibs/armeabi-v7a | true

cp build_android_arm-linux-androideabi/src/.libs/libgulden_unity_jni.so src/frontend/android/unity_wallet/app/src/main/jniLibs/armeabi-v7a/
cp /opt/android-ndk-gulden/arm-linux-androideabi-clang/arm-linux-androideabi/lib/armv7-a/libc++_shared.so src/frontend/android/unity_wallet/app/src/main/jniLibs/armeabi-v7a/
/opt/android-ndk-gulden/arm-linux-androideabi-clang/arm-linux-androideabi/bin/strip --strip-unneeded src/frontend/android/unity_wallet/app/src/main/jniLibs/armeabi-v7a/*.so
