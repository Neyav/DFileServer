#ifdef _WINDOWS
#define socklen_t int
#endif

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
		InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "Initializing IPv4 Interface...");

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
		InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "IPv4 Interface initialized.");

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
			InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "TCPInterface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "TCPInterface::acceptConnection -- Connection accepted from " + std::string(inet_ntoa(SocketStruct.sin_addr)));

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
			InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "TCPInterface::sendData -- send() failed.");
			return 0;
		}

		return DataSent;
	}
	
	size_t TCPInterface::receiveData(char* aData, int aLength)
	{
		size_t DataRecv;

		if ((DataRecv = recv(NetworkSocket, aData, aLength - 1, 0)) == -1)
		{
			InterfaceMessenger->SendMessage(MSG_TARGET_CONSOLE, "TCPInterface::receiveData -- recv() failed.");
			return -1;
		}

		return DataRecv;
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
}