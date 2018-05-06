# Build parameters
CC = c++
STRIP = strip
RM = rm

CFLAGS = -O2
LINKERFLAGS =

# The default build target. This is the console test engine.
dfileserver: CPathResolver.o DirectoryIndexing.o ClientConnection.o HTTPHeader.o DashFileServer.o Mimetypes.o Base64.o
	$(CC) -o dfileserver CPathResolver.o DirectoryIndexing.o ClientConnection.o HTTPHeader.o DashFileServer.o MimeTypes.o Base64.o $(LINKERFLAGS)
	$(STRIP) --strip-all dfileserver

CPathResolver.o: src/CPathResolver.cxx
	$(CC) $(CFLAGS) -o CPathResolver.o -c src/CPathResolver.cxx
	
DirectoryIndexing.o: src/DirectoryIndexing.cxx
	$(CC) $(CFLAGS) -o DirectoryIndexing.o -c src/DirectoryIndexing.cxx
	
ClientConnection.o: src/ClientConnection.cxx
	$(CC) $(CFLAGS) -o ClientConnection.o -c src/ClientConnection.cxx
	
HTTPHeader.o: src/HTTPHeader.cxx
	$(CC) $(CFLAGS) -o HTTPHeader.o -c src/HTTPHeader.cxx
	
DashFileServer.o: src/DashFileServer.o
	$(CC) $(CFLAGS) -o DashFileServer.o -c src/DashFileServer.cxx
	
Mimetypes.o: src/MimeTypes.o
	$(CC) $(CFLAGS) -o MimeTypes.o -c src/MimeTypes.cxx
	
Base64.o: src/contrib/Base64.cpp
	$(CC) $(CFLAGS) -o Base64.o -c src/contrib/Base64.cpp

clean:
	$(RM) *.o

clean-all:
	$(RM) *.o dfileserver
