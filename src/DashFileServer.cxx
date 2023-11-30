/*
** ($Header: /var/www/cvsroot/DFileServer/src/DashFileServer.cxx,v 1.61.2.19 2005/10/23 19:51:28 incubus Exp $)
**
** Copyright 2005, 2018 Chris Laverdure
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
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strncpy strncpy_s

#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif
#include <vector>
#include <iterator>
#include <string>
#include <iostream>

#include "ClientConnection.hxx"
#include "CPathResolver.hxx"
#include "DirectoryIndexing.hxx"
#include "InterProcessMessaging.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"
#include "Version.hxx"

#ifndef INFTIM
#define INFTIM -1
#endif

#define MAXIDENTIFIERLEN 151

namespace Version
{
	int MAJORVERSION = 2;
	int MINORVERSION = 0;
	int PATCHVERSION = 0;
}

bool ServerLockdown = false;
bool ServerShutdown = false;

std::string ConfigurationBasicCredentials = "";
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

void URLEncode( std::string &ArgBuffer )
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
			std::string URLEncodedCharacter;

			URLEncodedCharacter = "%" + i2hex( (unsigned int) 
					( (unsigned char) ArgBuffer[iterator] ), 2, true );

			// Swap the character out with the URL encoded version.
			ArgBuffer.replace (iterator, 1, URLEncodedCharacter);
		}
	}

}

void ParseURLEncoding( char *ArgBuffer )
{
	size_t BufferLength = strlen(ArgBuffer);
	size_t iterator1;
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

SOCKET InitalizeNetwork ( int ArgPort, int ArgBacklog )
{
	struct sockaddr_in ListenAddr;
	SOCKET NetworkSocket;
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
								std::vector<ClientConnection> *ArgList )
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

void TerminateConnection ( int ArgSocket, int ArgClient, int *ArgHighestIterator, 
						struct pollfd ArgPoll[], std::vector <ClientConnection> *ArgList )
{
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

	// Keep track of the old iterator. Previously I just used ArgClient->PollIterator in this function, but that doesn't
	// appear safe to do after the ArgList->erase command. It's causing crashes on some platforms (i.e. FreeBSD 6.x )
	OldPollIterator = (*ArgList)[ArgClient].PollIterator;

	// Remove this client from the Poll structure so it doesn't get polled.
	ArgPoll[OldPollIterator].fd = 0;
	ArgPoll[OldPollIterator].events = 0;

	// Close the socket and possibly do some other Misc shutdown stuff.
    (*ArgList)[ArgClient].DisconnectClient();

	// Remove the client from the linked list.
	(*ArgList).erase((*ArgList).begin() + ArgClient);

	// This Client had the highest descriptor..
	if ( OldPollIterator == *ArgHighestIterator )
	{
		// Traverse through the active connections to find the new highest file descriptor
		for (int ConnectionListIterator = 0;
			ConnectionListIterator < (*ArgList).size();
				ConnectionListIterator++)
		{
			if ( (*ArgList)[ConnectionListIterator].PollIterator > NewHighestPollIterator )
				NewHighestPollIterator = (*ArgList)[ConnectionListIterator].PollIterator;
		}

		*ArgHighestIterator = NewHighestPollIterator;
	}
	return;
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

char LocateResource ( std::string Resource, ClientConnection *ArgClient, char *DstResource, char *DstResourceType )
{
	FILE *FileStream;
	char FieldName[31];
	char ResolveIdentifier[MAXIDENTIFIERLEN];
	char RealIdentifier[MAXIDENTIFIERLEN];
	char IPRange[17];
	char ResourceType[MAXIDENTIFIERLEN];

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
		fscanf( FileStream, "%30s %150s %150s", FieldName, ResolveIdentifier, ResourceType );

		if ( strcmp( FieldName, "RESOURCE" ) == 0) // A resource.
		{
			if ( strcmp( ResourceType, "redirect" ) == 0 ) // redirect
			{
				if ( strncmp( ResolveIdentifier, Resource.c_str(), strlen( ResolveIdentifier ) ) == 0 )
				{
					strcpy ( DstResourceType, ResourceType );
					break;
				}
			}

			if ( strcmp( ResolveIdentifier, Resource.c_str() ) == 0) // A match.
			{
				strcpy ( DstResourceType, ResourceType );	
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

void Buffer404 ( ClientConnection *ArgClient )
{
	// This fills the buffer with the 404 error.

	ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Resource not found</TITLE></HEAD><BODY><H1>Resource not found</H1>";
	ArgClient->SendBuffer += "The resource you were trying to locate doesn't exist on this server.<br><br><HR>";
	ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "]</I>";
	ArgClient->SendBuffer += "</BODY></HTML>";
}

void Buffer401 ( ClientConnection *ArgClient )
{
	// This fills the buffer with the 401 error.
         
	ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Authorization Required</TITLE></HEAD><BODY><H1>Authorization Required</H1>";
	ArgClient->SendBuffer += "The resource you were trying to locate requires authorization on this server.<br><br><HR>";
	ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "]</I>";
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
   std::string ConfigurationChrootFolder;
#endif
   struct pollfd PollStruct[2048];
   std::vector<ClientConnection> ConnectionList;
   DFSMessaging::MessangerServer *MessangerServer = nullptr;

#ifndef _WINDOWS
   // Catch SIGPIPE and ignore it.
   signal( SIGPIPE, SIG_IGN );
#endif
   signal( SIGTERM, InitateServerShutdown );
   signal( SIGINT, InitateServerShutdown );

   // First we want to clear the screen.
   std::cout << "\u001B[2J\u001B[1;1H";
   std::cout << "\u001B[41m\u001B[30mDFileServer Version " << Version::MAJORVERSION << "." << Version::MINORVERSION << "." << Version::PATCHVERSION << "  !!-[DEVELOPMENT VERSION]-!!                                         **-]\u001B[0m" << std::endl;
   std::cout << "\u001B[41m\u001B[30m                    (c) 2005, 2018, 2023 Christopher Laverdure                                 **-]\u001B[0m" << std::endl;
   std::cout << "\u001B[41m\u001B[30m                    All Rights Reserved.                                                       **-]\u001B[0m" << std::endl;
   std::cout << "In memorial for all the code that was lost that one day when Sylvia wrote her last bit." << std::endl;
   std::cout << "Initalizing..." << std::endl;
   std::cout << " -=Parameters..." << std::endl;
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

		ConfigurationBasicCredentials = std::string ( argv[x] );

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

		ConfigurationChrootFolder = std::string(argv[x]);
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

   MessangerServer = new DFSMessaging::MessangerServer();

   std::cout << " -=Initalize Network..." << std::endl;

   memset( PollStruct, '\0', sizeof(PollStruct) );

   if ( (ServerSocket = InitalizeNetwork( ConfigurationPort, ConfigurationBackLog )) == -1 )
   {
	printf("CRITICAL ERROR: Couldn't initalize listening socket on port %i\n", ConfigurationPort );
	exit(-1);
   }

   printf(" -=Listening on port: %i\n", ConfigurationPort );

   DFSMessaging::Messanger* Messanger = nullptr;

   Messanger = MessangerServer->ReceiveActiveMessanger();

   Messanger->RegisterOnChannel("LocalConsole");

   Messanger->SendMessage("LocalConsole", "Message from DFileServer: Hello World!");

#ifndef _WINDOWS

   std::cout << " -=UNIX Specific configuration optionals..." << std::endl;

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

   std::cout << " -=Waiting for connections. Initalization complete." << std::endl;

   // Main Program Loop
   while (1)
   {
	poll ( PollStruct, HighestPollIterator + 1, INFTIM );

	// Do we have an incoming connection?
	if ( ( PollStruct[0].revents & POLLIN ) && ( ActiveConnections != ConfigurationMaxConnections || !ConfigurationMaxConnections ) )
	{
		// Handle the incoming connection.
		IncomingConnection ( ServerSocket, &HighestPollIterator, PollStruct, &ConnectionList );
	}

	// Go through the list looking for connections that have incoming data.
	for (int ConnectionListIterator = 0; ConnectionListIterator < ConnectionList.size(); ConnectionListIterator++)
	{
		// This socket has incoming data.
		if ( PollStruct[ConnectionList[ConnectionListIterator].PollIterator].revents & POLLIN)
		{
			char DataBuffer[500]; // 500 should be big enough.
			size_t DataRecved;

			if ( ( DataRecved = ConnectionList[ConnectionListIterator].RecvData( DataBuffer, sizeof( DataBuffer ) ) ) < 1 )
			{ // Disconnection or error. Terminate client.

				TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
												PollStruct, &ConnectionList );
				ConnectionListIterator--;
				if ( ConnectionListIterator < 0 )
					ConnectionListIterator = 0;
				continue;
			}
			else
			{ // Incoming data.
				ConnectionList[ConnectionListIterator].BrowserRequest.ImportHeader ( std::string( DataBuffer ) );

				// Find what resource this person is after.
				ConnectionList[ConnectionListIterator].Resource = ConnectionList[ConnectionListIterator].BrowserRequest.AccessPath;
			}			

		} // [/END] Incoming Data on socket.
		
		if ( ConnectionListIterator < ConnectionList.size() && !ConnectionList[ConnectionListIterator].Resource.empty() &&
				PollStruct[ConnectionList[ConnectionListIterator].PollIterator].revents & POLLOUT )
		{

			if ( !ConnectionList[ConnectionListIterator].FileStream 
					&& ConnectionList[ConnectionListIterator].SendBuffer.empty() )
			{
				char Resource[151];
				char ResourceType[151];
				size_t ResourceSize = 0;

				// We need to locate this resource.
				if ( LocateResource ( ConnectionList[ConnectionListIterator].Resource, &ConnectionList[ConnectionListIterator], Resource, ResourceType) == -1)
				{ // Resource wasn't found.
					Buffer404(&ConnectionList[ConnectionListIterator]);
					ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
					strcpy (Resource, "--404--");
					strcpy (ResourceType, "text/html");

					continue;
				}

				ParseURLEncoding( Resource );

				// Is it a redirect?
				if ( strcmp( ResourceType, "redirect" ) == 0 )
				{
					char direrror;

					direrror = GenerateFolderIndex( ConnectionList[ConnectionListIterator].Resource, Resource, ConnectionList[ConnectionListIterator].SendBuffer );

					if (direrror == -1)
					{ // File/Folder doesn't exist
						Buffer404(&ConnectionList[ConnectionListIterator]);
						ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
						strcpy (Resource, "--404--");
						strcpy (ResourceType, "text/html");
					}
					else if (direrror)
					{ // It generated an index for us.

						ConnectionList[ConnectionListIterator].SendBufferIterator = 0;

						ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();

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
					printf("%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionList[ConnectionListIterator].GetIP(), 
									ConnectionList[ConnectionListIterator].Resource.c_str(), Resource );
				}

				if ( LogFilePointer != NULL )
				{
					fprintf( LogFilePointer, "%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionList[ConnectionListIterator].GetIP(),
									ConnectionList[ConnectionListIterator].Resource.c_str(), Resource );

					// Make sure it gets written to disk.
					fflush( LogFilePointer );
				}

				// If there is no sendbuffer, try to open the file, and if you can't, terminate the connection.
				if ( ConnectionList[ConnectionListIterator].SendBuffer.empty() && 
					(( ResourceSize = ConnectionList[ConnectionListIterator].OpenFile( Resource ) ) == -1) )
				{ // File couldn't be opened.
					TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
													PollStruct, &ConnectionList );
					ConnectionListIterator--;
					if ( ConnectionListIterator < 0 )
						ConnectionListIterator = 0;
					continue;
				}
				else
				{				
					// Send the HTTP header.
					char Buffer[500];

					ConnectionList[ConnectionListIterator].ServerResponse.AccessType = "HTTP/1.1";
					ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "200";
					ConnectionList[ConnectionListIterator].ServerResponse.AccessProtocol = "OK";

					if ( !ConfigurationBasicCredentials.empty() ) // Authentication Required
					{
						if ( ConnectionList[ConnectionListIterator].BrowserRequest.GetValue( "Authorization" ).empty() )
						{ // Authentication wasn't attempted, send the request.
							ConnectionList[ConnectionListIterator].ServerResponse.SetValue ( "WWW-Authenticate", 
								"Basic realm=\"DFileServer\"" );
							ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "401"; // Authorization Required
						}
						else
						{ // Try to verify the authentication.
							Base64 Base64Encoding;
							std::string Base64Authorization;

							Base64Authorization = ConnectionList[ConnectionListIterator].BrowserRequest.GetValue( "Authorization" );

							Base64Authorization.erase (0, 6); // Remove the beginning "Basic "

						        if ( Base64Encoding.decode( Base64Authorization ) != ConfigurationBasicCredentials )
							{
								ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "401"; // Authorization Required

								ConnectionList[ConnectionListIterator].CloseFile(); // Make sure we close the resource.

								// Prepare the 401...
								Buffer401 (&ConnectionList[ConnectionListIterator]);
								ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
								strcpy (Resource, "--401--");
								strcpy (ResourceType, "text/html");
							}

						}
					}

					sprintf( Buffer, "DFileServer/%i.%i.%i", Version::MAJORVERSION, Version::MINORVERSION, Version::PATCHVERSION );
					
					ConnectionList[ConnectionListIterator].ServerResponse.SetValue ( "Server", std::string( Buffer ) );
					ConnectionList[ConnectionListIterator].ServerResponse.SetValue ( "Connection", "close" );

					sprintf( Buffer, "%i", (int) ResourceSize );

					ConnectionList[ConnectionListIterator].ServerResponse.SetValue ( "Content-Length", std::string ( Buffer ) );
					ConnectionList[ConnectionListIterator].ServerResponse.SetValue ( "Content-Type", std::string ( ResourceType ) );

					ConnectionList[ConnectionListIterator].SendData( 
						(char *) ConnectionList[ConnectionListIterator].ServerResponse.ExportHeader().c_str(), 0 );
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
					if ( ConnectionList[ConnectionListIterator].LastBandReset != time ( NULL ) )
					{
						ConnectionList[ConnectionListIterator].BandwidthLeft = ConfigurationMaxBandwidth / ActiveConnections;

						ConnectionList[ConnectionListIterator].LastBandReset = time ( NULL );
					}

					 if ( BytesToRead > ConnectionList[ConnectionListIterator].BandwidthLeft )
						 BytesToRead = ConnectionList[ConnectionListIterator].BandwidthLeft;

					ConnectionList[ConnectionListIterator].BandwidthLeft -= BytesToRead;
					// / -- Bandwidth Control --
				}
				
				if ( BytesToRead > ConnectionList[ConnectionListIterator].BytesRemaining )
					BytesToRead = ConnectionList[ConnectionListIterator].BytesRemaining;

				if ( ConnectionList[ConnectionListIterator].FileStream )
				{ // Read from a file
					ConnectionList[ConnectionListIterator].FileStream->read ( Buffer, BytesToRead );
				}
				else
				{ // Send from the buffer

					strcpy( Buffer, ConnectionList[ConnectionListIterator].SendBuffer.substr( 
								ConnectionList[ConnectionListIterator].SendBufferIterator, BytesToRead ).c_str() );
					
					// Increment Iterator
					ConnectionList[ConnectionListIterator].SendBufferIterator += BytesToRead;
				}

				BytesRead = ConnectionList[ConnectionListIterator].SendData( Buffer, BytesToRead);

				ConnectionList[ConnectionListIterator].BytesRemaining -= BytesRead;

				// It hasn't sent all the bytes we've read.
				if (BytesRead != BytesToRead)
				{
					if ( ConnectionList[ConnectionListIterator].FileStream )
					{
						// Relocate the file pointer to match where we are.

						int LocationOffset = BytesToRead - BytesRead;
						std::ifstream::pos_type CurrentPosition = ConnectionList[ConnectionListIterator].FileStream->tellg();

						ConnectionList[ConnectionListIterator].FileStream->seekg((CurrentPosition - (std::ifstream::pos_type) LocationOffset));
					}
					else
					{
						// Relocate the iterator to match where we are.

						int LocationOffset = BytesToRead - BytesRead;

						ConnectionList[ConnectionListIterator].SendBufferIterator -= LocationOffset;
					}
				}

				// End of the file?
				if ( ConnectionList[ConnectionListIterator].BytesRemaining == 0 )
				{
					TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator,
											PollStruct, &ConnectionList );
					ConnectionListIterator--;
					if ( ConnectionListIterator < 0 )
						ConnectionListIterator = 0;
					continue;
				}
			}
				

		} // [/END] Socket can accept data.

		// Connection was idle for too long, remove the jerk.
		if ( ConnectionList[ConnectionListIterator].SecondsIdle() > 15 )
		{
			TerminateConnection ( ServerSocket, ConnectionListIterator, &HighestPollIterator, 
									PollStruct, &ConnectionList );
			ConnectionListIterator--;
			if ( ConnectionListIterator < 0 )
				ConnectionListIterator = 0;
			continue;
		}

	} // The linked list for loop.

	// Initate self destruct sequence.
	if ( ServerShutdown )
	{
		printf("Initating self destruct sequence...\n");

		printf("Terminating %i Client(s)\n", ActiveConnections );
		// Remove all Client Connections gracefully.
		while ( ConnectionList.begin() != ConnectionList.end() )
		{
			TerminateConnection ( ServerSocket, 0, &HighestPollIterator,
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

