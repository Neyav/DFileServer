#pragma once
#ifdef _WINDOWS
#include <winsock.h>
#include "contrib/fakepoll.h"

#pragma comment(lib, "Ws2_32.lib")
#else
#define SOCKET int
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif

#include "InterProcessMessaging.hxx"

namespace DFSNetworking
{

	class TCPInterface
	{
	private:
		SOCKET NetworkSocket;
		DFSMessaging::Messenger* InterfaceMessenger;
		unsigned int listenPort;
		unsigned int backLog;

	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog);

		SOCKET getSocket(void) { return NetworkSocket; }

		TCPInterface();
		~TCPInterface();
	};

}