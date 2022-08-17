#!/bin/bash

DJINNI_REPO=https://github.com/mjmacleod/djinni.git

if [ ! -d djinni ]; then
	git clone --single-branch --branch gulden ${DJINNI_REPO}
else
	cd djinni
	git pull origin
	cd ..
fi
rm -rf src/unity/djinni/*
rm -rf src/frontend/android/unity_wallet/app/src/main/java/com/gulden/jniunifiedbackend/*

djinni/src/run \
 --java-out ./src/frontend/android/unity_wallet/app/src/main/java/unity_wallet/jniunifiedbackend/ \
   --java-package unity_wallet.jniunifiedbackend \
   --java-implement-android-os-parcelable true \
   --ident-java-field mFooBar \
   --jni-out src/unity/djinni/jni/ \
   --ident-jni-class NativeFooBar \
   --ident-jni-file NativeFooBar \
   --objc-out src/unity/djinni/objc/ \
   --objc-type-prefix DB \
   --objcpp-out src/unity/djinni/objc/ \
   --node-out src/unity/djinni/node_js/ \
   --node-package unifiedbackend \
   --node-type-prefix NJS \
   --cpp-out src/unity/djinni/cpp/ \
   --idl src/unity/libunity.djinni

mkdir src/unity/djinni/support-lib
cp -rf djinni/support-lib/* src/unity/djinni/support-lib/
