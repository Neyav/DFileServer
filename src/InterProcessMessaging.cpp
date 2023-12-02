#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"


namespace DFSMessaging
{
	std::mutex queueMutex;
	std::mutex channelMutex;

	void Messanger::RegisterOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		channelMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				return;
			}
		}

		// If we are not, register on it.
		RegisteredChannels.push_back(aChannel);
		channelMutex.unlock();
	}

	bool Messanger::isRegisteredOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		channelMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				channelMutex.unlock();
				return true;
			}
		}
		channelMutex.unlock();
	}

    void Messanger::SendMessage(unsigned int aChannel, std::string aMessage)
	{
		MessagePacket newMessage;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.channel = aChannel;

		parentServer->DistributeMessage(newMessage);
	}

	void Messanger::SendMessage(Messanger* aMessanger, std::string aMessage)
	{
		MessagePacket newMessage;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.channel = 0;

		aMessanger->RecieveMessage(newMessage);
	}
	
	void Messanger::RecieveMessage(MessagePacket aMessage)
	{
		if (aMessage.securityKey != securityKey)
		{
			return;
		}
		// Add it to our message queue.
		queueMutex.lock();
		MessageQueue.push_back(aMessage);
		queueMutex.unlock();
	}

	MessagePacket Messanger::AcceptMessage(void)
	{
		MessagePacket newMessage;
		queueMutex.lock();
		if (MessageQueue.size() > 0)
		{
			newMessage = MessageQueue.front();
			MessageQueue.erase(MessageQueue.begin()); 
		}
		queueMutex.unlock();
		return newMessage;
	}

	bool Messanger::HasMessages(void)
	{
		int MessageCount = 0;

		queueMutex.lock();
		MessageCount = MessageQueue.size();
		queueMutex.unlock();

		return (MessageCount > 0);
	}

	Messanger::Messanger(unsigned int aKey, MessangerServer *aParent)
	{
		securityKey = aKey;
		parentServer = aParent;
	}
	Messanger::~Messanger()
	{

	}

	/*
			-= Messanger Server =-
	*/

	void MessangerServer::DistributeMessage(MessagePacket aMessage)
	{
		MessageQueueMutex.lock();
		MessageQueue.push(aMessage);
		MessageQueueMutex.unlock();
		queueCondition.notify_one();
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
			//MessageQueueMutex.lock();
			while (MessageQueue.size() > 0)
			{
				MessagePacket newMessage = MessageQueue.front();
				MessageQueue.pop();
				//MessageQueueMutex.unlock();

				if (newMessage.channel == MSG_TARGET_ALL)
				{ // Send to all clients
					// TODO:
				}
				else
				{ // Send to specific channel
					for (auto& messanger : Messangers)
					{
						if (&messanger == newMessage.Origin)
						{
							//MessageQueueMutex.lock();
							continue;
						}
						if (messanger.isRegisteredOnChannel(newMessage.channel))
						{
							messanger.RecieveMessage(newMessage);
						}
					}
				}
				//MessageQueueMutex.lock();
			}
			//MessageQueueMutex.unlock();
			
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