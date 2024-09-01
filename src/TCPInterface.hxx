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
		SOCKET						NetworkSocket;
		struct sockaddr_in			NetworkAddress;
		DFSMessaging::Messenger		*InterfaceMessenger;
		unsigned int				listenPort;
		unsigned int				backLog;

	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog);
		TCPInterface *acceptConnection(void);

		size_t sendData(char* aData, int aLength);
		size_t receiveData(char* aData, int aLength);

		SOCKET getSocket(void) { return NetworkSocket; }
		char* getIP(void) { return inet_ntoa(NetworkAddress.sin_addr); }

		TCPInterface();
		~TCPInterface();
	};

}