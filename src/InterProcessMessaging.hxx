#pragma once
#include <vector>
#include <condition_variable>
#include <mutex>

#define MSG_TARGET_ALL		0	// Message goes out to everyone except for ourselves.
#define MSG_TARGET_SERVER	1	// Message goes to the MessangerServer only.

#define MESSAGE_NOMESSAGE	0

namespace DFSMessaging
{
	class MessangerServer;

	class Messanger
	{
	private:
		MessangerServer* parentServer;
		unsigned int securityKey;
	public:

		void PokeServer(void); // Testing function.

		Messanger(unsigned int aKey, MessangerServer *aParent);
		~Messanger();
	};

	class MessangerServer
	{
	private:
		unsigned int securityKey;
		
		std::vector<Messanger> Messangers;

		void MessangerServerRuntime(void);
	public:
		std::condition_variable queueCondition;
		

		Messanger* ReceiveActiveMessanger(void);

		MessangerServer();
		~MessangerServer();
	};
}