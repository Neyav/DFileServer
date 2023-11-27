/*
** ($Header: /var/www/cvsroot/DFileServer/src/ClientConnection.cxx,v 1.27.2.2 2005/10/04 06:55:59 incubus Exp $)
**
** Copyright 2005 Chris Laverdure
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <memory>
#ifdef _WINDOWS
#include <winsock.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif

#ifdef _WINDOWS
#define socklen_t int
#endif

#include "ClientConnection.hxx"

ClientConnection::ClientConnection ()
{

	FileStream = NULL;
	BytesRemaining = BandwidthLeft = 0;
	SendBufferIterator = 0;

	NetworkSocket = -1;

	PollIterator = -1;

	LastBandReset = 0;

	Resource = "";

	LastAction = time (NULL);
}

SOCKET ClientConnection::GetSocket ( void )
{
	return NetworkSocket;
}

char *ClientConnection::GetIP ( void )
{
	return inet_ntoa(SocketStruct.sin_addr);
}

size_t ClientConnection::OpenFile ( char *ArgFilename )
{
	using namespace std;

	ifstream::pos_type FileSize;

	FileStream = new ifstream;

	// Open the file.

	FileStream->open(ArgFilename, ios_base::binary | ios_base::in);

	if (!FileStream->good() || FileStream->eof() || !FileStream->is_open()) 
	{
		FileStream->close();
		delete FileStream;

		return -1; 
	}

	// Determine its size.

	FileStream->seekg(0, ios_base::end);
	FileSize = FileStream->tellg();
	FileStream->seekg(0, ios_base::beg);
	FileSize -= FileStream->tellg();

	BytesRemaining = static_cast<int>(FileSize);

	return BytesRemaining;
}

void ClientConnection::CloseFile ( void )
{

	if ( !FileStream || !FileStream->good() || FileStream->bad() )
		return;

	FileStream->close ();
	delete FileStream;
	FileStream = NULL;

}

char ClientConnection::AcceptConnection ( int ArgSocket )
{
	socklen_t sin_size = sizeof(struct sockaddr_in);

	LastAction = time ( NULL );

	if ( ( NetworkSocket = accept(ArgSocket, (struct sockaddr *)&SocketStruct,
			&sin_size)) == -1 )
	{
		perror("ClientConnection::AcceptConnection -- accept()");
		return -1;
	}

	return 0;
}

void ClientConnection::DisconnectClient ( void )
{
	// Close the socket.
#ifdef _WINDOWS
	if ( closesocket (NetworkSocket) == -1 )
#else
	if ( close (NetworkSocket) == -1 )
#endif
	perror("ClientConnection::DisconnectClient -- close()");

	NetworkSocket = -1;
}

int ClientConnection::SendData ( char *Argstring, int ArgSize )
{
	size_t DataSize;
	size_t DataSent;

	if (ArgSize == 0)
		DataSize = strlen( Argstring );
	else
		DataSize = ArgSize;

	if ( (DataSent = send( NetworkSocket, Argstring, DataSize, 0 )) == -1)
	{
		perror("ClientConnection::SendData -- send()");
		return 0;
	}

	// Set LastAction on a successful send()
	LastAction = time ( NULL );

	return (int) DataSent;
}

size_t ClientConnection::RecvData ( char *Argstring, size_t ArgDataSize )
{
	size_t DataRecv;

	LastAction = time ( NULL );

	if ( (DataRecv = recv( NetworkSocket, Argstring, ArgDataSize-1, 0 )) == -1)
	{
		perror("ClientConnection::RecvData -- recv()");
		return -1;
	}

	// We want to make sure that the std::string is NULL terminated.
	Argstring[DataRecv] = '\0';

	return DataRecv;
}

time_t ClientConnection::SecondsIdle ( void )
{
	time_t IdleTime;

	IdleTime = time ( NULL );

	IdleTime -= LastAction;

	return IdleTime;
}
