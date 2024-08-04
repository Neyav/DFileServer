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

#include "InterProcessMessaging.hxx"
#include "ClientConnection.hxx"

extern DFSMessaging::MessengerServer* MessengerServer;

namespace DFSNetworking
{
	class NetworkThread
	{
	private:
		unsigned int NetworkThreadID;
		bool primeThread;

		std::vector<struct pollfd> PollStruct;
		std::vector<ClientConnection*> ConnectionList;
		DFSMessaging::Messenger* NetworkHandlerMessenger;
	public:

		NetworkThread();
		NetworkThread(bool aprimeThread);
		~NetworkThread();
	};
}