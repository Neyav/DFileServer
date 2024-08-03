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

	Messenger* Message::identifyOrigin(void)
	{
		return Origin;
	}

	Message::Message(bool aisPointer, bool adeleteOnRecieve, Messenger* aOrigin, unsigned int asecurityKey)
	{
		isPointer = aisPointer;
		deleteOnReceive = adeleteOnRecieve;
		Origin = aOrigin;
		OriginName = aOrigin->Name;
		securityKey = asecurityKey;
		Outgoing = 0;
	}

	Message::Message()
	{
		isPointer = false;
		deleteOnReceive = false;
		Origin = nullptr;
		securityKey = 0;		
	}

	Message::~Message()
	{

	}

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

	void Messenger::AlertMessageID(unsigned int aMessageID)
	{
		MessangerQueueMutex.lock();
		waitingMessageIDs.push_back(aMessageID);
		MessangerQueueMutex.unlock();
	}

    void Messenger::SendMessage(unsigned int aChannel, std::string aMessage)
	{
		Message newMessage(false, false, this, securityKey);

		newMessage.message = aMessage;
		newMessage.channel = aChannel;

		parentServer->DistributeMessage(newMessage);
	}
	
	Message Messenger::AcceptMessage(void)
	{
		Message newMessage;
		MessangerQueueMutex.lock();
		if (waitingMessageIDs.size() > 0)
		{
			newMessage = parentServer->GetMessage(waitingMessageIDs[0]);
			waitingMessageIDs.erase(waitingMessageIDs.begin());
		}
		MessangerQueueMutex.unlock();
		return newMessage;
	}

	bool Messenger::HasMessages(void)
	{
		int MessageCount = 0;

		MessangerQueueMutex.lock();
		MessageCount = this->waitingMessageIDs.size();
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

	void MessengerServer::DistributeMessage(Message aMessage)
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

	Message MessengerServer::GetMessage(unsigned int aMessageID)
	{
		Message newMessage;
		MessageServerQueueMutex.lock();
		if (sentMessages.find(aMessageID) != sentMessages.end())
		{
			newMessage = sentMessages[aMessageID];
			if (sentMessages[aMessageID].Outgoing-- == 0)
			{	// The last reciepient has recieved the message.
				sentMessages.erase(aMessageID);
			} 
			
		}
		MessageServerQueueMutex.unlock();
		return newMessage;
	}

	// Any messages that have been around for longer than the timeout will be removed.
	// WARNING: IT IS ASSUMED THE SERVER MUTEX IS LOCKED HERE.
	void MessengerServer::PruneOldMessages(unsigned int atimeout)
	{
		auto now = std::time(nullptr);

		for (auto it = sentMessages.begin(); it != sentMessages.end(); )
		{
			if (now - it->second.sendTime > atimeout)
			{
				it = sentMessages.erase(it);
			}
			else
			{
				++it;
			}
		}
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
				Message newMessage = MessageQueue.front();
				MessageQueue.pop();

				if (newMessage.channel == MSG_TARGET_ALL)
				{ // Send to all clients
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.identifyOrigin())
						{
							continue;
						}

						messenger->AlertMessageID(nextMessageID);
						newMessage.Outgoing++;
					}				
				}
				else
				{ // Send to specific channel
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.identifyOrigin())
						{
							continue;
						}
						if (messenger->isRegisteredOnChannel(newMessage.channel))
						{
							messenger->AlertMessageID(nextMessageID);
							newMessage.Outgoing++;
						}
					}
				}

				// Add the message to the map.
				newMessage.sendTime = std::time(nullptr);
				sentMessages[nextMessageID] = newMessage;
				nextMessageID++;

				// Dispose of any messages that have been around for longer than 60 seconds.
				this->PruneOldMessages(60);

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

		nextMessageID = 0;

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
