#pragma once
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>
#include <atomic>

#ifdef SendMessage
#undef SendMessage
#endif

#define MESSAGE_DEBUG

#define MSG_TARGET_USER		0	// Message is for a particular messanger only.
#define MSG_TARGET_ALL		1
#define MSG_TARGET_CONSOLE  2
#define MSG_TARGET_NETWORK  3

#define MESSAGE_NOMESSAGE	0

namespace DFSMessaging
{
	class Messanger;
	class MessangerServer;

	struct MessagePacket
	{
		bool isPointer;
		Messanger* Origin;
		void* Pointer;
		unsigned int securityKey;
		unsigned int channel;
		std::string message;
	};

	class Messanger
	{
	private:
		MessangerServer* parentServer;
		unsigned int securityKey;

		std::vector<MessagePacket> MessageQueue;
		std::vector<unsigned int> RegisteredChannels;
	public:
		std::string Name;

		void SendMessage(unsigned int aChannel, std::string aMessage);
		void SendMessage(Messanger *aMessanger, std::string aMessage);
		void SendPointer(Messanger* aMessanger, void* aTransferPointer);
		void RecieveMessage(MessagePacket aMessage);
		bool HasMessages(void);
		MessagePacket AcceptMessage(void);

		void RegisterOnChannel(unsigned int aChannel);
		bool isRegisteredOnChannel(unsigned int aChannel);
		void unRegisterAllChannels(void);

		Messanger(unsigned int aKey, MessangerServer *aParent);
		~Messanger();
	};

	class MessangerServer
	{
	private:
		unsigned int securityKey;
		std::vector<Messanger*> Messangers;
		std::condition_variable queueCondition;
		std::queue<MessagePacket> MessageQueue;

		void MessangerServerRuntime(void);
	public:
		void DistributeMessage(MessagePacket aMessage);

		Messanger* ReceiveActiveMessanger(void);
		void DeactivateActiveMessanger(Messanger* aMessanger);

		MessangerServer();
		~MessangerServer();
	};
}