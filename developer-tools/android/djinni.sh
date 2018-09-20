#bin/bash

djinni/djinni/src/run \
   --java-out ./src/frontend/android/unity_wallet/app/src/main/java/com/gulden/jniunifiedbackend/ \
   --java-package com.gulden.jniunifiedbackend \
   --java-implement-android-os-parcelable true \
   --ident-java-field mFooBar \
   --cpp-out src/unity/djinni/cpp/ \
   --jni-out src/unity/djinni/jni/ \
   --ident-jni-class NativeFooBar \
   --ident-jni-file NativeFooBar \
   --objc-out src/unity/djinni/objc/ \
   --objc-type-prefix DB \
   --objcpp-out src/unity/djinni/objc/ \
   --idl src/unity/libunity.djinni
