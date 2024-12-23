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
#include <io.h>
#include <conio.h>

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
//#include <iterator>
#include <string>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <string_view>

#include "InterProcessMessaging.hxx"
#include "TCPInterface.hxx"
#include "Networking.hxx"
#include "Version.hxx"

DFSMessaging::MessengerServer* MessengerServer = nullptr;

namespace Version
{
	int MAJORVERSION = 2;
	int MINORVERSION = 1;
	int PATCHVERSION = 0;
	std::string VERSIONTITLE = "The Red Deer Redemption";
}

void DFSleep(int milliseconds)
{
#ifdef _WINDOWS
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}

Configuration_t Configuration;

bool ServerLockdown = false;
bool ServerShutdown = false;

int ActiveConnections = 0;

#ifdef _WINDOWS
BOOL WINAPI InitiateServerShutdown(DWORD ArgSignal)
{
	if (signal != CTRL_C_EVENT)
		return FALSE;

	std::cout << " -=Caught Ctrl-C, Initiating Server Shutdown..." << std::endl;	

	if (MessengerServer)
	{
		DFSMessaging::Messenger* ServerShutdownMessenger;

		ServerShutdownMessenger = MessengerServer->ReceiveActiveMessenger();

		ServerShutdownMessenger->sendMessage(MSG_TARGET_NETWORK, "SHUTDOWN");

		while (!ServerShutdownMessenger->HasMessages())
		{ // Wait for the response.
			DFSleep(100);
		}
	}
	else
	{
		ServerShutdown = true;	
	}

	return TRUE;
}
#else
void InitateServerShutdown ( int ArgSignal )
{
	if (MessengerServer)
	{
		DFSMessaging::Messenger* ServerShutdownMessanger;

		ServerShutdownMessanger = MessengerServer->ReceiveActiveMessenger();
	
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
#endif

// Helper function to convert an integer to a hex string
std::string i2hex(unsigned int value, int width, bool lowercase = true)
{
	std::ostringstream stream;
	stream << std::setw(width) << std::setfill('0') << std::hex << (lowercase ? std::nouppercase : std::uppercase) << value;
	return stream.str();
}

void URLEncode(std::string& ArgBuffer)
{
	static const std::unordered_set<char> unreservedChars = { '-', '.', '_', '~', '/'};
	std::ostringstream encodedStream;
	std::string_view bufferView(ArgBuffer);

	for (unsigned char c : bufferView)
	{
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || unreservedChars.count(c))
		{
			encodedStream << c; // Append unreserved characters as-is
		}
		else
		{
			encodedStream << "%" << i2hex(static_cast<unsigned int>(c), 2); // Append URL-encoded characters
		}
	}

	ArgBuffer = encodedStream.str(); // Replace the original string with the encoded string
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

inline void DisplayBannerString(std::string ArgBannerString)
{
	int ConsoleBlankSpace = Configuration.Console.Width() - (int)ArgBannerString.size() - 4;
	std::cout << ArgBannerString << std::string(ConsoleBlankSpace, ' ');
}

inline void punchBannertoScreen(void)
{
	// FileServer Name/Version Banner
		std::string VersionBanner;
		std::string CopyrightBanner;
		std::string CopyrightBanner2;

		VersionBanner = "DFileServer Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + " --> {" + Version::VERSIONTITLE + "}";
		CopyrightBanner = "                    (c) 2005, 2018, 2023-2024 Christopher Laverdure";
		CopyrightBanner2 = "                    All Rights Reserved.";

		// First we want to clear the screen.
		std::cout << "\u001B[2J\u001B[1;1H";
		std::cout << "\u001B[41m\u001B[30m";
		DisplayBannerString(VersionBanner);
		std::cout << "**-]\u001B[0m" << std::endl;

		std::cout << "\u001B[41m\u001B[30m";
		DisplayBannerString(CopyrightBanner);
		std::cout << "**-]\u001B[0m" << std::endl;

		std::cout << "\u001B[41m\u001B[30m";
		DisplayBannerString(CopyrightBanner2);
		std::cout << "**-]\u001B[0m" << std::endl;
}

void interactiveConsole(DFSMessaging::Messenger* ConsoleMessenger)
{
	// Clear screen by default.
	Configuration.Console.clearScreen();
	punchBannertoScreen();

	while (1)
	{

	}
}

void legacyConsole(DFSMessaging::Messenger* ConsoleMessenger)
{
	while (1)
	{
		// Check for messages.
		ConsoleMessenger->pauseForMessage(250);

		while (ConsoleMessenger->HasMessages())
		{
			DFSMessaging::Message MessagePacket = ConsoleMessenger->AcceptMessage();

			if (Configuration.Verbose) // If we are in verbose mode, then we want to print out the message.
				std::cout << std::string(TimeAndDate()) << " [" << MessagePacket.OriginName << "]: " << MessagePacket.message << std::endl;

			if (Configuration.LogFile)
			{
				fprintf(Configuration.LogFile, "%s [%s]: %s\n", TimeAndDate(), MessagePacket.OriginName.c_str(), MessagePacket.message.c_str());
				fflush(Configuration.LogFile);
			}
		}

		if (_kbhit())
		{
			char keyhit = _getch();

			if (keyhit == 'q' || keyhit == 'Q')
			{
				std::cout << " -=Termination Key hit, sending SHUTDOWN message." << std::endl;
				if (MessengerServer != nullptr)
					MessengerServer->ShutdownServer();
			}
		}
	}
}

int main( int argc, char *argv[] )
{

#ifndef _WINDOWS
   int ConfigurationSetUID = 0;
   std::string ConfigurationChrootFolder;
#endif

#ifdef _DFS_USE_OPENSSL
#ifdef _WINDOWS
#pragma comment(lib, "Crypt32.lib")
#endif
   SSL_library_init();
   OpenSSL_add_all_algorithms();
   SSL_load_error_strings();
#endif

   DFSNetworking::NetworkDaemon *NetworkDaemon = nullptr;
   DFSMessaging::Messenger *ConsoleMessenger = nullptr;


#ifndef _WINDOWS
   // Catch SIGPIPE and ignore it.
   signal( SIGPIPE, SIG_IGN );
   signal(SIGTERM, InitateServerShutdown);
   signal(SIGINT, InitateServerShutdown);
   signal(SIGQUIT, InitateServerShutdown);
#else
   if (!SetConsoleCtrlHandler(InitiateServerShutdown, TRUE)) {
	   std::cout << " -=Control Handler capture failed." << std::endl;
	   return 1;
   }
#endif

   // FileServer Name/Version Banner
   Configuration.Console.clearScreen();
   punchBannertoScreen();

   std::cout << "In memorial for all the code that was lost that one day when Sylvia wrote her last bit." << std::endl;
   std::cout << "Initializing..." << std::endl;
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

   MessengerServer = new DFSMessaging::MessengerServer();
   NetworkDaemon = new DFSNetworking::NetworkDaemon();
   
   ConsoleMessenger = MessengerServer->ReceiveActiveMessenger();
   ConsoleMessenger->Name = "Console";
   ConsoleMessenger->RegisterOnChannel(MSG_TARGET_CONSOLE);

   std::cout << " -=Initialize Network..." << std::endl;

   DFSNetworking::TCPInterface *IPv4Interface = new DFSNetworking::TCPInterface;
   DFSNetworking::IPv6Interface* IPv6Interface = new DFSNetworking::IPv6Interface;

   if (NetworkDaemon->addListener(Configuration.Port, Configuration.BackLog, IPv4Interface) == false)
   {
	   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << Configuration.Port << std::endl;
	   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
   }

   if (NetworkDaemon->addListener(Configuration.Port, Configuration.BackLog, IPv6Interface) == false)
   {
	   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << Configuration.Port << std::endl;
	   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
   }
#ifdef _DFS_USE_OPENSSL
   DFSNetworking::HTTPSIPv4Interface* HTTPSIPv4Interface = new DFSNetworking::HTTPSIPv4Interface;

   if (NetworkDaemon->addListener(443, Configuration.BackLog, HTTPSIPv4Interface) == false)
   {
	   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << Configuration.Port << std::endl;
	   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
   }
#endif

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
   if (Configuration.interactiveConsole)
   {
	   // Give us one second for our threads to spawn.
	   DFSleep(1000);
	   interactiveConsole(ConsoleMessenger);
   }
   else
	   legacyConsole(ConsoleMessenger);

   return 0;
}

