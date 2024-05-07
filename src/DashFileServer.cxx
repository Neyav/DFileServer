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

#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define Sleep sleep

#endif
#include <vector>
#include <iterator>
#include <string>
#include <iostream>
#include <thread>

#include "InterProcessMessaging.hxx"
#include "Networking.hxx"
#include "Version.hxx"

DFSMessaging::MessangerServer* MessangerServer = nullptr;

namespace Version
{
	int MAJORVERSION = 2;
	int MINORVERSION = 0;
	int PATCHVERSION = 0;
	std::string VERSIONTITLE = "The Port Colborne Comeback";
}

Configuration_t Configuration;

bool ServerLockdown = false;
bool ServerShutdown = false;

int ActiveConnections = 0;

void InitateServerShutdown ( int ArgSignal )
{
	if (MessangerServer)
	{
		DFSMessaging::Messanger* ServerShutdownMessanger;

		ServerShutdownMessanger = MessangerServer->ReceiveActiveMessanger();
	
	}

	/*
	printf("Recieved Signal %i\n", ArgSignal );

	// First lockdown the server and close when all the connections are done.
	// If the controlling user insists, then terminate immediately.
	if ( ServerLockdown || ActiveConnections == 0 )
		ServerShutdown = true;
	else
	{
		ServerLockdown = true;

		printf( "Initating Server Lockdown, waiting on %i Client(s).\n", ActiveConnections );
	}*/
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

int main( int argc, char *argv[] )
{

#ifndef _WINDOWS
   int ConfigurationSetUID = 0;
   std::string ConfigurationChrootFolder;
#endif

   DFSNetworking::NetworkDaemon *NetworkDaemon = nullptr;
   DFSMessaging::Messanger *ConsoleMessanger = nullptr;


#ifndef _WINDOWS
   // Catch SIGPIPE and ignore it.
   signal( SIGPIPE, SIG_IGN );
#endif
   signal( SIGTERM, InitateServerShutdown );
   signal( SIGINT, InitateServerShutdown );

   // First we want to clear the screen.
   std::cout << "\u001B[2J\u001B[1;1H";
   std::cout << "\u001B[41m\u001B[30mDFileServer Version " << Version::MAJORVERSION << "." << Version::MINORVERSION << "." << Version::PATCHVERSION << " --> {" << Version::VERSIONTITLE << "}                                     **-]\u001B[0m" << std::endl;
   std::cout << "\u001B[41m\u001B[30m                    (c) 2005, 2018, 2023-2024 Christopher Laverdure                            **-]\u001B[0m" << std::endl;
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

		Configuration.Port = atoi(argv[x]);
		printf(" -=Configuration: Port -> %i\n", Configuration.Port);
	}
	else if ( strcasecmp( "-backlog", argv[x] ) == 0 )
	{
		x++;

		Configuration.BackLog = atoi(argv[x]);
		printf(" -=Configuration: BackLog -> %i\n", Configuration.BackLog);
	}
	else if ( strcasecmp( "-maxconnections", argv[x] ) == 0 )
	{
		x++;

		Configuration.MaxConnections = atoi(argv[x]);
		printf(" -=Configuration: MaxConnections -> %i\n", Configuration.MaxConnections);
	}
	else if ( strcasecmp( "-maxbandwidth", argv[x] ) == 0 )
	{
		x++;

		Configuration.MaxBandwidth = atoi(argv[x]);
		if ( Configuration.MaxBandwidth > 1048576 )
			printf(" -=Configuration: MaxBandwidth -> %.1f MB/s\n", (float) Configuration.MaxBandwidth / 1048576);
		else if ( Configuration.MaxBandwidth > 1024 )
			printf(" -=Configuration: MaxBandwidth -> %.1f KB/s\n", (float) Configuration.MaxBandwidth / 1024);
		else
			printf(" -=Configuration: MaxBandwidth -> %i B/s\n", Configuration.MaxBandwidth);
	}
	else if ( strcasecmp( "-verbose", argv[x] ) == 0)
	{
		Configuration.Verbose = true;

		printf(" -=Configuration: Verbose Console Messages Enabled.\n");
	}
	else if ( strcasecmp( "-log", argv[x] ) == 0 )
	{
		x++;

		// Open the log file.
		Configuration.LogFile = fopen( argv[x], "w+");

		printf(" -=Configuration: LogFile -> %s\n", argv[x] );
	}
	else if ( strcasecmp( "-requireauth", argv[x] ) == 0 )
	{
		x++;

		Configuration.BasicCredentials = std::string ( argv[x] );

		printf(" -=Configuration: Required Authentication Credentials -> %s\n", argv[x] );
	}
#ifndef _WINDOWS
	else if ( strcasecmp( "-setuid", argv[x] ) == 0 )
	{
		x++;

		ConfigurationSetUID = atoi(argv[x]);
		printf(" -=Configuration: Set UID to %i\n", ConfigurationSetUID );
	}
	else if ( strcasecmp( "-chroot", argv[x] ) == 0 )
	{
		x++;

		ConfigurationChrootFolder = std::string(argv[x]);
		printf(" -=Configuration: chroot to %s\n", argv[x]);
	}
	else if ( strcasecmp( "-background", argv[x] ) == 0 )
	{
		printf(" -=Configuration: Dropping to background...\n");

		if ( fork() != 0 )
		{
			printf(" -=Configuration: Child process spawned.\n");
			exit (0);
		}

		fclose(stdout); // We have no need for this.
	}
#endif
   }

   MessangerServer = new DFSMessaging::MessangerServer();
   NetworkDaemon = new DFSNetworking::NetworkDaemon();
   
   ConsoleMessanger = MessangerServer->ReceiveActiveMessanger();
   ConsoleMessanger->Name = "Console";
   ConsoleMessanger->RegisterOnChannel(MSG_TARGET_CONSOLE);

   std::cout << " -=Initalize Network..." << std::endl;

   if (NetworkDaemon->initalizeNetwork(Configuration.Port, Configuration.BackLog) == false)
   {
	   std::cout << "CRITICAL ERROR: Couldn't initalize listening socket on port " << Configuration.Port << std::endl;
	   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
   }

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

   // Thread out and detach the network loop.
   std::thread NetworkThread( &DFSNetworking::NetworkDaemon::NetworkLoop, NetworkDaemon );
   NetworkThread.detach();

   // Main Program Loop
   while (1)
   {
	   // Check for messages.
	   while (ConsoleMessanger->HasMessages())
	   {
		   DFSMessaging::MessagePacket MessagePacket = ConsoleMessanger->AcceptMessage();
		   
		   if ( Configuration.Verbose ) // If we are in verbose mode, then we want to print out the message.
				std::cout << std::string(TimeAndDate()) << " [" << MessagePacket.OriginName << "]: " << MessagePacket.message << std::endl;

		   if ( Configuration.LogFile)
		   {
			   fprintf(Configuration.LogFile, "%s [%s]: %s\n", TimeAndDate(), MessagePacket.OriginName.c_str(), MessagePacket.message.c_str());
			   fflush(Configuration.LogFile);
		   }
	   }
	   Sleep(1000);
   }

   return 0;
}

