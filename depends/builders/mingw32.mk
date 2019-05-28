build_mingw32_SHA256SUM = sha256sum.exe
build_mingw32_DOWNLOAD = curl --location --fail --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) -o

mingw32_CC=gcc.exe
mingw32_CXX:=g++.exe

build_mingw32_BINARYEXT=.exe
