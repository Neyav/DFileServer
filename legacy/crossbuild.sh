#!/bin/sh

GCCFLAGS="-Wall -Werror -O3 -D_WINDOWS"

mkdir ./obj/
mkdir ./obj/contrib/

echo "Win32 Cross-Build:" $GCCFLAGS

echo "Compiling src/DashFileServer.cxx"
mingw32-g++ $GCCFLAGS -c src/DashFileServer.cxx -o obj/DashFileServer.o
echo "Compiling src/ClientConnection.cxx"
mingw32-g++ $GCCFLAGS -c src/ClientConnection.cxx -o obj/ClientConnection.o
echo "Compiling src/CPathResolver.cxx"
mingw32-g++ $GCCFLAGS -c src/CPathResolver.cxx -o obj/CPathResolver.o
echo "Compiling src/DirectoryIndexing.cxx"
mingw32-g++ $GCCFLAGS -c src/DirectoryIndexing.cxx -o obj/DirectoryIndexing.o
echo "Compiling src/MimeTypes.cxx"
mingw32-g++ $GCCFLAGS -c src/MimeTypes.cxx -o obj/MimeTypes.o
echo "Compiling src/contrib/Base64.cpp"
mingw32-g++ $GCCFLAGS -c src/contrib/Base64.cpp -o obj/contrib/Base64.o

echo "Linking..."
mingw32-g++ -o DFileServer.exe obj/DashFileServer.o obj/ClientConnection.o obj/CPathResolver.o obj/DirectoryIndexing.o obj/MimeTypes.o obj/contrib/Base64.o -lwsock32

echo "Stripping..."
mingw32-strip --strip-all DFileServer.exe

rm -rf ./obj/

