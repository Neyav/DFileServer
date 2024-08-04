
#include "NetworkThread.hxx"

namespace DFSNetworking
{
	NetworkThread::NetworkThread()
	{
		NetworkThreadID = (size_t)this;
		primeThread = false;
	}

	NetworkThread::NetworkThread(bool aprimeThread)
	{
		NetworkThreadID = (size_t)this;
		primeThread = aprimeThread;
	}

	NetworkThread::~NetworkThread()
	{

	}
}