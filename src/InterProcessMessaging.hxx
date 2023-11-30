#pragma once
#include <vector>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>

#ifdef SendMessage
#undef SendMessage
#endif

#define MESSAGE_DEBUG

#define MSG_TARGET_ALL		0	// Message goes out to everyone except for ourselves.
#define MSG_TARGET_SERVER	1	// Message goes to the MessangerServer only.

#define MESSAGE_NOMESSAGE	0

namespace DFSMessaging
{
	class Messanger;
	class MessangerServer;

	struct MessagePacket
	{
		Messanger* Origin;
		unsigned int securityKey;
		std::string channelName;
		std::string message;
	};

	class Messanger
	{
	private:
		MessangerServer* parentServer;
		unsigned int securityKey;

		std::queue<MessagePacket> MessageQueue;

	public:
		void SendMessage(std::string aChannelName, std::string aMessage);
		void SendMessage(Messanger *aMessanger, std::string aMessage);
		void RecieveMessage(MessagePacket aMessage);

		bool RegisterOnChannel(std::string aChannelName);

		Messanger(unsigned int aKey, MessangerServer *aParent);
		~Messanger();
	};

	class MessangerChannel
	{
	private:
		std::vector<Messanger*> Messangers;
		std::string ChannelName;
	public:
		// Override for == operator that compares it to a std::string of the channel name
		bool operator==(const std::string &aString) const;

		void DistributeMessage(MessagePacket aMessage);

		// Channel registration functions
		void RegisterOnChannel(Messanger* aMessanger);

		MessangerChannel(std::string);
	};

	class MessangerServer
	{
	private:
		unsigned int securityKey;
		std::vector<Messanger> Messangers;
		std::vector<MessangerChannel> Channels;
		std::condition_variable queueCondition;
		std::mutex MessageQueueMutex;
		std::queue<MessagePacket> MessageQueue;

		void MessangerServerRuntime(void);
	public:
		void DistributeMessage(MessagePacket aMessage);

		bool RegisterOnChannel(unsigned int asecurityKey, Messanger* aMessanger, std::string aChannelName);

		Messanger* ReceiveActiveMessanger(void);

		MessangerServer();
		~MessangerServer();
	};
}