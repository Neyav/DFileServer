#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"


namespace DFSMessaging
{
	std::mutex MessangerQueueMutex;
	std::mutex MessangerchannelMutex;
	std::mutex MessageServerQueueMutex;
	std::mutex MessageServerMutex;

	void Messenger::RegisterOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		MessangerchannelMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessangerchannelMutex.unlock();
				return;
			}
		}

		// If we are not, register on it.
		RegisteredChannels.push_back(aChannel);
		MessangerchannelMutex.unlock();
	}

	bool Messenger::isRegisteredOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		MessangerchannelMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessangerchannelMutex.unlock();
				return true;
			}
		}
		MessangerchannelMutex.unlock();

		return false;
	}

	void Messenger::unRegisterAllChannels(void)
	{
		MessangerchannelMutex.lock();
		RegisteredChannels.clear();
		MessangerchannelMutex.unlock();
	}

    void Messenger::SendMessage(unsigned int aChannel, std::string aMessage)
	{
		MessagePacket newMessage;

		newMessage.Pointer = nullptr;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.OriginName = this->Name;
		newMessage.channel = aChannel;

		parentServer->DistributeMessage(newMessage);
	}

	void Messenger::SendMessage(Messenger* aMessenger, std::string aMessage)
	{
		MessagePacket newMessage;
		
		newMessage.Pointer = nullptr;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.OriginName = this->Name;
		newMessage.channel = 0;

		// TODO: Might need a mutex here. Probably need a mutex here. 
		if (parentServer->ValidateMessenger(aMessenger) == false)
		{
			return; // The target messenger is not valid.
		}
		aMessenger->ReceiveMessage(newMessage);
	}

	void Messenger::SendPointer(Messenger* aMessenger, void* aTransferPointer)
	{
		MessagePacket newMessage;

		newMessage.Pointer = aTransferPointer;
		newMessage.securityKey = securityKey;
		newMessage.message = "";
		newMessage.Origin = this;
		newMessage.channel = 0;

		aMessenger->ReceiveMessage(newMessage);
	}
	
	void Messenger::ReceiveMessage(MessagePacket aMessage)
	{
		if (aMessage.securityKey != securityKey)
		{
			return;
		}
		// Add it to our message queue.
		MessangerQueueMutex.lock();
		MessageQueue.push_back(aMessage);
		MessangerQueueMutex.unlock();
	}

	MessagePacket Messenger::AcceptMessage(void)
	{
		MessagePacket newMessage;
		MessangerQueueMutex.lock();
		if (MessageQueue.size() > 0)
		{
			newMessage = MessageQueue.front();
			MessageQueue.erase(MessageQueue.begin()); 
		}
		MessangerQueueMutex.unlock();
		return newMessage;
	}

	bool Messenger::HasMessages(void)
	{
		int MessageCount = 0;

		MessangerQueueMutex.lock();
		MessageCount = MessageQueue.size();
		MessangerQueueMutex.unlock();

		return (MessageCount > 0);
	}

	Messenger::Messenger(unsigned int aKey, MessengerServer *aParent)
	{
		securityKey = aKey;
		parentServer = aParent;
	}
	Messenger::~Messenger()
	{
		if (parentServer != nullptr)
		{
			parentServer->DeactivateActiveMessenger(this);
		}
	}

	/*
			-= Messenger Server =-
	*/

	void MessengerServer::DistributeMessage(MessagePacket aMessage)
	{
		MessageServerQueueMutex.lock();
		MessageQueue.push(aMessage);
		MessageServerQueueMutex.unlock();
		queueCondition.notify_all();
	}

	bool MessengerServer::ValidateMessenger(Messenger *aMessenger)
	{
		MessageServerQueueMutex.lock();
		for (auto& messenger : Messengers)
		{
			if (messenger == aMessenger)
			{
				MessageServerQueueMutex.unlock();
				return true;
			}
		}
		MessageServerQueueMutex.unlock();
		return false;
	}

	void MessengerServer::MessengerServerRuntime(void)
	{
		std::mutex queueMutex;
		std::unique_lock<std::mutex> queueLock(queueMutex);

		std::cout << " -=Messenger Server Runtime started." << std::endl;

		while (true)
		{
			// Do stuff here.
			queueCondition.wait(queueLock);

			// Distribute any messages in the queue.
			MessageServerQueueMutex.lock();
			while (MessageQueue.size() > 0)
			{
				MessagePacket newMessage = MessageQueue.front();
				MessageQueue.pop();
				MessageServerQueueMutex.unlock();

				if (newMessage.channel == MSG_TARGET_ALL)
				{ // Send to all clients
					MessageServerQueueMutex.lock();
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.Origin)
						{
							continue;
						}
						messenger->ReceiveMessage(newMessage);
					}
					MessageServerQueueMutex.unlock();					
				}
				else
				{ // Send to specific channel
					MessageServerQueueMutex.lock();
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.Origin)
						{
							continue;
						}
						if (messenger->isRegisteredOnChannel(newMessage.channel))
						{
							messenger->ReceiveMessage(newMessage);
						}
					}
					MessageServerQueueMutex.unlock();
				}
				MessageServerQueueMutex.lock();
			}
			MessageServerQueueMutex.unlock();
			
		}
	}

	Messenger* MessengerServer::ReceiveActiveMessenger(void)
	{
		Messenger* newMessenger;
		
		newMessenger = new Messenger(securityKey, this);

		MessageServerMutex.lock();
		Messengers.push_back(newMessenger);
		MessageServerMutex.unlock();

		return Messengers.back();
	}

	void MessengerServer::DeactivateActiveMessenger(Messenger* aMessenger)
	{
		MessageServerMutex.lock();
		for (int i = 0; i < Messengers.size(); i++)
		{
			if (Messengers[i] == aMessenger)
			{
				// The destructor of a Messenger will call this to ensure it is removed from the server.
				// Thus it is unwise to delete it here. This should therefore NEVER be called elsewhere.				
				Messengers.erase(Messengers.begin() + i);
				break;
			}
		}
		MessageServerMutex.unlock();
	}

	MessengerServer::MessengerServer()
	{
		// Initialize Messenger Server
		std::cout << " -=Messaging Service Initializing..." << std::endl;

		std::random_device rd;
		std::mt19937 gen(rd());
		
		std::uniform_int_distribution<> dis(0, 1073741824);

		securityKey = dis(gen);
	
		std::cout << " -=Messenger Service Security Key generated. [" << securityKey << "]" << std::endl;

		// Start Messenger Server Runtime
		std::thread MessengerServerThread(&MessengerServer::MessengerServerRuntime, this);
		MessengerServerThread.detach();
	}
	MessengerServer::~MessengerServer()
	{
		// Destroy Messenger Server
		// Start by destroying all Messengers
		std::cout << " -=Messaging Service shutting down..." << std::endl;

		MessageServerMutex.lock();
		while (Messengers.size() > 0)
		{
			delete Messengers.back();
			Messengers.pop_back();
		}
		MessageServerMutex.unlock();

		std::cout << " -=Messaging Service shutdown complete." << std::endl;
	}
}
