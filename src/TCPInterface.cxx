
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
		memset(&ListenAddr, 0, sizeof(struct sockaddr_in6));
		ListenAddr.sin6_family = AF_INET6;
		ListenAddr.sin6_port = htons(aPort);
		ListenAddr.sin6_addr = in6addr_any;

		// Bind the socket to the listening network struct.
		if (bind(NetworkSocket, (struct sockaddr*)&ListenAddr,
			sizeof(struct sockaddr_in6)) == -1)
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

#ifdef _DFS_USE_OPENSSL

	void generate_self_signed_cert(EVP_PKEY** pkey, X509** x509) 
	{
		*pkey = EVP_PKEY_new();
		EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
		EVP_PKEY_keygen_init(pctx);
		EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048);
		EVP_PKEY_keygen(pctx, pkey);
		EVP_PKEY_CTX_free(pctx);

		*x509 = X509_new();
		X509_set_version(*x509, 2);
		ASN1_INTEGER_set(X509_get_serialNumber(*x509), 0);
		X509_gmtime_adj(X509_get_notBefore(*x509), 0);
		X509_gmtime_adj(X509_get_notAfter(*x509), 31536000L); // 1 year
		X509_set_pubkey(*x509, *pkey);

		X509_NAME* name = X509_get_subject_name(*x509);
		X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"US", -1, -1, 0);
		X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"My Company", -1, -1, 0);
		X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);
		X509_set_issuer_name(*x509, name);

		X509_sign(*x509, *pkey, EVP_sha256());
	}

	bool HTTPSIPv4Interface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
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
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "Initializing HTTPS IPv4 Interface...");

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
		InterfaceMessenger->Name = "HTTPS IPv4 Interface: " + std::string(inet_ntoa(ListenAddr.sin_addr)) + ":" + std::to_string(aPort);
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "HTTPS IPv4 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;


		method = TLS_server_method();
		ctx = SSL_CTX_new(method);

		generate_self_signed_cert(&pkey, &x509);

		SSL_CTX_use_certificate(ctx, x509);
		SSL_CTX_use_PrivateKey(ctx, pkey);

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "SSL Certificate generated and self signed.");

		return true;
	}

	TCPInterface* HTTPSIPv4Interface::acceptConnection(void)
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

		HTTPSIPv4Interface* NewInterface = new HTTPSIPv4Interface;
		NewInterface->NetworkSocket = NewSocket;
		NewInterface->NetworkAddress = SocketStruct;
		NewInterface->listenPort = listenPort;
		NewInterface->backLog = backLog;
		NewInterface->ctx = ctx;
		NewInterface->pkey = pkey;
		NewInterface->x509 = x509;
		NewInterface->method = method;

		NewInterface->ssl = SSL_new(ctx);

		if (!NewInterface->ssl) {
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "SSL_new failed!");
			delete NewInterface;
			return nullptr;
		}

		SSL_set_fd(NewInterface->ssl, NewSocket);

		if (SSL_accept(NewInterface->ssl) <= 0)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, "SSL Accept failed!");
			//ERR_print_errors_fp(stderr); // Print OpenSSL error messages

			delete NewInterface;
			return nullptr;
		}

		return NewInterface;
	}

	HTTPSIPv4Interface::HTTPSIPv4Interface()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;
		ssl = nullptr;

		if (MessengerServer)
		{
			//InterfaceMessenger = MessengerServer->ReceiveActiveMessenger();
			InterfaceMessenger->Name = "HTTPS IPv4 Interface";
		}
		else
		{
			InterfaceMessenger = nullptr;
		}
	}

	size_t HTTPSIPv4Interface::sendData(char* aData, int aLength)
	{
		return SSL_write(ssl, aData, aLength);
	}

	size_t HTTPSIPv4Interface::receiveData(char* aData, int aLength)
	{
		int bytes_read;

		bytes_read = SSL_read(ssl, aData, aLength - 1);

		return (size_t)bytes_read;
	}

	HTTPSIPv4Interface::~HTTPSIPv4Interface()
	{
		if (ssl)
		{
			SSL_shutdown(ssl);
			SSL_free(ssl);
		}

	/*	// Close the socket.
#ifdef _WINDOWS
		if (closesocket(NetworkSocket) == -1)
#else
		if (close(NetworkSocket) == -1)
#endif
		{
			perror("HTTPSIPv4Interface::~HTTPSIPv4Interface -- close()");
		}

		if (InterfaceMessenger != nullptr)
		{
			delete InterfaceMessenger;
			InterfaceMessenger = nullptr;
		}*/
	}
#endif
}