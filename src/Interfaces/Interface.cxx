
#include <string>

#include "Interface.hxx"

namespace DFSNetworking
{
	bool Interface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
	{
		return false;
	}

	Interface* Interface::acceptConnection(void)
	{
		return nullptr;
	}

	size_t Interface::sendData(char* aData, int aLength)
	{
		size_t DataSent;

		if ((DataSent = send(NetworkSocket, aData, aLength, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "TCPInterface::sendData -- send() failed.");
			return 0;
		}

		return DataSent;
	}
	
	size_t Interface::receiveData(char* aData, int aLength)
	{
		size_t DataRecv;

		if ((DataRecv = recv(NetworkSocket, aData, aLength - 1, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "TCPInterface::receiveData -- recv() failed.");
			return -1;
		}

		return DataRecv;
	}	

	char *Interface::getIP(void)
	{
		return nullptr;
	}

	Interface::Interface()
	{
		listenPort = 0;
		backLog = 0;
		NetworkSocket = 0;

		if (MessengerServer)
		{
			InterfaceMessenger = MessengerServer->ReceiveActiveMessenger();
			InterfaceMessenger->Name = "TCP Interface";
		}
		else
		{
			InterfaceMessenger = nullptr;
		}
		
	}

	Interface::~Interface()
	{
		// Close the socket.
#ifdef _WINDOWS
		if (closesocket(NetworkSocket) == -1)
#else
		if (close(NetworkSocket) == -1)
#endif
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "TCPInterface::~TCPInterface -- close() failed.");
		}

		if (InterfaceMessenger != nullptr)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_SYSTEM, "Interface shutdown.");
			delete InterfaceMessenger;
		}
	}

	bool IPv4Interface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
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
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Initializing IPv4 Interface...");

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
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "IPv4 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;

		return true;
	}

	Interface* IPv4Interface::acceptConnection(void)
	{
		socklen_t sin_size = sizeof(struct sockaddr_in);
		SOCKET NewSocket;
		struct sockaddr_in SocketStruct;

		if ((NewSocket = accept(NetworkSocket, (struct sockaddr*)&SocketStruct,
			&sin_size)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "TCPInterface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "TCPInterface::acceptConnection -- Connection accepted from " + std::string(inet_ntoa(SocketStruct.sin_addr)));

		IPv4Interface* NewInterface = new IPv4Interface;
		NewInterface->NetworkSocket = NewSocket;
		NewInterface->NetworkAddress = SocketStruct;
		NewInterface->listenPort = listenPort;
		NewInterface->backLog = backLog;

		return NewInterface;
	}

	char* IPv4Interface::getIP(void)
	{
		return inet_ntoa(NetworkAddress.sin_addr);
	}

	IPv4Interface::IPv4Interface()
	{
		if (InterfaceMessenger)
		{		
			InterfaceMessenger->Name = "IPv4 Interface";
		}		
	}

	IPv4Interface::~IPv4Interface()
	{

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
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Initializing IPv6 Interface...");

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- socket() failed.");			
			return false;
		}

		// Clear the socket incase it hasn't been properly closed so that we may use it.
		if (setsockopt(NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- setsockopt() failed.");			
			return false;
		}

#ifndef _WINDOWS
		// Set the socket to allow dual-stack (IPv4 and IPv6) connections.
		if (setsockopt(NetworkSocket, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(int)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- setsockopt(IPV6_V6ONLY) failed.");
			return false;
		}
#endif

		// Set the listening network struct up.
		memset(&ListenAddr, 0, sizeof(struct sockaddr_in6));
		ListenAddr.sin6_family = AF_INET6;
		ListenAddr.sin6_port = htons(aPort);
		ListenAddr.sin6_addr = in6addr_any;

		// Bind the socket to the listening network struct.
		if (bind(NetworkSocket, (struct sockaddr*)&ListenAddr,
			sizeof(struct sockaddr_in6)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- bind() failed.");			
			return false;
		}

		// Start listening
		if (listen(NetworkSocket, aBackLog) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- listen() failed.");
			return false;
		}

		// Change the name of the Messenger to include the IP address and port.
		InterfaceMessenger->Name = "IPv6 Interface: " + std::string(this->getIP()) + ":" + std::to_string(aPort);
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "IPv6 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;

		return true;
	}

	Interface* IPv6Interface::acceptConnection(void)
	{
		socklen_t sin_size = sizeof(struct sockaddr_in6);
		SOCKET NewSocket;
		struct sockaddr_in6 SocketStruct;

		if ((NewSocket = accept(NetworkSocket, (struct sockaddr*)&SocketStruct,
			&sin_size)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "IPv6Interface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "IPv6Interface::acceptConnection -- Connection accepted from " + std::string(this->getIP()));

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

	}

#ifdef _DFS_USE_OPENSSL

	bool generate_self_signed_cert(EVP_PKEY** pkey, X509** x509)
	{
		*pkey = EVP_PKEY_new();
		if (!*pkey) {
			return false;
		}

		EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
		if (!pctx) {
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (EVP_PKEY_keygen_init(pctx) <= 0) {
			EVP_PKEY_CTX_free(pctx);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
			EVP_PKEY_CTX_free(pctx);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (EVP_PKEY_keygen(pctx, pkey) <= 0) {
			EVP_PKEY_CTX_free(pctx);
			EVP_PKEY_free(*pkey);
			return false;
		}

		EVP_PKEY_CTX_free(pctx);

		*x509 = X509_new();
		if (!*x509) {
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_set_version(*x509, 2) != 1) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (ASN1_INTEGER_set(X509_get_serialNumber(*x509), 0) != 1) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_gmtime_adj(X509_get_notBefore(*x509), 0) == nullptr) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_gmtime_adj(X509_get_notAfter(*x509), 31536000L) == nullptr) { // 1 year
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_set_pubkey(*x509, *pkey) != 1) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		X509_NAME* name = X509_get_subject_name(*x509);
		if (!name) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"US", -1, -1, 0) != 1 ||
			X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"My Company", -1, -1, 0) != 1 ||
			X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0) != 1) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_set_issuer_name(*x509, name) != 1) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		if (X509_sign(*x509, *pkey, EVP_sha256()) == 0) {
			X509_free(*x509);
			EVP_PKEY_free(*pkey);
			return false;
		}

		return true;
	}

	bool HTTPSIPv4Interface::initializeInterface(unsigned int aPort, unsigned int aBackLog)
	{
		struct sockaddr_in ListenAddr;
		const char yes = 1;
#ifdef _WINDOWS
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "Failed to initialize WSAStartup.");
			return false;
		}
