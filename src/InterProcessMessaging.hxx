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

#define MSG_TARGET_ALL		1
#define MSG_TARGET_CONSOLE  2
#define MSG_TARGET_NETWORK  3

#define MESSAGE_NOMESSAGE	0

namespace DFSMessaging
{
	class Messenger;
	class MessengerServer;

	struct MessagePacket
	{
		bool isPointer;
		Messenger* Origin;
		std::string OriginName; // We need this here because sometimes the origin messenger is deleted before the message is processed.
		void* Pointer;
		unsigned int securityKey;
		unsigned int channel;
		std::string message;
	};

	class Messenger
	{
	private:
		MessengerServer* parentServer;
		unsigned int securityKey;

		std::vector<MessagePacket> MessageQueue;
		std::vector<unsigned int> RegisteredChannels;
	public:
		std::string Name;

		void SendMessage(unsigned int aChannel, std::string aMessage);
		void SendMessage(Messenger *aMessanger, std::string aMessage);
		void SendPointer(Messenger* aMessanger, void* aTransferPointer);
		void ReceiveMessage(MessagePacket aMessage);
		bool HasMessages(void);
		MessagePacket AcceptMessage(void);

		void RegisterOnChannel(unsigned int aChannel);
		bool isRegisteredOnChannel(unsigned int aChannel);
		void unRegisterAllChannels(void);

		Messenger(unsigned int aKey, MessengerServer *aParent);
		~Messenger();
	};

	class MessengerServer
	{
	private:
		unsigned int securityKey;
		std::vector<Messenger*> Messengers;
		std::condition_variable queueCondition;
		std::queue<MessagePacket> MessageQueue;

		void MessengerServerRuntime(void);
	public:
		void DistributeMessage(MessagePacket aMessage);

		bool ValidateMessenger(Messenger *aMessanger);

		Messenger* ReceiveActiveMessenger(void);
		void DeactivateActiveMessenger(Messenger* aMessanger);

		MessengerServer();
		~MessengerServer();
	};
}