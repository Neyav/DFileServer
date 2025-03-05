# Build parameters
CC = c++
STRIP = strip
RM = rm

CFLAGS = -O2 -std=c++17 -D_DFS_USE_OPENSSL
LINKERFLAGS = -lssl -lcrypto

# The default build target.
dfileserver: CPathResolver.o DirectoryIndexing.o ClientConnection.o HTTPHeader.o DashFileServer.o MimeTypes.o Base64.o InterProcessMessaging.o Networking.o NetworkThread.o TCPInterface.o
	$(CC) -o dfileserver CPathResolver.o DirectoryIndexing.o ClientConnection.o HTTPHeader.o DashFileServer.o MimeTypes.o Base64.o InterProcessMessaging.o Networking.o NetworkThread.o TCPInterface.o $(LINKERFLAGS)
	$(STRIP) --strip-all dfileserver

CPathResolver.o: src/CPathResolver.cxx
	$(CC) $(CFLAGS) -o CPathResolver.o -c src/CPathResolver.cxx
	
DirectoryIndexing.o: src/DirectoryIndexing.cxx
	$(CC) $(CFLAGS) -o DirectoryIndexing.o -c src/DirectoryIndexing.cxx
	
ClientConnection.o: src/ClientConnection.cxx
	$(CC) $(CFLAGS) -o ClientConnection.o -c src/ClientConnection.cxx
	
HTTPHeader.o: src/HTTPHeader.cxx
	$(CC) $(CFLAGS) -o HTTPHeader.o -c src/HTTPHeader.cxx
	
DashFileServer.o: src/DashFileServer.cxx
	$(CC) $(CFLAGS) -o DashFileServer.o -c src/DashFileServer.cxx
	
MimeTypes.o: src/MimeTypes.cxx
	$(CC) $(CFLAGS) -o MimeTypes.o -c src/MimeTypes.cxx
	
Base64.o: src/contrib/Base64.cpp
	$(CC) $(CFLAGS) -o Base64.o -c src/contrib/Base64.cpp
	
InterProcessMessaging.o: src/InterProcessMessaging.cxx
	$(CC) $(CFLAGS) -o InterProcessMessaging.o -c src/InterProcessMessaging.cxx
	
Networking.o: src/Networking.cxx
	$(CC) $(CFLAGS) -o Networking.o -c src/Networking.cxx

NetworkThread.o: src/NetworkThread.cxx
	$(CC) $(CFLAGS) -o NetworkThread.o -c src/NetworkThread.cxx

TCPInterface.o: src/TCPInterface.cxx
	$(CC) $(CFLAGS) -o TCPInterface.o -c src/TCPInterface.cxx

clean:
	$(RM) *.o

clean-all:
	$(RM) *.o dfileserver
