build_mingw64_SHA256SUM = sha256sum.exe
build_mingw64_DOWNLOAD = curl --location --fail --connect-timeout $(DOWNLOAD_CONNECT_TIMEOUT) --retry $(DOWNLOAD_RETRIES) -o

mingw64_CC=gcc.exe
mingw64_CXX:=g++.exe

build_mingw64_BINARYEXT=.exe

