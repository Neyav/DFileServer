#ifndef _WINDOWS
#include <netinet/in.h>
#define SOCKET int
#endif

#include <time.h>
#include <fstream>
#include <string>

#include "HTTPHeader.hxx"
#include "InterProcessMessaging.hxx"

extern DFSMessaging::MessengerServer* MessengerServer;

class ClientConnection
{
	private:
		SOCKET				NetworkSocket;
		struct sockaddr_in	SocketStruct;
		time_t				LastAction;
		DFSMessaging::Messenger* Messanger;
	public:	

		std::ifstream		*FileStream;
		std::string			Resource;
		size_t				BytesRemaining;

		int					BandwidthLeft;
		time_t				LastBandReset;

		std::string			SendBuffer;
		int					SendBufferIterator;

		HTTPHeader			BrowserRequest;
		HTTPHeader			ServerResponse;

		ClientConnection ();		// Constructor
		~ClientConnection();
		SOCKET GetSocket ( void );
		char *GetIP ( void );
		size_t OpenFile ( char * );
		void CloseFile ( void );
		char AcceptConnection ( int );
		void DisconnectClient ( void );
		int SendData ( char *, int );
		size_t RecvData ( char *, size_t );
		time_t SecondsIdle ( void );
};
