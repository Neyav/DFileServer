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
	void NetworkDaemon::IncomingConnection(SOCKET IncomingSocket)
	{
		ClientConnection *IncomingClient;

		IncomingClient = new ClientConnection;

		// Accept the connection and break if there was an error.
		if (IncomingClient->AcceptConnection(IncomingSocket) == -1)
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

		NetworkMessenger->SendPointer(MSG_TARGET_NETWORK, (void *)IncomingClient);
	}

	bool NetworkDaemon::addListener(unsigned int aPort, unsigned int aBackLog, TCPInterface *aInterface)
	{
		struct pollfd PollFDTemp;
		memset(&PollFDTemp, 0, sizeof(struct pollfd));

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
		if (PollStruct.size() < 2)
			NetworkMessenger->SendMessage(MSG_TARGET_CONSOLE, "Network Loop activated: Listening on " + std::to_string(PollStruct.size()) + " Interface...");
		else
			NetworkMessenger->SendMessage(MSG_TARGET_CONSOLE, "Network Loop activated: Listening on " + std::to_string(PollStruct.size()) + " Interfaces...");

		// Start the specified number of prime threads.
		for (int i = 0; i < Configuration.primeThreads; i++)
			NetworkThread* PrimeNetworkThread = new NetworkThread(true);
		
		while (1)
		{
			// As of C++11, vectors are guaranteed to be contiguous in memory. So this new hackery works.
			if (PollStruct.size() == 0)
			{
				NetworkMessenger->SendMessage(MSG_TARGET_CONSOLE, "No interfaces to listen on; exiting.");
				break;
			}

			struct pollfd *CPollStruct = &PollStruct[0];
			poll(CPollStruct, (int)PollStruct.size(), INFTIM);

			// Do we have an incoming connection?
			if ((PollStruct[0].revents & POLLIN) && (ActiveConnections != Configuration.MaxConnections || !Configuration.MaxConnections))
			{
				// Handle the incoming connection.
				NetworkMessenger->SendMessage(MSG_TARGET_CONSOLE, "Accepting incoming connection; distributing to NetworkThreads");
				IncomingConnection(PollStruct[0].fd);
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
	}
}