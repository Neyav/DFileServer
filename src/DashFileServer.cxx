/*
** ($Header: /var/www/cvsroot/DFileServer/src/DashFileServer.cxx,v 1.61.2.19 2005/10/23 19:51:28 incubus Exp $)
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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifdef _WINDOWS
#include <winsock.h>
#include <io.h>
#include "contrib/fakepoll.h"
#else
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif
#include <list>
#include <string>

#include "ClientConnection.hxx"
#include "CPathResolver.hxx"
#include "DirectoryIndexing.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"

#ifndef INFTIM
#define INFTIM -1
#endif

using namespace std;

char MAJORVERSION[] = "1";
char MINORVERSION[] = "1";
char PATCHVERSION[] = "3";
bool ServerLockdown = false;
bool ServerShutdown = false;
string ConfigurationBasicCredentials = "";
int ActiveConnections = 0;

void InitateServerShutdown ( int ArgSignal )
{
	printf("Recieved Signal %i\n", ArgSignal );

	// First lockdown the server and close when all the connections are done.
	// If the controlling user insists, then terminate immediately.
	if ( ServerLockdown || ActiveConnections == 0 )
		ServerShutdown = true;
	else
	{
		ServerLockdown = true;

		printf( "Initating Server Lockdown, waiting on %i Client(s).\n", ActiveConnections );
	}
}

// This function contributed by Markus Thiele
std::string i2hex( unsigned int n, unsigned int minWidth, bool upperCase ) 
{
	std::string out; 
	{
		char charA = upperCase ? 'A' : 'a'; 
		char digit; 
		while( n != 0 || out.size() < minWidth ) 
		{
			digit = n % 16; 
			n /= 16; 

			if( digit < 10 ) 
			{
				digit = '0' + digit;
			} 
			else 
			{
			digit = charA + digit - 10;
			}
			
			out.insert( out.begin(), digit ); 
		}
	}

   return out;
} 

void URLEncode( string &ArgBuffer )
{
	unsigned int iterator;
	
	for (iterator = 0; iterator < ArgBuffer.size(); iterator++)
	{
		// Should it be URL encoded?
		if ( (ArgBuffer[iterator] < 40 || ArgBuffer[iterator] > 57) && 
			(ArgBuffer[iterator] < 65 || ArgBuffer[iterator] > 90) &&
				(ArgBuffer[iterator] < 97 || ArgBuffer[iterator] > 122) )
		{
			// URL encode this character
			string URLEncodedCharacter;

			URLEncodedCharacter = "%" + i2hex( (unsigned int) 
					( (unsigned char) ArgBuffer[iterator] ), 2, true );

			// Swap the character out with the URL encoded version.
			ArgBuffer.replace (iterator, 1, URLEncodedCharacter);
		}
	}

}

void ParseURLEncoding( char *ArgBuffer )
{
	int BufferLength = strlen(ArgBuffer);
	int iterator1;
	int iterator2 = 0;

	// Iterator through the entire buffer.
	for (iterator1 = 0; iterator1 < BufferLength; iterator1++)
	{
		ArgBuffer[iterator2] = ArgBuffer[iterator1]; // Copy it, character by character.

		// Check for a % sign. They indicates URL encoding.
		if ( ArgBuffer[iterator1] == '%' )
		{
			if ( (iterator1 + 2) < BufferLength ) // Make sure we don't go past the end.
			{
				char HexCode[2];
				char *CharPointer;
				long Value;

				CharPointer = &ArgBuffer[iterator1 + 1];

				strncpy (HexCode, CharPointer, 2);

				if ( (Value = strtol(HexCode, NULL, 16)) != 0 && Value < 256) // It's a valid hex code.
				{
					// Replace the value at iterator2.
					ArgBuffer[iterator2] = (char) Value;

					// Increment iterator1 by 2
					iterator1 += 2;
				}

			}

		} // [END] -- if ( ArgBuffer... )

		iterator2++;
	} // [END] -- for ( ...

	// Null terminate it.
	ArgBuffer[iterator2] = '\0';

}

int InitalizeNetwork ( int ArgPort, int ArgBacklog )
{
	struct sockaddr_in ListenAddr;
	int NetworkSocket;
	const char yes = 1;	
#ifdef _WINDOWS
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
	{
		perror("InitalizeNetwork -- WSAStartup()");
		return -1;
	}
#endif

	// Grab the master socket.
	if ( ( NetworkSocket = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1)
	{
		perror("InitalizeNetwork -- socket()");
		return -1;
	}

	// Clear the socket incase it hasn't been properly closed so that we may use it.
	if ( setsockopt( NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
	{
		perror("InitalizeNetwork -- setsockopt()");
		return -1;
	}

	// Set the listening network struct up.
	ListenAddr.sin_family = AF_INET;
	ListenAddr.sin_port = htons(ArgPort);
	ListenAddr.sin_addr.s_addr = INADDR_ANY;
	memset(&(ListenAddr.sin_zero), '\0', 8);

	// Bind the socket to the listening network struct.
	if ( bind( NetworkSocket, (struct sockaddr *)&ListenAddr, 
					sizeof( struct sockaddr ) ) == -1 )
	{
		perror("InitalizeNetwork -- bind()");
		return -1;
	}

	// Start listening
	if ( listen( NetworkSocket, ArgBacklog ) == -1 )
	{
		perror("InitalizeNetwork -- listen()");
		return -1;
	}

	// We were successful, return the socket.
	return NetworkSocket;
}

void IncomingConnection ( int ArgSocket, int *ArgHighestIterator, struct pollfd ArgPoll[], 
								list<ClientConnection> *ArgList )
{
	ClientConnection IncomingClient;

	// Accept the connection and break if there was an error.
	if ( IncomingClient.AcceptConnection( ArgSocket ) == -1 )
		return;

	// If we're in serverlockdown, close the connection right away.
	if ( ServerLockdown )
	{
		IncomingClient.DisconnectClient();

		return;
	}

	ActiveConnections++;
	
	// Find the next free slot.
	for ( int x = 0; x < 2048; x++ )
		if ( ArgPoll[x].fd <= 0 )
		{
			IncomingClient.PollIterator = x;
			break;
		}

	if (IncomingClient.PollIterator > *ArgHighestIterator)
		*ArgHighestIterator = IncomingClient.PollIterator;

	// Add it to the Poll Structure so it gets polled.
	ArgPoll[IncomingClient.PollIterator].fd = IncomingClient.GetSocket();
	ArgPoll[IncomingClient.PollIterator].events = (POLLIN | POLLOUT);

	// Add it to the linked list.
	ArgList->push_back ( IncomingClient );
	
}

list<ClientConnection>::iterator
TerminateConnection ( int ArgSocket, list<ClientConnection>::iterator ArgClient, int *ArgHighestIterator, 
						struct pollfd ArgPoll[], list <ClientConnection> *ArgList )
{
	list<ClientConnection>::iterator ConnectionListIterator;
	list<ClientConnection>::iterator PreviousElement;
	int NewHighestPollIterator = 0;
	int OldPollIterator = 0;

	ActiveConnections--;

	// If serverlockdown was established and this is the last connection, do a complete shutdown.
	if ( ServerLockdown )
	{
		if ( ActiveConnections == 0 )
			ServerShutdown = true;
		else
			printf("Client Disconnected; %i remaining...\n", ActiveConnections);
	}

	if ( ArgClient->FileStream )
		ArgClient->CloseFile();

	// Keep track of the old iterator. Previously I just used ArgClient->PollIterator in this function, but that doesn't
	// appear safe to do after the ArgList->erase command. It's causing crashes on some platforms (i.e. FreeBSD 6.x )
	OldPollIterator = ArgClient->PollIterator;

	// Remove this client from the Poll structure so it doesn't get polled.
	ArgPoll[OldPollIterator].fd = 0;
	ArgPoll[OldPollIterator].events = 0;

	// Close the socket and possibly do some other Misc shutdown stuff.
        ArgClient->DisconnectClient();

	// Remove the client from the linked list.
	PreviousElement = ArgList->erase ( ArgClient );

	// This Client had the highest descriptor..
	if ( OldPollIterator == *ArgHighestIterator )
	{
		// Traverse through the active connections to find the new highest file descriptor
		for (ConnectionListIterator = ArgList->begin();
			ConnectionListIterator != ArgList->end();
				ConnectionListIterator++)
		{
			if ( ConnectionListIterator->PollIterator > NewHighestPollIterator )
				NewHighestPollIterator = ConnectionListIterator->PollIterator;
		}

		*ArgHighestIterator = NewHighestPollIterator;
	}

	return PreviousElement--;
}

char *TimeAndDate ( void )
{
	time_t 		CurrentTime;
	struct tm	*LocalTime;
	static char Output[100];

	CurrentTime = time(NULL);
	LocalTime = localtime(&CurrentTime);

	snprintf(Output, sizeof(Output), "[%4d/%02d/%02d %02d:%02d:%02d]", LocalTime->tm_year+1900, LocalTime->tm_mon+1, LocalTime->tm_mday, 
								LocalTime->tm_hour, LocalTime->tm_min, LocalTime->tm_sec ); 
	return Output;
}

char LocateResource ( string Resource, list<ClientConnection>::iterator ArgClient, char *DstResource, char *DstResourceType )
{
	FILE *FileStream;
	char FieldName[31];
	char ResolveIdentifier[151];
	char RealIdentifier[151];
	char IPRange[17];
	char SignOfBadEngineering[151];

	// No resource defined.
	if ( Resource.empty() )
	{
		return -1;
	}

	if ( (FileStream = fopen("resource.cfg", "r") ) == NULL )
	{
		// Resource.cfg file couldn't be opened, so just make a dummy index for where we are.
		try
		{
			CPathResolver PathResolver;

			PathResolver.Add ("/", ".");  // Make the root of the tree our current location.

			strcpy ( DstResource, PathResolver( Resource ).c_str() );
			strcpy ( DstResourceType, "redirect");

			return 1;
		}
		catch( CPathResolver::exn_t exn )
		{ // Invalid path.
			return -1;
		}
	}

	// Find the resource identifier.
	while (!feof(FileStream))
	{
		fscanf( FileStream, "%30s %150s %150s", FieldName, ResolveIdentifier, SignOfBadEngineering );

		if ( strcmp( FieldName, "RESOURCE" ) == 0) // A resource.
		{
			if ( strcmp( SignOfBadEngineering, "redirect" ) == 0 ) // redirect
			{
				if ( strncmp( ResolveIdentifier, Resource.c_str(), strlen( ResolveIdentifier ) ) == 0 )
				{
					strcpy ( DstResourceType, SignOfBadEngineering );
					break;
				}
			}

			if ( strcmp( ResolveIdentifier, Resource.c_str() ) == 0) // A match.
			{
				strcpy ( DstResourceType, SignOfBadEngineering );	
				break;
			}
		}
	}

	// Find the first IP match.
	while (!feof(FileStream))
	{
		fscanf( FileStream, "%30s %16s %150s", FieldName, IPRange, RealIdentifier );

		// Slipped past into another resource without a match.
		if ( strcmp( FieldName, "RESOURCE" ) == 0)
		{
			fclose ( FileStream );
			return -1;
		}

		// We've got a match.
		if ( 	( strncmp( ArgClient->GetIP(), IPRange, strlen( IPRange ) ) == 0 ) || // Direct match.
			( strcmp( "x.x.x.x", IPRange ) == 0 ) ) // Wildcarded.
		{
			fclose ( FileStream );

			if ( strcmp( DstResourceType, "redirect" ) == 0 ) // Resolve actual path.
			{
				try 
				{

					CPathResolver PathResolver;
				
					PathResolver.Add ( ResolveIdentifier, RealIdentifier );

					strcpy ( DstResource, PathResolver( Resource ).c_str() );

				}
				catch( CPathResolver::exn_t exn )
				{
					// Illegal path.
					return -1;
				}
			}
			else  // Direct Resource.
				strcpy ( DstResource, RealIdentifier );

			return 1;
		}
	}

	// Reached end of file without a match.
	fclose ( FileStream );
	return -1;

}

void Buffer404 ( list<ClientConnection>::iterator ArgClient )
{
	// This fills the buffer with the 404 error.

	ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Resource not found</TITLE></HEAD><BODY><H1>Resource not found</H1>";
	ArgClient->SendBuffer += "The resource you were trying to locate doesn't exist on this server.<br><br><HR>";
	ArgClient->SendBuffer += "<I>DashFileServer [Version " + string(MAJORVERSION) + "." + string(MINORVERSION) + "." + string(PATCHVERSION) + "]</I>";
	ArgClient->SendBuffer += "</BODY></HTML>";
}

void Buffer401 ( list<ClientConnection>::iterator ArgClient )
{
	// This fills the buffer with the 401 error.
         
	ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Authorization Required</TITLE></HEAD><BODY><H1>Authorization Required</H1>";
	ArgClient->SendBuffer += "The resource you were trying to locate requires authorization on this server.<br><br><HR>";
	ArgClient->SendBuffer += "<I>DashFileServer [Version " + string(MAJORVERSION) + "." + string(MINORVERSION) + "." + string(PATCHVERSION) + "]</I>";
	ArgClient->SendBuffer += "</BODY></HTML>";
}

int main( int argc, char *argv[] )
{
   FILE *LogFilePointer = NULL;
   int ServerSocket;
   int HighestPollIterator;
   int ConfigurationPort = 2000;
   int ConfigurationBackLog = 100;
   int ConfigurationMaxConnections = 0;
   int ConfigurationMaxBandwidth = 0;
   int ConfigurationShowConnections = 0;
#ifndef _WINDOWS
   int ConfigurationSetUID = 0;
   string ConfigurationChrootFolder;
#endif
   struct pollfd PollStruct[2048];
   list<ClientConnection> ConnectionList;

#ifndef _WINDOWS
   // Catch SIGPIPE and ignore it.
   signal( SIGPIPE, SIG_IGN );
#endif
   signal( SIGTERM, InitateServerShutdown );
   signal( SIGINT, InitateServerShutdown );

   printf("DashFileServer Version %s.%s.%s\n", MAJORVERSION, MINORVERSION, PATCHVERSION);
   printf("             Copyright 2005 Chris Laverdure\n");
   printf("-------------------------------------------\n");

   // Parse for command line parameters.
   for (int x = 1; x < argc; x++)
   {
	if ( strcasecmp( "-port", argv[x] ) == 0 )
	{
		x++;

		ConfigurationPort = atoi(argv[x]);
		printf("Configuration: Port -> %i\n", ConfigurationPort);
	}
	else if ( strcasecmp( "-backlog", argv[x] ) == 0 )
	{
		x++;

		ConfigurationBackLog = atoi(argv[x]);
		printf("Configuration: BackLog -> %i\n", ConfigurationBackLog);
	}
	else if ( strcasecmp( "-maxconnections", argv[x] ) == 0 )
	{
		x++;

		ConfigurationMaxConnections = atoi(argv[x]);
		printf("Configuration: MaxConnections -> %i\n", ConfigurationMaxConnections);
	}
	else if ( strcasecmp( "-maxbandwidth", argv[x] ) == 0 )
	{
		x++;

		ConfigurationMaxBandwidth = atoi(argv[x]);
		if ( ConfigurationMaxBandwidth > 1048576 )
			printf("Configuration: MaxBandwidth -> %.1f MB/s\n", (float) ConfigurationMaxBandwidth / 1048576);
		else if ( ConfigurationMaxBandwidth > 1024 )
			printf("Configuration: MaxBandwidth -> %.1f KB/s\n", (float) ConfigurationMaxBandwidth / 1024);
		else
			printf("Configuration: MaxBandwidth -> %i B/s\n", ConfigurationMaxBandwidth);
	}
	else if ( strcasecmp( "-showconnections", argv[x] ) == 0 )
	{
		ConfigurationShowConnections = 1;

		printf("Configuration: Show Connections\n");
	}
	else if ( strcasecmp( "-log", argv[x] ) == 0 )
	{
		x++;

		// Open the log file.
		LogFilePointer = fopen( argv[x], "w+");

		printf("Configuration: LogFile -> %s\n", argv[x] );
	}
	else if ( strcasecmp( "-requireauth", argv[x] ) == 0 )
	{
		x++;

		ConfigurationBasicCredentials = string ( argv[x] );

		printf("Configuration: Required Authentication Credentials -> %s\n", argv[x] );
	}
#ifndef _WINDOWS
	else if ( strcasecmp( "-setuid", argv[x] ) == 0 )
	{
		x++;

		ConfigurationSetUID = atoi(argv[x]);
		printf("Configuration: Set UID to %i\n", ConfigurationSetUID );
	}
	else if ( strcasecmp( "-chroot", argv[x] ) == 0 )
	{
		x++;

		ConfigurationChrootFolder = string(argv[x]);
		printf("Configuration: chroot to %s\n", argv[x]);
	}
	else if ( strcasecmp( "-background", argv[x] ) == 0 )
	{
		printf("Configuration: Dropping to background...\n");

		if ( fork() != 0 )
		{
			printf("Configuration: Child process spawned.\n");
			exit (0);
		}

		fclose(stdout); // We have no need for this.
	}
#endif
   }

   memset( PollStruct, '\0', sizeof(PollStruct) );

   if ( (ServerSocket = InitalizeNetwork( ConfigurationPort, ConfigurationBackLog )) == -1 )
   {
	printf("CRITICAL ERROR: Couldn't initalize listening socket on port %i\n", ConfigurationPort );
	exit(-1);
   }

   printf("Listening on port: %i\n", ConfigurationPort );

#ifndef _WINDOWS

   // chroot
   if ( !ConfigurationChrootFolder.empty() )
   {
	// First we attempt to CHROOT to that folder.
	if ( chroot( ConfigurationChrootFolder.c_str() ) == -1 )
	{
		perror("ConfigurationChrootFolder");
		exit(-1);
	}

	// Then we change the current directory to /
	if ( chdir("/") == -1 )
	{
		perror("ConfigurationChrootFolder");
		exit(-1);
	}
   }

   // Set UID
   if ( ConfigurationSetUID > 0 )
   {
	if ( setuid( ConfigurationSetUID ) == -1 )
	{
		perror("ConfigurationSetUID");
		exit(-1);
	}
   }
#endif

   PollStruct[0].fd = ServerSocket;
   PollStruct[0].events = POLLIN;
   HighestPollIterator = 0;

   // Main Program Loop
   while (1)
   {
	list<ClientConnection>::iterator ConnectionListIterator;

	poll ( PollStruct, HighestPollIterator + 1, INFTIM );

	// Do we have an incoming connection?
	if ( ( PollStruct[0].revents & POLLIN ) && ( ActiveConnections != ConfigurationMaxConnections || !ConfigurationMaxConnections ) )
	{
		// Handle the incoming connection.
		IncomingConnection ( ServerSocket, &HighestPollIterator, PollStruct, &ConnectionList );
	}

	// Traverse the linked list searching for Sockets that are ready.
	for (ConnectionListIterator = ConnectionList.begin();
		ConnectionListIterator != ConnectionList.end();
			ConnectionListIterator++)
	{
		// This socket has incoming data.
		if ( PollStruct[ConnectionListIterator->PollIterator].revents & POLLIN )
		{
			char DataBuffer[500]; // 500 should be big enough.
			ssize_t DataRecved;

			if ( ( DataRecved = ConnectionListIterator->RecvData( DataBuffer, sizeof( DataBuffer ) ) ) < 1 )
			{ // Disconnection or error. Terminate client.

				ConnectionListIterator = TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
												PollStruct, &ConnectionList );
			}
			else
			{ // Incoming data.
				ConnectionListIterator->BrowserRequest.ImportHeader ( string( DataBuffer ) );

				// Find what resource this person is after.
				ConnectionListIterator->Resource = ConnectionListIterator->BrowserRequest.AccessPath;
			}			

		} // [/END] Incoming Data on socket.
		
		if ( ConnectionListIterator != ConnectionList.end() && !ConnectionListIterator->Resource.empty() && 
				PollStruct[ConnectionListIterator->PollIterator].revents & POLLOUT )
		{

			if ( !ConnectionListIterator->FileStream && ConnectionListIterator->SendBuffer.empty() )
			{
				char Resource[151];
				char ResourceType[151];
				int ResourceSize = 0;

				// We need to locate this resource.
				if ( LocateResource ( ConnectionListIterator->Resource, ConnectionListIterator, Resource, ResourceType ) == -1 )
				{ // Resource wasn't found.
					Buffer404( ConnectionListIterator );
					ResourceSize = ConnectionListIterator->BytesRemaining = ConnectionListIterator->SendBuffer.size();
					strcpy (Resource, "--404--");
					strcpy (ResourceType, "text/html");

					continue;
				}

				ParseURLEncoding( Resource );

				// Is it a redirect?
				if ( strcmp( ResourceType, "redirect" ) == 0 )
				{
					char direrror;

					direrror = GenerateFolderIndex( ConnectionListIterator->Resource, Resource, ConnectionListIterator->SendBuffer );

					if (direrror == -1)
					{ // File/Folder doesn't exist
						Buffer404( ConnectionListIterator );
						ResourceSize = ConnectionListIterator->BytesRemaining = ConnectionListIterator->SendBuffer.size();
						strcpy (Resource, "--404--");
						strcpy (ResourceType, "text/html");
					}
					else if (direrror)
					{ // It generated an index for us.

						ConnectionListIterator->SendBufferIterator = 0;

						ResourceSize = ConnectionListIterator->BytesRemaining = ConnectionListIterator->SendBuffer.size();

						strcpy (ResourceType, "text/html");
					}
					else
					{ // The location points to a file, not a folder.
						// Determine the mimetype by the filename's extention
						strcpy ( ResourceType, ReturnMimeType ( Resource ) );
					}
				}
				else if ( strcmp( ResourceType, "-" ) == 0 )
				{ // User didn't specify a mimetype. Determine by extension.
					strcpy ( ResourceType, ReturnMimeType ( Resource ) );
				}

				if ( ConfigurationShowConnections )
				{
					printf("%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionListIterator->GetIP(), 
									ConnectionListIterator->Resource.c_str(), Resource );
				}

				if ( LogFilePointer != NULL )
				{
					fprintf( LogFilePointer, "%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionListIterator->GetIP(),
									ConnectionListIterator->Resource.c_str(), Resource );

					// Make sure it gets written to disk.
					fflush( LogFilePointer );
				}

				// If there is no sendbuffer, try to open the file, and if you can't, terminate the connection.
				if ( ConnectionListIterator->SendBuffer.empty() && 
					(( ResourceSize = ConnectionListIterator->OpenFile( Resource ) ) == -1) )
				{ // File couldn't be opened.
					ConnectionListIterator = TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
													PollStruct, &ConnectionList );
				}
				else
				{				
					// Send the HTTP header.
					char Buffer[500];

					ConnectionListIterator->ServerResponse.AccessType = "HTTP/1.1";
					ConnectionListIterator->ServerResponse.AccessPath = "200";
					ConnectionListIterator->ServerResponse.AccessProtocol = "OK";

					if ( !ConfigurationBasicCredentials.empty() ) // Authentication Required
					{
						if ( ConnectionListIterator->BrowserRequest.GetValue( "Authorization" ).empty() )
						{ // Authentication wasn't attempted, send the request.
							ConnectionListIterator->ServerResponse.SetValue ( "WWW-Authenticate", 
								"Basic realm=\"DFileServer\"" );
							ConnectionListIterator->ServerResponse.AccessPath = "401"; // Authorization Required
						}
						else
						{ // Try to verify the authentication.
							Base64 Base64Encoding;
							string Base64Authorization;

							Base64Authorization = ConnectionListIterator->BrowserRequest.GetValue( "Authorization" );

							Base64Authorization.erase (0, 6); // Remove the beginning "Basic "

						        if ( Base64Encoding.decode( Base64Authorization ) != ConfigurationBasicCredentials )
							{
								ConnectionListIterator->ServerResponse.AccessPath = "401"; // Authorization Required

								ConnectionListIterator->CloseFile(); // Make sure we close the resource.

								// Prepare the 401...
								Buffer401 ( ConnectionListIterator );
								ResourceSize = ConnectionListIterator->BytesRemaining = ConnectionListIterator->SendBuffer.size();
								strcpy (Resource, "--401--");
								strcpy (ResourceType, "text/html");
							}

						}
					}

					sprintf( Buffer, "DashFileServer/%s.%s.%s", MAJORVERSION, MINORVERSION, PATCHVERSION );
					
					ConnectionListIterator->ServerResponse.SetValue ( "Server", string( Buffer ) );
					ConnectionListIterator->ServerResponse.SetValue ( "Connection", "close" );

					sprintf( Buffer, "%i", ResourceSize );

					ConnectionListIterator->ServerResponse.SetValue ( "Content-Length", string ( Buffer ) );
					ConnectionListIterator->ServerResponse.SetValue ( "Content-Type", string ( ResourceType ) );

					ConnectionListIterator->SendData( 
						(char *) ConnectionListIterator->ServerResponse.ExportHeader().c_str(), 0 );
				}					
			}
			else
			{ // Send 4096 bytes of the file the way of the client.
				char Buffer[4096];
				int BytesToRead, BytesRead;

				memset(Buffer, '\0', sizeof( Buffer) );

				BytesToRead = sizeof( Buffer ) - 1;

				if ( ConfigurationMaxBandwidth )
				{
					// -- Bandwidth Control --
					if ( ConnectionListIterator->LastBandReset != time ( NULL ) )
					{
						ConnectionListIterator->BandwidthLeft = ConfigurationMaxBandwidth / ActiveConnections;

						ConnectionListIterator->LastBandReset = time ( NULL );
					}

					 if ( BytesToRead > ConnectionListIterator->BandwidthLeft )
						 BytesToRead = ConnectionListIterator->BandwidthLeft;

					ConnectionListIterator->BandwidthLeft -= BytesToRead;
					// / -- Bandwidth Control --
				}
				
				if ( BytesToRead > ConnectionListIterator->BytesRemaining )
					BytesToRead = ConnectionListIterator->BytesRemaining;

				if ( ConnectionListIterator->FileStream )
				{ // Read from a file
					ConnectionListIterator->FileStream->read ( Buffer, BytesToRead );
				}
				else
				{ // Send from the buffer

					strcpy( Buffer, ConnectionListIterator->SendBuffer.substr( 
								ConnectionListIterator->SendBufferIterator, BytesToRead ).c_str() );
					
					// Increment Iterator
					ConnectionListIterator->SendBufferIterator += BytesToRead;
				}

				BytesRead = ConnectionListIterator->SendData( Buffer, BytesToRead);

				ConnectionListIterator->BytesRemaining -= BytesRead;

				// It hasn't sent all the bytes we've read.
				if (BytesRead != BytesToRead)
				{
					if ( ConnectionListIterator->FileStream )
					{
						// Relocate the file pointer to match where we are.

						int LocationOffset = BytesToRead - BytesRead;
						ifstream::pos_type CurrentPosition = ConnectionListIterator->FileStream->tellg();

						ConnectionListIterator->FileStream->seekg((CurrentPosition - (ifstream::pos_type) LocationOffset));
					}
					else
					{
						// Relocate the iterator to match where we are.

						int LocationOffset = BytesToRead - BytesRead;

						ConnectionListIterator->SendBufferIterator -= LocationOffset;
					}
				}

				// End of the file?
				if ( ConnectionListIterator->BytesRemaining == 0 )
				{
					ConnectionListIterator = TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
											PollStruct, &ConnectionList );
				}
			}
				

		} // [/END] Socket can accept data.

		// Connection was idle for too long, remove the jerk.
		if ( ConnectionListIterator != ConnectionList.end() && ConnectionListIterator->SecondsIdle() > 15 )
		{
			ConnectionListIterator = TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator, 
									PollStruct, &ConnectionList );
		}

	} // The linked list for loop.

	// Initate self destruct sequence.
	if ( ServerShutdown )
	{
		ConnectionListIterator = ConnectionList.begin();
		
		printf("Initating self destruct sequence...\n");

		printf("Terminating %i Client(s)\n", ActiveConnections );
		// Remove all Client Connections gracefully.
		while ( ConnectionListIterator != ConnectionList.end() )
		{
			ConnectionListIterator = TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
									PollStruct, &ConnectionList );
		}

		// Close the main server socket.
#ifdef _WINDOWS
		closesocket( ServerSocket );

		WSACleanup();
#else
		close( ServerSocket );
#endif

		printf("Clean shutdown\n");
		break; // Leave the main while loop, effectively terminating the program.
	}

   } // while (1) : The main loop.

   return 0;
}

