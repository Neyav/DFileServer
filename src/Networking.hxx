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
#include "TCPInterface.hxx"

namespace DFSNetworking
{
	class NetworkDaemon
	{
	private:
		std::vector<struct pollfd> PollStruct;
		std::vector<TCPInterface*> InterfaceList;
		DFSMessaging::Messenger* NetworkMessenger;

		void IncomingConnection(SOCKET IncomingSocket);
	public:
		bool addListener(unsigned int aPort, unsigned int aBacklog, TCPInterface *aInterface);

		void NetworkLoop();

		NetworkDaemon();
		~NetworkDaemon();
	};
}