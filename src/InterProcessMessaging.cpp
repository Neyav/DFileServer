#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"

namespace DFSMessaging
{
	bool Messanger::RegisterOnChannel(std::string aChannelName)
	{
		return parentServer->RegisterOnChannel(securityKey, this, aChannelName);
	}

    void Messanger::SendMessage(std::string aChannelName, std::string aMessage)
	{
		MessagePacket newMessage;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.channelName = aChannelName;

		parentServer->DistributeMessage(newMessage);
	}

	void Messanger::SendMessage(Messanger* aMessanger, std::string aMessage)
	{
		MessagePacket newMessage;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.channelName = "";

		aMessanger->RecieveMessage(newMessage);
	}
	
	void Messanger::RecieveMessage(MessagePacket aMessage)
	{
		if (aMessage.securityKey != securityKey)
		{
			return;
		}
		// Add it to our message queue.
		MessageQueue.push(aMessage);
	}

	Messanger::Messanger(unsigned int aKey, MessangerServer *aParent)
	{
		securityKey = aKey;
		parentServer = aParent;
	}
	Messanger::~Messanger()
	{

	}
	
	void MessangerChannel::DistributeMessage(MessagePacket aMessage)
	{
		for (auto &messanger : Messangers)
		{
			if (messanger != aMessage.Origin)
			{
				messanger->RecieveMessage(aMessage);
			}
		}
	
	}

	void MessangerChannel::RegisterOnChannel(Messanger* aMessanger)
	{
		Messangers.push_back(aMessanger);
	}

	bool MessangerChannel::operator==(const std::string &aOther) const
	{
		return (ChannelName == aOther);
	}

	MessangerChannel::MessangerChannel(std::string aName)
	{
		ChannelName = aName;
#ifdef MESSAGE_DEBUG
		std::cout << " -=Messanger Channel created. [" << ChannelName << "]" << std::endl;
#endif
	}

	void MessangerServer::DistributeMessage(MessagePacket aMessage)
	{
		MessageQueueMutex.lock();
		MessageQueue.push(aMessage);
		MessageQueueMutex.unlock();
		queueCondition.notify_one();
	}

	bool MessangerServer::RegisterOnChannel(unsigned int asecurityKey, Messanger* aMessanger, std::string aChannelName)
	{
		if (asecurityKey != securityKey)
		{
			return false;
		}

		for (auto &channel : Channels)
		{
			if (channel == aChannelName)
			{
				channel.RegisterOnChannel(aMessanger);
				return true;
			}
		}

		MessangerChannel newChannel(aChannelName);
		newChannel.RegisterOnChannel(aMessanger);
		Channels.push_back(newChannel);

		return true;
	}

	void MessangerServer::MessangerServerRuntime(void)
	{
		std::mutex queueMutex;
		std::unique_lock<std::mutex> queueLock(queueMutex);

		std::cout << " -=Messanger Server Runtime started." << std::endl;

		while (true)
		{
			// Do stuff here.
			queueCondition.wait(queueLock);
			std::cout << " -=Messanger Server Runtime woke up." << std::endl;

			// Distribute any messages in the queue.
			MessageQueueMutex.lock();
			while (MessageQueue.size() > 0)
			{
				MessagePacket newMessage = MessageQueue.front();
				MessageQueue.pop();

				if (newMessage.channelName == "*")
				{ // Send to all clients
					// TODO:
				}
				else
				{ // Send to specific channel 
					for (auto& channel : Channels)
					{
						if (channel == newMessage.channelName)
						{
							channel.DistributeMessage(newMessage);
						}
					}
				}
			}
			MessageQueueMutex.unlock();
		}
	}

	Messanger* MessangerServer::ReceiveActiveMessanger(void)
	{
		Messanger newMessager(securityKey, this);

		Messangers.push_back(newMessager);

		return &Messangers.back();
	}

	MessangerServer::MessangerServer()
	{
		// Initalize Messanger Server
		std::cout << " -=Messaging Service Initalizing..." << std::endl;

		std::random_device rd;
		std::mt19937 gen(rd());
		
		std::uniform_int_distribution<> dis(0, 1073741824);

		securityKey = dis(gen);
	
		std::cout << " -=Messanger Service Security Key generated. [" << securityKey << "]" << std::endl;

		// Start Messanger Server Runtime
		std::thread MessangerServerThread(&MessangerServer::MessangerServerRuntime, this);
		MessangerServerThread.detach();
	}
	MessangerServer::~MessangerServer()
	{
		// Destroy Messanger Server
		// Start by destrying all Messangers
		std::cout << " -=Messanging Service shutting down..." << std::endl;

		while (Messangers.size() > 0)
		{
			Messangers.pop_back();
		}

		std::cout << " -=Messanging Service shutdown complete." << std::endl;
	}
}