aarch64_ios_SDK=$(shell xcrun --sdk iphoneos --show-sdk-path)
x86_64_ios_SDK=$(shell xcrun --sdk iphonesimulator --show-sdk-path)
ios_SDK=$($(host_arch)_ios_SDK)
$(warning Using ios_SDK $(ios_SDK))

ios_ARCH=$(host_arch)
ifeq ($(host_arch),aarch64)
ios_ARCH=arm64	
endif
$(warning Using ios_ARCH $(ios_ARCH))

ios_CC=clang -arch $(ios_ARCH) --sysroot $(ios_SDK)
ios_CXX=clang++ -arch $(ios_ARCH) --sysroot $(ios_SDK) -stdlib=libc++

#x86_64_ios_CC=clang -willem_x86_arget $(host) --sysroot $(OSX_SDK)
#x86_64_ios_CXX=clang++ -willem_x86_target $(host) --sysroot $(OSX_SDK) -stdlib=libc++

ios_CFLAGS=-pipe
ios_CXXFLAGS=$(ios_CFLAGS)

ios_release_CFLAGS=-O2
ios_release_CXXFLAGS=$(ios_release_CFLAGS)

ios_debug_CFLAGS=-O1
ios_debug_CXXFLAGS=$(ios_debug_CFLAGS)

ios_native_toolchain=native_cctools
