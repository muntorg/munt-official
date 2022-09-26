#!/bin/bash
set -e
set -x

#Load NDK config
source `dirname $0`/ndk_definitions.conf

#Load private config
source developer-tools/private.conf

case "$OSTYPE" in
  darwin*)
      PLATFORM=darwin
      ;;
  *)
      PLATFORM=linux
      ;;
esac

export NDK_FILENAME=${NDK_VERSION}-${PLATFORM}.zip

#sha256_file=5dfbbdc2d3ba859fed90d0e978af87c71a91a5be1f6e1c40ba697503d48ccecd

mkdir ./developer-tools/android-ndk | true
cd ./developer-tools/android-ndk
curl -sSO https://dl.google.com/android/repository/${NDK_FILENAME} &> /dev/null
#echo "${sha256_file}  ${NDK_FILENAME}" | shasum -a 256 --check
unzip -qq ${NDK_FILENAME} &> /dev/null
rm ${NDK_FILENAME}
cd -
