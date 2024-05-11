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

	void Messanger::RegisterOnChannel(unsigned int aChannel)
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

	bool Messanger::isRegisteredOnChannel(unsigned int aChannel)
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
	}

	void Messanger::unRegisterAllChannels(void)
	{
		MessangerchannelMutex.lock();
		RegisteredChannels.clear();
		MessangerchannelMutex.unlock();
	}

    void Messanger::SendMessage(unsigned int aChannel, std::string aMessage)
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

	void Messanger::SendMessage(Messanger* aMessanger, std::string aMessage)
	{
		MessagePacket newMessage;
		
		newMessage.Pointer = nullptr;
		newMessage.securityKey = securityKey;
		newMessage.message = aMessage;
		newMessage.Origin = this;
		newMessage.OriginName = this->Name;
		newMessage.channel = 0;

		// TODO: Might need a mutex here. Probably need a mutex here. 
		if (parentServer->ValidateMessanger(aMessanger) == false)
		{
			return; // The target messanger is not valid.
		}
		aMessanger->RecieveMessage(newMessage);
	}

	void Messanger::SendPointer(Messanger* aMessanger, void* aTransferPointer)
	{
		MessagePacket newMessage;

		newMessage.Pointer = aTransferPointer;
		newMessage.securityKey = securityKey;
		newMessage.message = "";
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
		MessangerQueueMutex.lock();
		MessageQueue.push_back(aMessage);
		MessangerQueueMutex.unlock();
	}

	MessagePacket Messanger::AcceptMessage(void)
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

	bool Messanger::HasMessages(void)
	{
		int MessageCount = 0;

		MessangerQueueMutex.lock();
		MessageCount = MessageQueue.size();
		MessangerQueueMutex.unlock();

		return (MessageCount > 0);
	}

	Messanger::Messanger(unsigned int aKey, MessangerServer *aParent)
	{
		securityKey = aKey;
		parentServer = aParent;
	}
	Messanger::~Messanger()
	{
		if (parentServer != nullptr)
		{
			parentServer->DeactivateActiveMessanger(this);
		}
	}

	/*
			-= Messanger Server =-
	*/

	void MessangerServer::DistributeMessage(MessagePacket aMessage)
	{
		MessageServerQueueMutex.lock();
		MessageQueue.push(aMessage);
		MessageServerQueueMutex.unlock();
		queueCondition.notify_all();
	}

	bool MessangerServer::ValidateMessanger(Messanger *aMessanger)
	{
		MessageServerQueueMutex.lock();
		for (auto& messanger : Messangers)
		{
			if (messanger == aMessanger)
			{
				MessageServerQueueMutex.unlock();
				return true;
			}
		}
		MessageServerQueueMutex.unlock();
		return false;
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

			// Distribute any messages in the queue.
			MessageServerQueueMutex.lock();
			while (MessageQueue.size() > 0)
			{
				MessagePacket newMessage = MessageQueue.front();
				MessageQueue.pop();
				MessageServerQueueMutex.unlock();

				if (newMessage.channel == MSG_TARGET_ALL)
				{ // Send to all clients
					// TODO:
				}
				else
				{ // Send to specific channel
					MessageServerQueueMutex.lock();
					for (auto& messanger : Messangers)
					{
						if (messanger == newMessage.Origin)
						{
							continue;
						}
						if (messanger->isRegisteredOnChannel(newMessage.channel))
						{
							messanger->RecieveMessage(newMessage);
						}
					}
					MessageServerQueueMutex.unlock();
				}
				MessageServerQueueMutex.lock();
			}
			MessageServerQueueMutex.unlock();
			
		}
	}

	Messanger* MessangerServer::ReceiveActiveMessanger(void)
	{
		Messanger* newMessanger;
		
		newMessanger = new Messanger(securityKey, this);

		MessageServerMutex.lock();
		Messangers.push_back(newMessanger);
		MessageServerMutex.unlock();

		return Messangers.back();
	}

	void MessangerServer::DeactivateActiveMessanger(Messanger* aMessanger)
	{
		MessageServerMutex.lock();
		for (int i = 0; i < Messangers.size(); i++)
		{
			if (Messangers[i] == aMessanger)
			{
				// The destructor of a Messanger will call this to ensure it is removed from the server.
				// Thus it is unwise to delete it here. This should therefore NEVER be called elsewhere.
				//delete Messangers[i];
				Messangers.erase(Messangers.begin() + i);
				break;
			}
		}
		MessageServerMutex.unlock();
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
		// Start by destroying all Messangers
		std::cout << " -=Messanging Service shutting down..." << std::endl;

		MessageServerMutex.lock();
		while (Messangers.size() > 0)
		{
			delete Messangers.back();
			Messangers.pop_back();
		}
		MessageServerMutex.unlock();

		std::cout << " -=Messanging Service shutdown complete." << std::endl;
	}
}
