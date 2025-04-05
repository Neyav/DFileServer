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
#include <termios.h>
#include <sys/ioctl.h>

#endif
#include <vector>
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

#ifndef _WINDOWS
// Terminal Control functions that mimic Windows ones in UNIX.
int _kbhit(void)
{
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
	select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}
char _getch(void)
{
	char buf = 0;
	struct termios old = { 0 };
	fflush(stdout);
	if (tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror("tcsetattr ~ICANON");
	return (buf);
}
#endif

DFSMessaging::MessengerServer* MessengerServer = nullptr;

namespace Version
{
	int MAJORVERSION = 2;
	int MINORVERSION = 2;
	int PATCHVERSION = 0;
	std::string VERSIONTITLE = "The Cochrane Collapse";
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

int ActiveConnections = 0;

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
		CopyrightBanner = "                    (c) 2005, 2018, 2023-2025 Christopher Laverdure";
		CopyrightBanner2 = "                    All Rights Reserved.";

		// First we want to clear the screen.
		std::cout << "\u001B[2J\u001B[1;1H";
		std::cout << "\u001B[46m\u001B[30m";
		DisplayBannerString(VersionBanner);
		std::cout << "**-]\u001B[0m" << std::endl;

		std::cout << "\u001B[46m\u001B[30m";
		DisplayBannerString(CopyrightBanner);
		std::cout << "**-]\u001B[0m" << std::endl;

		std::cout << "\u001B[46m\u001B[30m";
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

			if (MessagePacket.isMessageVisible(Configuration.ConsoleVisibility)) // Is this message in our visibility scope.
				std::cout << std::string(TimeAndDate()) << " [" << MessagePacket.OriginName << "]: " << MessagePacket.message << std::endl;

			if (Configuration.LogFile && MessagePacket.isMessageVisible(Configuration.ConsoleVisibility))
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

				std::cout << " -=Waiting 5 seconds for all threads to terminate..." << std::endl;
				DFSleep(5000);

				if (Configuration.LogFile)
					fclose(Configuration.LogFile);

				exit(0);
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

   // FileServer Name/Version Banner
   Configuration.Console.clearScreen();
   punchBannertoScreen();

   std::cout << "In memorial for all the code that was lost that one day when Sylvia wrote her last bit." << std::endl;
   std::cout << "Initializing..." << std::endl;
   std::cout << " -=Parameters..." << std::endl;

   bool interfacesActivated = false;

   MessengerServer = new DFSMessaging::MessengerServer();
   NetworkDaemon = new DFSNetworking::NetworkDaemon();

   ConsoleMessenger = MessengerServer->ReceiveActiveMessenger();
   ConsoleMessenger->Name = "Console";
   ConsoleMessenger->RegisterOnChannel(MSG_TARGET_CONSOLE);

   // Parse for command line parameters.
   for (int x = 1; x < argc; x++)
   {
	if ( strcasecmp( "-ipv4", argv[x] ) == 0 )
	{
		int port;
		x++;

		if (x >= argc)
		{
			std::cout << " -=CRITICAL ERROR: -ipv4 requires a port number." << std::endl;
			exit(-1);
		}

		port = atoi(argv[x]);

		if (!(port > 0 && port < 65536))
		{
			std::cout << " -=CRITICAL ERROR: Invalid port number for -ipv4." << std::endl;
			exit(-1);
		}

		DFSNetworking::TCPInterface* IPv4Interface = new DFSNetworking::IPv4Interface;
		
		printf(" -=Configuration: Added IPv4 Port -> %i\n", port);

		if (NetworkDaemon->addListener(port, Configuration.BackLog, IPv4Interface) == false)
		{
			std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << port << std::endl;
			exit(-1); // TODO: Replace with an exit function that cleans up after itself.
		}
		interfacesActivated = true;
	}
	else if (strcasecmp("-ipv6", argv[x]) == 0)
	{
		int port;
		
		x++;

		if (x >= argc)
		{
			std::cout << " -=CRITICAL ERROR: -ipv6 requires a port number." << std::endl;
			exit(-1);
		}

		port = atoi(argv[x]);

		if (!(port > 0 && port < 65536))
		{
			std::cout << " -=CRITICAL ERROR: Invalid port number for -ipv6." << std::endl;
			exit(-1);
		}

		DFSNetworking::IPv6Interface* IPv6Interface = new DFSNetworking::IPv6Interface;
		printf(" -=Configuration: Added IPv6 Port -> %i\n", port);
		if (NetworkDaemon->addListener(port, Configuration.BackLog, IPv6Interface) == false)
		{
			std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << port << std::endl;
			exit(-1); // TODO: Replace with an exit function that cleans up after itself.
		}
		interfacesActivated = true;
	}
	else if (strcasecmp("-ipv4s", argv[x]) == 0)
	{
#ifdef _DFS_USE_OPENSSL
		int port;	
		
		x++;
		if (x >= argc)
		{
			std::cout << " -=CRITICAL ERROR: -ipv4s requires a port number." << std::endl;
			exit(-1);
		}

		port = atoi(argv[x]);

		if (!(port > 0 && port < 65536))
		{
			std::cout << " -=CRITICAL ERROR: Invalid port number for -ipv4s." << std::endl;
			exit(-1);
		}

		printf(" -=Configuration: Added HTTPS IPv4 Port -> %i\n", port);
		DFSNetworking::HTTPSIPv4Interface* HTTPSIPv4Interface = new DFSNetworking::HTTPSIPv4Interface;
		if (NetworkDaemon->addListener(port, Configuration.BackLog, HTTPSIPv4Interface) == false)
		{
			std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << port << std::endl;
			exit(-1); // TODO: Replace with an exit function that cleans up after itself.
		}
		interfacesActivated = true;
#else
		std::cout << " -=Configuration: SSL Support not compiled in. Ignoring -ipv4s\n";
#endif
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
	else if ( strcasecmp( "-verbose_all", argv[x] ) == 0)
	{
		Configuration.ConsoleVisibility = VISIBILITY_ALL;

		printf(" -=Configuration: Verbose Console Messages Enabled.\n");
	}
	else if (strcasecmp("-verbose_connections", argv[x]) == 0)
	{
		Configuration.ConsoleVisibility |= VISIBILITY_CONNECTIONS;

		printf(" -=Configuration: Verbose Connection Messages Enabled.\n");
	}
	else if (strcasecmp("-verbose_fileaccess", argv[x]) == 0)
	{
		Configuration.ConsoleVisibility |= VISIBILITY_FILEACCESS;

		printf(" -=Configuration: Verbose File Access Messages Enabled.\n");
	}
	else if (strcasecmp("-verbose_system", argv[x]) == 0)
	{
		Configuration.ConsoleVisibility |= VISIBILITY_SYSTEM;

		printf(" -=Configuration: Verbose System Messages Enabled.\n");
	}
	else if (strcasecmp("-verbose_debug", argv[x]) == 0)
	{
		Configuration.ConsoleVisibility |= VISIBILITY_DEBUG;

		printf(" -=Configuration: Verbose Debug Messages Enabled.\n");
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

   std::cout << " -=Initialize Network..." << std::endl;

   if (interfacesActivated == false)
   {
	   DFSNetworking::IPv4Interface* IPv4Interface = new DFSNetworking::IPv4Interface;

	   printf(" -=Configuration: Added default IPv4 Port -> %i\n", 2000);

	   if (NetworkDaemon->addListener(2000, Configuration.BackLog, IPv4Interface) == false)
	   {
		   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << 2000 << std::endl;
		   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
	   }

	   DFSNetworking::IPv6Interface* IPv6Interface = new DFSNetworking::IPv6Interface;

	   if (NetworkDaemon->addListener(2000, Configuration.BackLog, IPv6Interface) == false)
	   {
		   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << 2000 << std::endl;
		   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
	   }
#ifdef _DFS_USE_OPENSSL
	   DFSNetworking::HTTPSIPv4Interface* HTTPSIPv4Interface = new DFSNetworking::HTTPSIPv4Interface;

	   // The proper port for SSL connections is 443, but without root access on Linux we can't bind to that port. I could make
	   // this a platform specific change, but that would be too confusing. So out of respect for portability, we'll stick with 2001
	   // for now.
	   if (NetworkDaemon->addListener(2001, Configuration.BackLog, HTTPSIPv4Interface) == false)
	   {
		   std::cout << "CRITICAL ERROR: Couldn't initialize listening socket on port " << 443 << std::endl;
		   exit(-1); // TODO: Replace with an exit function that cleans up after itself.
	   }
#endif
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

