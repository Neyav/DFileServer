#include <iostream>

#ifdef _WINDOWS
#include <WinSock2.h>
#include <WS2tcpip.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#endif

#include "Networking.hxx"
#include "NetworkThread.hxx"
#include "Version.hxx"
#include "CPathResolver.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"

extern int ActiveConnections;

namespace DFSNetworking
{
	void NetworkDaemon::IncomingConnection(Interface *aInterface)
	{
		ClientConnection *IncomingClient;

		IncomingClient = new ClientConnection;

		// Accept the connection and break if there was an error.
		if (IncomingClient->AcceptConnection(aInterface) == -1)
		{
			delete IncomingClient;
			return;
		}

		ActiveConnections++;

		NetworkMessenger->SendPointer(MSG_TARGET_NETWORK, (void *)IncomingClient);
	}

	bool NetworkDaemon::addListener(unsigned int aPort, unsigned int aBackLog, Interface *aInterface)
	{
#ifdef _WINDOWS
		WSAPOLLFD PollFDTemp;
		memset(&PollFDTemp, 0, sizeof(WSAPOLLFD));
#else
		struct pollfd PollFDTemp;
		memset(&PollFDTemp, 0, sizeof(struct pollfd));
#endif
		

		aInterface->initializeInterface(aPort, aBackLog);

		PollFDTemp.fd = aInterface->getSocket();
		PollFDTemp.events = POLLIN;

		PollStruct.push_back(PollFDTemp);

		InterfaceList.push_back(aInterface);

		// We were successful, return the socket.
		return true;
	}

	void NetworkDaemon::NetworkLoop(void)
	{

		// Start the specified number of prime threads.

		NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_SYSTEM, "Starting " + std::to_string(Configuration.primeThreads) + " prime network threads.");

		for (int i = 0; i < Configuration.primeThreads; i++)
			NetworkThread* PrimeNetworkThread = new NetworkThread(true);

		if (PollStruct.size() < 2)
			NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Network Loop activated: Listening on " + std::to_string(PollStruct.size()) + " Interface...");
		else
			NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Network Loop activated: Listening on " + std::to_string(PollStruct.size()) + " Interfaces...");
		
		while (1)
		{
			// As of C++11, vectors are guaranteed to be contiguous in memory. So this new hackery works.
			if (PollStruct.size() == 0)
			{
				NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "No interfaces to listen on; exiting.");
				break;
			}
		

#ifdef _WINDOWS
			WSAPOLLFD* CPollStruct = (WSAPOLLFD*)&PollStruct[0];
			WSAPoll(CPollStruct, (ULONG) PollStruct.size(), 100);
#else
			struct pollfd *CPollStruct = (struct pollfd *) &PollStruct[0];
			poll(CPollStruct, PollStruct.size(), 100);
#endif

			if (NetworkMessenger->HasMessages())
			{
				DFSMessaging::Message IncomingMessage = NetworkMessenger->AcceptMessage();

				if (IncomingMessage.message == "SHUTDOWN")
				{
					NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Shutdown signal received, shutting down interfaces.");
					delete this;
					return;
				}
			}

			for (int i = 0; i < PollStruct.size(); i++)
			{
				// Do we have an incoming connection?
				if ((PollStruct[i].revents & POLLIN) && (ActiveConnections != Configuration.MaxConnections || !Configuration.MaxConnections))
				{
					// Handle the incoming connection.
					NetworkMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "Accepting incoming connection; distributing to NetworkThreads");
					IncomingConnection(InterfaceList[i]);
				}
			}

		} // while (1) : The main loop.
	}

	NetworkDaemon::NetworkDaemon()
	{
		if (MessengerServer)
		{
			NetworkMessenger = MessengerServer->ReceiveActiveMessenger();
			NetworkMessenger->Name = "Network";
		}
		else
			NetworkMessenger = nullptr;
	}
	NetworkDaemon::~NetworkDaemon()
	{
		if (NetworkMessenger)
			delete NetworkMessenger;

		for (int i = 0; i < InterfaceList.size(); i++)
			delete InterfaceList[i];
	}
}