#pragma once
#include <vector>
#include <condition_variable>
#include <mutex>

#define MESSAGE_DEBUG

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

		bool RegisterOnChannel(std::string aChannelName);

		void PokeServer(void); // Testing function.

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

		void MessangerServerRuntime(void);
	public:
		std::condition_variable queueCondition;
		
		bool RegisterOnChannel(unsigned int asecurityKey, Messanger* aMessanger, std::string aChannelName);

		Messanger* ReceiveActiveMessanger(void);

		MessangerServer();
		~MessangerServer();
	};
}