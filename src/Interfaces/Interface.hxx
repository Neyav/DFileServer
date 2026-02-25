#pragma once

#ifdef _WINDOWS
#define WIN32_LEAN_AND_MEAN

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#ifdef _DFS_USE_OPENSSL
#pragma comment(lib, "libssl_static.lib")
#pragma comment(lib, "libcrypto_static.lib")
#endif
#else
#define SOCKET int
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif

#ifdef _DFS_USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#endif

#include "../InterProcessMessaging.hxx"

namespace DFSNetworking
{

	class Interface
	{
	protected:
		SOCKET						NetworkSocket;
		DFSMessaging::Messenger		*InterfaceMessenger;
		unsigned int				listenPort;
		unsigned int				backLog;

	public:
		virtual bool initializeInterface(unsigned int aPort, unsigned int aBackLog);
		virtual Interface *acceptConnection(void);

		virtual size_t sendData(char* aData, int aLength);
		virtual size_t receiveData(char* aData, int aLength);

		SOCKET getSocket(void) { return NetworkSocket; };
		virtual char* getIP(void);

		Interface();
		~Interface();
	};

	class IPv4Interface : public Interface
	{
	protected:		
		struct sockaddr_in			NetworkAddress;
	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog) override;
		Interface* acceptConnection(void) override;

		char* getIP(void) override;

		IPv4Interface();
		~IPv4Interface();
	};

	class IPv6Interface : public Interface
	{
	private:
		struct sockaddr_in6			NetworkAddress;

	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog) override;
		Interface* acceptConnection(void) override;

		char* getIP(void) override;

		IPv6Interface();
		~IPv6Interface();
	};


#ifdef _DFS_USE_OPENSSL

	class HTTPSIPv4Interface : public IPv4Interface
	{
	private:
		const SSL_METHOD* method;
		SSL_CTX* ctx;

		EVP_PKEY* pkey = nullptr;
		X509* x509 = nullptr;

		SSL* ssl;
	public:
		bool initializeInterface(unsigned int aPort, unsigned int aBackLog) override;
		TCPInterface *acceptConnection(void) override;

		virtual size_t sendData(char* aData, int aLength) override;
		virtual size_t receiveData(char* aData, int aLength) override;

		HTTPSIPv4Interface();
		~HTTPSIPv4Interface();
	};

#endif
}