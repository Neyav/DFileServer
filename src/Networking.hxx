#pragma once
#ifdef _WINDOWS
#include <winsock.h>
#include "contrib/fakepoll.h"

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
#include "ClientConnection.hxx"
#include "InterProcessMessaging.hxx"

extern DFSMessaging::MessengerServer* MessangerServer;

namespace DFSNetworking
{
	class NetworkHandler
	{
	private:
		std::vector<struct pollfd> PollStruct;
		std::vector<ClientConnection*> ConnectionList;
		DFSMessaging::Messanger* NetworkHandlerMessanger;

	public:
		void NetworkHandlerLoop(void);

		NetworkHandler();
		~NetworkHandler();
	};

	class NetworkDaemon
	{
	private:
		unsigned int listenPort;
		unsigned int backLog;
		
		std::vector<struct pollfd> PollStruct;
		std::vector<ClientConnection*> ConnectionList;
		DFSMessaging::Messanger* NetworkMessanger;

		SOCKET NetworkSocket;

		void IncomingConnection(void);
		void TerminateConnection(int aConnectionIndex);

		char LocateResource(std::string Resource, ClientConnection* ArgClient, char* DstResource, char* DstResourceType);
		void Buffer404(ClientConnection* ArgClient);
		void Buffer401(ClientConnection* ArgClient);
		void ParseURLEncoding(char* ArgBuffer);
	public:
		bool initalizeNetwork(unsigned int aPort, unsigned int aBackLog);

		void NetworkLoop();

		NetworkDaemon();
		~NetworkDaemon();
	};
}