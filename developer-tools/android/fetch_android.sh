#!/bin/bash
set -e
set -x

export NDK_VERSION=android-ndk-r18-beta2
export NDK_FILENAME=${NDK_VERSION}-linux-x86_64.zip

#sha256_file=5dfbbdc2d3ba859fed90d0e978af87c71a91a5be1f6e1c40ba697503d48ccecd

#mkdir /opt/android-ndk-gulden | true
#cd /opt/android-ndk-gulden && curl -sSO https://dl.google.com/android/repository/${NDK_FILENAME} &> /dev/null
#echo "${sha256_file}  ${NDK_FILENAME}" | shasum -a 256 --check
#unzip -qq ${NDK_FILENAME} &> /dev/null
#rm ${NDK_FILENAME}

/opt/android-ndk-gulden/${NDK_VERSION}/build/tools/make-standalone-toolchain.sh --use-llvm --stl=libc++ --platform=21 --toolchain=arm-linux-androideabi-clang --install-dir=/opt/android-ndk-gulden/arm-linux-androideabi-clang
/opt/android-ndk-gulden/${NDK_VERSION}/build/tools/make-standalone-toolchain.sh --use-llvm --stl=libc++ --platform=21 --toolchain=x86-clang --install-dir=/opt/android-ndk-gulden/x86-clang
/opt/android-ndk-gulden/${NDK_VERSION}/build/tools/make-standalone-toolchain.sh --use-llvm --stl=libc++ --platform=21 --toolchain=x86_64-clang --install-dir=/opt/android-ndk-gulden/x86_64-clang
/opt/android-ndk-gulden/${NDK_VERSION}/build/tools/make-standalone-toolchain.sh --use-llvm --stl=libc++ --platform=21 --toolchain=aarch64-linux-android-clang --install-dir=/opt/android-ndk-gulden/aarch64-linux-android-clang

