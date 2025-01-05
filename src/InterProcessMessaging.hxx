#pragma once
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>
#include <atomic>

#define MESSAGE_DEBUG

#define MSG_TARGET_ALL		 1
#define MSG_TARGET_CONSOLE   2
#define MSG_TARGET_NETWORK   3
#define MSG_TARGET_INTERFACE 4

#define VISIBILITY_ALL		 0
#define VISIBILITY_VERBOSE	 1
#define VISIBILITY_DEBUG	 2

#define MESSAGE_NOMESSAGE    0

namespace DFSMessaging
{
	class Messenger;
	class MessengerServer;

	class Message
	{
	private:		
		bool isPointer;
		bool isTask;
		unsigned int visibility;
		unsigned int securityKey;
		unsigned int messageID;
		Messenger* Origin;
		unsigned int Pending; // This is how many copies of this message are out there waiting to be processed.
		
		friend class MessengerServer;
	public:
		unsigned int sendTime;
		std::string OriginName; // We need this here because sometimes the origin messenger is deleted before the message is processed.
		void* Pointer;
		
		unsigned int channel;
		std::string message;

		Messenger *identifyOrigin(void);
		bool acceptTask(void);
		void refuseTask(void);

		Message(bool aisPointer, bool aisTask, Messenger *aOrigin, unsigned int asecurityKey, unsigned int aVisibility);
		Message();
		~Message();
		
	};

	class Messenger
	{
	private:
		MessengerServer* parentServer;
		unsigned int securityKey;
		std::condition_variable messengerCondition;

		std::vector<unsigned int> waitingMessageIDs;
		std::vector<unsigned int> RegisteredChannels;

		friend class Message;
	public:
		std::string Name;

		void AlertMessageID(unsigned int aMessageID);
		void pauseForMessage(unsigned int aTimeout = 0);
		void sendMessage(unsigned int aChannel, unsigned int aVisibility, std::string aMessage);
		void SendPointer(unsigned int aChannel, void* aPointer);
		bool HasMessages(void);
		Message AcceptMessage(void);

		void RegisterOnChannel(unsigned int aChannel);
		bool isRegisteredOnChannel(unsigned int aChannel);
		void unRegisterAllChannels(void);

		Messenger(unsigned int aKey, MessengerServer *aParent);
		~Messenger();
	};

	class MessengerServer
	{
	private:
		bool serverShutdown;
		unsigned int securityKey;
		unsigned int nextMessageID;
		std::vector<Messenger*> Messengers;
		std::condition_variable queueCondition;
		std::queue<Message> MessageQueue;
		std::unordered_map<unsigned int, Message> sentMessages;

		void PruneOldMessages(unsigned int atimeout);

		void MessengerServerRuntime(void);

		friend class Message;
	public:
		void DistributeMessage(Message aMessage);
		bool ValidateMessenger(Messenger *aMessanger);

		Message GetMessage(unsigned int aMessageID);

		void ShutdownServer(void);

		Messenger* ReceiveActiveMessenger(void);
		void DeactivateActiveMessenger(Messenger* aMessanger);

		MessengerServer();
		~MessengerServer();
	};
}

extern DFSMessaging::MessengerServer* MessengerServer;