#include <iostream>

#include "Networking.hxx"
#include "NetworkThread.hxx"
#include "Version.hxx"
#include "CPathResolver.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"

extern int ActiveConnections;
extern bool ServerShutdown;
extern bool ServerLockdown;

namespace DFSNetworking
{
	/*void NetworkHandler::NetworkHandlerLoop(void)
	{

	}

	NetworkHandler::NetworkHandler()
	{
		size_t PointerReference = std::uintptr_t(this);

		NetworkHandlerMessanger = MessengerServer->ReceiveActiveMessenger();
		NetworkHandlerMessanger->Name = "NetworkHandler " + std::to_string(PointerReference);
		NetworkHandlerMessanger->RegisterOnChannel(MSG_TARGET_NETWORK);
		NetworkHandlerMessanger->SendMessage(MSG_TARGET_CONSOLE, "NetworkHandler - Initalized.");
	}

	NetworkHandler::~NetworkHandler()
	{
		NetworkHandlerMessanger->SendMessage(MSG_TARGET_CONSOLE, "NetworkHandler - Terminated.");
		delete NetworkHandlerMessanger;
	}*/

	void NetworkDaemon::IncomingConnection(void)
	{
		ClientConnection *IncomingClient;

		IncomingClient = new ClientConnection;

		// Accept the connection and break if there was an error.
		if (IncomingClient->AcceptConnection(NetworkSocket) == -1)
		{
			delete IncomingClient;
			return;
		}

		// If we're in serverlockdown, close the connection right away.
		if (ServerLockdown)
		{
			delete IncomingClient;

			return;
		}

		ActiveConnections++;

		NetworkMessanger->SendPointer(MSG_TARGET_NETWORK, (void *)IncomingClient);
	}

	bool NetworkDaemon::initializeNetwork(unsigned int aPort, unsigned int aBackLog)
	{
		struct sockaddr_in ListenAddr;
		const char yes = 1;
#ifdef _WINDOWS
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			perror("InitializeNetwork -- WSAStartup()");
			return false;
		}
#endif
		std::cout << " -=Initializing Network Daemon..." << std::endl;

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror("InitializeNetwork -- socket()");
			return false;
		}

		// Clear the socket incase it hasn't been properly closed so that we may use it.
		if (setsockopt(NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("InitializeNetwork -- setsockopt()");
			return false;
		}

		// Set the listening network struct up.
		ListenAddr.sin_family = AF_INET;
		ListenAddr.sin_port = htons(aPort);
		ListenAddr.sin_addr.s_addr = INADDR_ANY;
		memset(&(ListenAddr.sin_zero), '\0', 8);

		// Bind the socket to the listening network struct.
		if (bind(NetworkSocket, (struct sockaddr*)&ListenAddr,
			sizeof(struct sockaddr)) == -1)
		{
			perror("InitializeNetwork -- bind()");
			return false;
		}

		// Start listening
		if (listen(NetworkSocket, aBackLog) == -1)
		{
			perror("InitializeNetwork -- listen()");
			return false;
		}

		PollStruct[0].fd = NetworkSocket;
		PollStruct[0].events = POLLIN;
		listenPort = aPort;

		// We were successful, return the socket.
		return true;
	}

	void NetworkDaemon::NetworkLoop(void)
	{
		std::cout << " -=Network Loop activated: Listening on port " << listenPort << "..." << std::endl;

		// Start our prime Network Thread.
		NetworkThread* PrimeNetworkThread = new NetworkThread(true);
		
		while (1)
		{
			// As of C++11, vectors are guaranteed to be contiguous in memory. So this new hackery works.
			struct pollfd *CPollStruct = &PollStruct[0];
			poll(CPollStruct, PollStruct.size(), 100);

			// Do we have an incoming connection?
			if ((PollStruct[0].revents & POLLIN) && (ActiveConnections != Configuration.MaxConnections || !Configuration.MaxConnections))
			{
				// Handle the incoming connection.
				NetworkMessanger->SendMessage(MSG_TARGET_CONSOLE, "Accepting incoming connection; distributing to NetworkThreads");
				IncomingConnection();
			}

			// Initate self destruct sequence.
			if (ServerShutdown)
			{
				printf("Initating self destruct sequence...\n");

				// Close the main server socket.
#ifdef _WINDOWS
				closesocket(NetworkSocket);

				WSACleanup();
#else
				close(NetworkSocket);
#endif

				printf("Clean shutdown\n");
				break; // Leave the main while loop, effectively terminating the program.
			}

		} // while (1) : The main loop.
	}

	NetworkDaemon::NetworkDaemon()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;
		struct pollfd PollFDTemp;
		memset(&PollFDTemp, 0, sizeof(struct pollfd));

		PollStruct.push_back(PollFDTemp); // Add the master socket to the poll structure.

		if (MessengerServer)
		{
			NetworkMessanger = MessengerServer->ReceiveActiveMessenger();
			NetworkMessanger->Name = "Network";
		}
		else
			NetworkMessanger = nullptr;
	}
	NetworkDaemon::~NetworkDaemon()
	{
		if (NetworkMessanger)
			delete NetworkMessanger;
	}
}