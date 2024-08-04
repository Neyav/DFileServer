
#include "NetworkThread.hxx"

namespace DFSNetworking
{
	NetworkThread::NetworkThread()
	{
		NetworkThreadID = (unsigned int)this;
		primeThread = false;
	}

	NetworkThread::NetworkThread(bool aprimeThread)
	{
		NetworkThreadID = (unsigned int)this;
		primeThread = aprimeThread;
	}

	NetworkThread::~NetworkThread()
	{

	}
}