#bin/bash

git clone --single-branch --branch gulden https://github.com/mjmacleod/djinni.git
rm -rf src/unity/djinni/*

djinni/src/run \
 --java-out ./src/frontend/android/unity_wallet/app/src/main/java/com/gulden/jniunifiedbackend/ \
   --java-package com.gulden.jniunifiedbackend \
   --java-implement-android-os-parcelable true \
   --ident-java-field mFooBar \
   --jni-out src/unity/djinni/jni/ \
   --ident-jni-class NativeFooBar \
   --ident-jni-file NativeFooBar \
   --objc-out src/unity/djinni/objc/ \
   --objc-type-prefix DB \
   --objcpp-out src/unity/djinni/objc/ \
   --node-out src/unity/djinni/node_js/ \
   --node-package guldenunifiedbackend \
   --node-type-prefix NJS \
   --cpp-out src/unity/djinni/cpp/ \
   --idl src/unity/libunity.djinni

