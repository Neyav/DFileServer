#pragma once

#ifdef _WINDOWS
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
#include <string>

#include "InterProcessMessaging.hxx"
#include "ClientConnection.hxx"

extern DFSMessaging::MessengerServer* MessengerServer;

namespace DFSNetworking
{
	class NetworkThread
	{
	private:
		size_t NetworkThreadID;
		bool primeThread;
		int localConnections;

#ifdef _WINDOWS
		std::vector<WSAPOLLFD> PollStruct;
#else
		std::vector<struct pollfd> PollStruct;
#endif
		std::vector<ClientConnection*> ConnectionList;
		DFSMessaging::Messenger* NetworkThreadMessenger;

		void TerminateConnection(int aConnectionIndex);

		char LocateResource(std::string Resource, ClientConnection* ArgClient, char* DstResource, char* DstResourceType);
		void Buffer404(ClientConnection* ArgClient);
		void Buffer401(ClientConnection* ArgClient);
		void ParseURLEncoding(char* ArgBuffer);
	public:

		void NetworkThreadLoop();

		NetworkThread();
		NetworkThread(bool aprimeThread);
		~NetworkThread();
	};
}