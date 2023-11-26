#ifndef _WINDOWS
#include <netinet/in.h>
#endif

#include <time.h>
#include <fstream>
#include <string>

#include "HTTPHeader.hxx"

class ClientConnection
{
	private:
		int			NetworkSocket;
		struct sockaddr_in	SocketStruct;
		time_t			LastAction;
	public:
		std::ifstream		*FileStream;
		std::string             Resource;
		int			BytesRemaining;
		int			PollIterator;

		int			BandwidthLeft;
		time_t			LastBandReset;

		std::string		SendBuffer;
		int			SendBufferIterator;

		HTTPHeader		BrowserRequest;
		HTTPHeader		ServerResponse;

		ClientConnection ();		// Constructor
		int  GetSocket ( void );
		char *GetIP ( void );
		int OpenFile ( char * );
		void CloseFile ( void );
		char AcceptConnection ( int );
		void DisconnectClient ( void );
		int SendData ( char *, int );
		size_t RecvData ( char *, size_t );
		time_t SecondsIdle ( void );
};
