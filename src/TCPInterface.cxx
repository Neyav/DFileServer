#include <iostream>

#include "TCPInterface.hxx"

namespace DFSNetworking
{
	bool TCPInterface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
	{
		struct sockaddr_in ListenAddr;
		const char yes = 1;
#ifdef _WINDOWS
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			perror("InitializeNetwork -- WSAStartup()");
			return false;
		}
#endif
		std::cout << " -=Initializing IPv4 Interface..." << std::endl;

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror("InitializeNetwork -- socket()");
			return false;
		}

		// Clear the socket incase it hasn't been properly closed so that we may use it.
		if (setsockopt(NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("InitializeNetwork -- setsockopt()");
			return false;
		}

		// Set the listening network struct up.
		ListenAddr.sin_family = AF_INET;
		ListenAddr.sin_port = htons(aPort);
		ListenAddr.sin_addr.s_addr = INADDR_ANY;
		memset(&(ListenAddr.sin_zero), '\0', 8);

		// Bind the socket to the listening network struct.
		if (bind(NetworkSocket, (struct sockaddr*)&ListenAddr,
			sizeof(struct sockaddr)) == -1)
		{
			perror("InitializeNetwork -- bind()");
			return false;
		}

		// Start listening
		if (listen(NetworkSocket, aBackLog) == -1)
		{
			perror("InitializeNetwork -- listen()");
			return false;
		}

		listenPort = aPort;
		backLog = aBackLog;

		return true;
	}

	TCPInterface::TCPInterface()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;
	}

	TCPInterface::~TCPInterface()
	{
		// Close the socket.
#ifdef _WINDOWS
		if (closesocket(NetworkSocket) == -1)
#else
		if (close(NetworkSocket) == -1)
#endif
		{
			perror("TCPInterface::~TCPInterface -- close()");
		}
	}
}