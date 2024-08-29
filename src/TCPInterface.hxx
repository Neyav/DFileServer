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

namespace DFSNetworking
{

	class TCPInterface
	{
	private:
		SOCKET NetworkSocket;
		unsigned int listenPort;
		unsigned int backLog;

	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog);

		TCPInterface();
		~TCPInterface();

	};

}