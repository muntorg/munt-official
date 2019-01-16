aarch64_ios_SDK=$(shell xcrun --sdk iphoneos --show-sdk-path)
x86_64_ios_SDK=$(shell xcrun --sdk iphonesimulator --show-sdk-path)
ios_SDK=$($(host_arch)_ios_SDK)

ios_ARCH=$(host_arch)
ifeq ($(host_arch),aarch64)
ios_ARCH=arm64
endif

aarch64_ios_SYSTEM=apple-ios
x86_64_ios_SYSTEM=apple-darwin
ios_SYSTEM=$($(host_arch)_ios_SYSTEM)

ios_TARGET=$(ios_ARCH)-$(ios_SYSTEM)
ios_HOST=$(host_arch)-$(ios_SYSTEM)

ios_CC=clang -target $(ios_TARGET) -isysroot $(ios_SDK)
ios_CXX=clang++ -target $(ios_TARGET) -isysroot $(ios_SDK) -stdlib=libc++

ios_CFLAGS=-pipe
ios_CXXFLAGS=$(ios_CFLAGS)

ios_release_CFLAGS=-O2
ios_release_CXXFLAGS=$(ios_release_CFLAGS)

ios_debug_CFLAGS=-O1
ios_debug_CXXFLAGS=$(ios_debug_CFLAGS)

ios_native_toolchain=
