mingw64_CFLAGS=-pipe
mingw64_CXXFLAGS=$(mingw64_CFLAGS)

mingw64_release_CFLAGS=-O2
mingw64_release_CXXFLAGS=$(mingw64_release_CFLAGS)

mingw64_debug_CFLAGS=-O1
mingw64_debug_CXXFLAGS=$(mingw64_debug_CFLAGS)

mingw64_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

