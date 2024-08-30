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

namespace DFSNetworking
{
	class NetworkDaemon
	{
	private:
		unsigned int listenPort;
		unsigned int backLog;
		
		std::vector<struct pollfd> PollStruct;
		std::vector<ClientConnection*> ConnectionList;
		DFSMessaging::Messenger* NetworkMessanger;

		SOCKET NetworkSocket;

		void IncomingConnection(void);
	public:
		bool initializeNetwork(unsigned int aPort, unsigned int aBackLog);

		void NetworkLoop();

		NetworkDaemon();
		~NetworkDaemon();
	};
}