#endif
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Initializing HTTPS IPv4 Interface...");

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- socket() failed.");
			return false;
		}

		// Clear the socket incase it hasn't been properly closed so that we may use it.
		if (setsockopt(NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- setsockopt() failed.");
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
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- bind() failed.");
			return false;
		}

		// Start listening
		if (listen(NetworkSocket, aBackLog) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "InitializeNetwork -- listen() failed.");
			return false;
		}

		// Change the name of the Messenger to include the IP address and port.
		InterfaceMessenger->Name = "HTTPS IPv4 Interface: " + std::string(inet_ntoa(ListenAddr.sin_addr)) + ":" + std::to_string(aPort);
		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "HTTPS IPv4 Interface initialized.");

		listenPort = aPort;
		backLog = aBackLog;
		NetworkAddress = ListenAddr;


		method = TLS_server_method();
		ctx = SSL_CTX_new(method);

		if (!generate_self_signed_cert(&pkey, &x509))
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "Failed to generate self signed certificate.");
			return false;
		}

		SSL_CTX_use_certificate(ctx, x509);
		SSL_CTX_use_PrivateKey(ctx, pkey);

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_ALL, "SSL Certificate generated and self signed.");

		return true;
	}

	Interface* HTTPSIPv4Interface::acceptConnection(void)
	{
		socklen_t sin_size = sizeof(struct sockaddr_in);
		SOCKET NewSocket;
		struct sockaddr_in SocketStruct;

		if ((NewSocket = accept(NetworkSocket, (struct sockaddr*)&SocketStruct,
			&sin_size)) == -1)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "HTTPSIPv4Interface::acceptConnection -- accept() failed.");

			return nullptr;
		}

		InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "HTTPSIPv4Interface::acceptConnection -- Connection accepted from " + std::string(inet_ntoa(SocketStruct.sin_addr)));

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
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "SSL_new failed!");
			delete NewInterface;
			return nullptr;
		}

		SSL_set_fd(NewInterface->ssl, (int) NewSocket);

		if (SSL_accept(NewInterface->ssl) <= 0)
		{
			InterfaceMessenger->sendMessage(MSG_TARGET_CONSOLE, VISIBILITY_DEBUG, "SSL Accept failed!");
			//ERR_print_errors_fp(stderr); // Print OpenSSL error messages

			delete NewInterface;
			return nullptr;
		}

		return NewInterface;
	}

	HTTPSIPv4Interface::HTTPSIPv4Interface()
	{
		ssl = nullptr;

		if (InterfaceMessenger)
		{
			InterfaceMessenger->Name = "HTTPS IPv4 Interface";
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
	}
#endif
}