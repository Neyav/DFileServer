#!/bin/sh

GCCFLAGS="-Wall -Werror -O"

mkdir ./obj/
mkdir ./obj/contrib/

echo "Native Build:" $GCCFLAGS

echo "Compiling src/DashFileServer.cxx"
g++ $GCCFLAGS -c src/DashFileServer.cxx -o obj/DashFileServer.o
echo "Compiling src/ClientConnection.cxx"
g++ $GCCFLAGS -c src/ClientConnection.cxx -o obj/ClientConnection.o
echo "Compiling src/CPathResolver.cxx"
g++ $GCCFLAGS -c src/CPathResolver.cxx -o obj/CPathResolver.o
echo "Compiling src/DirectoryIndexing.cxx"
g++ $GCCFLAGS -c src/DirectoryIndexing.cxx -o obj/DirectoryIndexing.o
echo "Compiling src/MimeTypes.cxx"
g++ $GCCFLAGS -c src/MimeTypes.cxx -o obj/MimeTypes.o
echo "Compiling src/HTTPHeader.cxx"
g++ $GCCFLAGS -c src/HTTPHeader.cxx -o obj/HTTPHeader.o

echo "Compiling src/contrib/Base64.cpp"
g++ $GCCFLAGS -c src/contrib/Base64.cpp -o obj/contrib/Base64.o


echo "Linking..."
g++ -o DFileServer.x86 obj/DashFileServer.o obj/ClientConnection.o obj/CPathResolver.o obj/DirectoryIndexing.o obj/MimeTypes.o obj/HTTPHeader.o obj/contrib/Base64.o

echo "Striping..."
strip --strip-all DFileServer.x86

rm -rf ./obj/
