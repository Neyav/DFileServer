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
#include "ClientConnection.hxx"
#include "InterProcessMessaging.hxx"
#include "Interfaces/Interface.hxx"

namespace DFSNetworking
{
	class NetworkDaemon
	{
	private:
#ifdef _WINDOWS
		std::vector<WSAPOLLFD> PollStruct;
#else
		std::vector<struct pollfd> PollStruct;
#endif
		std::vector<Interface*> InterfaceList;
		DFSMessaging::Messenger* NetworkMessenger;

		void IncomingConnection(Interface *aInterface);
	public:
		bool addListener(unsigned int aPort, unsigned int aBacklog, Interface *aInterface);

		void NetworkLoop();

		NetworkDaemon();
		~NetworkDaemon();
	};
}