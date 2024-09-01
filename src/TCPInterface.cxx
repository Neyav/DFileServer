
#include <string>

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
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "Initializing IPv4 Interface...");

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

		// Change the name of the Messenger to include the IP address and port.
		InterfaceMessenger->Name = "IPv4 Interface: " + std::string(inet_ntoa(ListenAddr.sin_addr)) + ":" + std::to_string(aPort);
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "IPv4 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;

		return true;
	}

	TCPInterface* TCPInterface::acceptConnection(void)
	{
		socklen_t sin_size = sizeof(struct sockaddr_in);
		SOCKET NewSocket;
		struct sockaddr_in SocketStruct;

		if ((NewSocket = accept(NetworkSocket, (struct sockaddr*)&SocketStruct,
			&sin_size)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "TCPInterface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "TCPInterface::acceptConnection -- Connection accepted from " + std::string(inet_ntoa(SocketStruct.sin_addr)));

		TCPInterface* NewInterface = new TCPInterface;
		NewInterface->NetworkSocket = NewSocket;
		NewInterface->NetworkAddress = SocketStruct;
		NewInterface->listenPort = listenPort;
		NewInterface->backLog = backLog;

		return NewInterface;
	}

	size_t TCPInterface::sendData(char* aData, int aLength)
	{
		size_t DataSent;

		if ((DataSent = send(NetworkSocket, aData, aLength, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "TCPInterface::sendData -- send() failed.");
			return 0;
		}

		return DataSent;
	}
	
	size_t TCPInterface::receiveData(char* aData, int aLength)
	{
		size_t DataRecv;

		if ((DataRecv = recv(NetworkSocket, aData, aLength - 1, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "TCPInterface::receiveData -- recv() failed.");
			return -1;
		}

		return DataRecv;
	}	

	char *TCPInterface::getIP(void)
	{
		return inet_ntoa(NetworkAddress.sin_addr);
	}

	TCPInterface::TCPInterface()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;

		if (MessengerServer)
		{
			InterfaceMessenger = MessengerServer->ReceiveActiveMessenger();
			InterfaceMessenger->Name = "IPv4 Interface";
		}
		else
		{
			InterfaceMessenger = nullptr;
		}
		
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

		if (InterfaceMessenger != nullptr)
			delete InterfaceMessenger;
	}

	bool IPv6Interface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
	{
		struct sockaddr_in6 ListenAddr;
		const char yes = 1;
#ifdef _WINDOWS
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			perror("InitializeNetwork -- WSAStartup()");
			return false;
		}
#endif
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "Initializing IPv6 Interface...");

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
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
		ListenAddr.sin6_family = AF_INET6;
		ListenAddr.sin6_port = htons(aPort);
		ListenAddr.sin6_addr = in6addr_any;

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

		// Change the name of the Messenger to include the IP address and port.
		InterfaceMessenger->Name = "IPv6 Interface: " + std::string(this->getIP()) + ":" + std::to_string(aPort);
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "IPv6 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;

		return true;
	}

	TCPInterface* IPv6Interface::acceptConnection(void)
	{
		socklen_t sin_size = sizeof(struct sockaddr_in6);
		SOCKET NewSocket;
		struct sockaddr_in6 SocketStruct;

		if ((NewSocket = accept(NetworkSocket, (struct sockaddr*)&SocketStruct,
			&sin_size)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "IPv6Interface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "IPv6Interface::acceptConnection -- Connection accepted from " + std::string(this->getIP()));

		IPv6Interface* NewInterface = new IPv6Interface;
		NewInterface->NetworkSocket = NewSocket;
		NewInterface->NetworkAddress = SocketStruct;
		NewInterface->listenPort = listenPort;
		NewInterface->backLog = backLog;

		return NewInterface;
	}

	char *IPv6Interface::getIP(void)
	{
		char *IPString = new char[INET6_ADDRSTRLEN];

		inet_ntop(AF_INET6, &NetworkAddress.sin6_addr, IPString, INET6_ADDRSTRLEN);

		return IPString;
	}

	IPv6Interface::IPv6Interface()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;

		if (MessengerServer)
		{
			InterfaceMessenger = MessengerServer->ReceiveActiveMessenger();
			InterfaceMessenger->Name = "IPv6 Interface";
		}
		else
		{
			InterfaceMessenger = nullptr;
		}
	}

	IPv6Interface::~IPv6Interface()
	{
		// Close the socket.
#ifdef _WINDOWS
		if (closesocket(NetworkSocket) == -1)
#else
		if (close(NetworkSocket) == -1)
#endif
		{
			perror("IPv6Interface::~IPv6Interface -- close()");
		}

		if (InterfaceMessenger != nullptr)
			delete InterfaceMessenger;
	}
}