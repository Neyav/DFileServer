#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"


namespace DFSMessaging
{
	std::mutex MessengerMutex;
	std::mutex MessageServerMutex;
	std::mutex MessageServerAddRemoveMutex;

	// The Message class contains the message data, and is used to accept tasks and receive messages.
	// ==============================================================================
	// = DFS:Messaging:Message														=
	// ==============================================================================

	Messenger* Message::identifyOrigin(void)
	{
		return Origin;
	}

	bool Message::acceptTask(void)
	{
		// Check if the message is still valid. Delete it and return true, otherwise return false.
		if (Origin == nullptr)
		{
			return false;
		}

		if (Origin->securityKey != securityKey)
		{
			return false;
		}
	
		if (!isTask)
		{
			return false;
		}

		MessengerServer *parentServer = Origin->parentServer;

		MessageServerMutex.lock();
		// Check if the message is in the map, if so, delete it and return true.
		if (parentServer->sentMessages.find(messageID) != parentServer->sentMessages.end())
		{
			parentServer->sentMessages.erase(messageID);
			MessageServerMutex.unlock();
			return true;
		}
		MessageServerMutex.unlock();		

		return false;
	}

	void Message::refuseTask(void)
	{
		// Check if the message is still valid, if it is, decrease the pending count.
		if (Origin == nullptr)
		{
			return;
		}

		if (Origin->securityKey != securityKey)
		{
			return;
		}

		if (!isTask)
		{
			return;
		}

		MessengerServer* parentServer = Origin->parentServer;

		MessageServerMutex.lock();
		// Check if the message is in the map, if so, decrease the pending count.
		if (parentServer->sentMessages.find(messageID) != parentServer->sentMessages.end())
		{
			if (parentServer->sentMessages[messageID].Pending-- == 0)
			{	// This message has been rejected by everyone that looked at it. Erase it from the map.
				parentServer->sentMessages.erase(messageID);
			}
		}
		MessageServerMutex.unlock();
	}

	Message::Message(bool aisPointer, bool aisTask, Messenger* aOrigin, unsigned int asecurityKey)
	{
		isPointer = aisPointer;
		isTask = aisTask;
		Origin = aOrigin;
		OriginName = aOrigin->Name;
		securityKey = asecurityKey;
		Pointer = nullptr;
		Pending = 0;
		messageID = 0;
	}

	Message::Message()
	{
		isPointer = false;
		isTask = false;
		Origin = nullptr;
		Pointer = nullptr;
		securityKey = 0;
		messageID = 0;
	}

	Message::~Message()
	{

	}

	// The Messenger class is used as a method to send and receive messages between other messengers.
	// ==============================================================================
	// = DFS:Messaging:Messenger													=
	// ==============================================================================

	void Messenger::RegisterOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		MessengerMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessengerMutex.unlock();
				return;
			}
		}

		// If we are not, register on it.
		RegisteredChannels.push_back(aChannel);
		MessengerMutex.unlock();
	}

	bool Messenger::isRegisteredOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		MessengerMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessengerMutex.unlock();
				return true;
			}
		}
		MessengerMutex.unlock();

		return false;
	}

	void Messenger::unRegisterAllChannels(void)
	{
		MessengerMutex.lock();
		RegisteredChannels.clear();
		MessengerMutex.unlock();
	}

	void Messenger::AlertMessageID(unsigned int aMessageID)
	{
		MessengerMutex.lock();
		waitingMessageIDs.push_back(aMessageID);
		// If we're waiting for a message, notify us.
		messengerCondition.notify_all();
		MessengerMutex.unlock();
	}

	void Messenger::pauseForMessage(unsigned int aTimeout)
	{
		// If we have no messages, wait for one on our condition variable for aTimeout milliseconds, or indefinitely if aTimeout is 0.
		int waitingMessages = 0;

		MessengerMutex.lock();
		waitingMessages = waitingMessageIDs.size();
		MessengerMutex.unlock();

		if (waitingMessages == 0)
		{
			std::mutex MessengerPauseMutex;
			std::unique_lock<std::mutex> lock(MessengerPauseMutex);
			if (aTimeout == 0)
			{
				messengerCondition.wait(lock);
			}
			else
			{
				messengerCondition.wait_for(lock, std::chrono::milliseconds(aTimeout));
			}
		}
		else
		{
			// If we have messages, we don't need to wait.
			return;
		}
	}

    void Messenger::SendMessage(unsigned int aChannel, std::string aMessage)
	{
		Message newMessage(false, false, this, securityKey);

		newMessage.message = aMessage;
		newMessage.channel = aChannel;

		parentServer->DistributeMessage(newMessage);
	}

	void Messenger::SendPointer(unsigned int aChannel, void* aPointer)
	{
		Message newMessage(true, true, this, securityKey);

		newMessage.Pointer = aPointer;
		newMessage.channel = aChannel;

		parentServer->DistributeMessage(newMessage);
	}
	
	Message Messenger::AcceptMessage(void)
	{
		Message newMessage;
		MessengerMutex.lock();
		if (waitingMessageIDs.size() > 0)
		{
			newMessage = parentServer->GetMessage(waitingMessageIDs[0]);
			waitingMessageIDs.erase(waitingMessageIDs.begin());
		}
		MessengerMutex.unlock();
		return newMessage;
	}

	bool Messenger::HasMessages(void)
	{
		int MessageCount = 0;

		MessengerMutex.lock();
		MessageCount = this->waitingMessageIDs.size();
		MessengerMutex.unlock();

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
			std::cout << " -=Messenger [" << Name << "] shutting down." << std::endl;
			parentServer->DeactivateActiveMessenger(this);
		}
	}

	/*
			-= Messenger Server =-
	*/

	void MessengerServer::DistributeMessage(Message aMessage)
	{
		MessageServerMutex.lock();
		MessageQueue.push(aMessage);
		MessageServerMutex.unlock();
		queueCondition.notify_all();
	}

	bool MessengerServer::ValidateMessenger(Messenger *aMessenger)
	{
		MessageServerMutex.lock();
		for (auto& messenger : Messengers)
		{
			if (messenger == aMessenger)
			{
				MessageServerMutex.unlock();
				return true;
			}
		}
		MessageServerMutex.unlock();
		return false;
	}

	Message MessengerServer::GetMessage(unsigned int aMessageID)
	{
		Message newMessage;
		MessageServerMutex.lock();
		if (sentMessages.find(aMessageID) != sentMessages.end())
		{
			newMessage = sentMessages[aMessageID];
			if (!sentMessages[aMessageID].isTask)
			{  // If it's not a task messages are accepted as received.
				if (sentMessages[aMessageID].Pending-- == 0)
				{
					sentMessages.erase(aMessageID);
				}
			}
			
		}
		MessageServerMutex.unlock();
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
			MessageServerAddRemoveMutex.lock();
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
						newMessage.Pending++;
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
							newMessage.Pending++;
						}
					}
				}

				// Add the message to the map.
				newMessage.sendTime = std::time(nullptr);
				newMessage.messageID = nextMessageID;
				sentMessages[nextMessageID] = newMessage;
				nextMessageID++;

				// Dispose of any messages that have been around for longer than 60 seconds.
				this->PruneOldMessages(60);

			}
			MessageServerAddRemoveMutex.unlock();			
		}
	}

	Messenger* MessengerServer::ReceiveActiveMessenger(void)
	{
		Messenger* newMessenger;
		
		newMessenger = new Messenger(securityKey, this);

		MessageServerAddRemoveMutex.lock();
		Messengers.push_back(newMessenger);
		MessageServerAddRemoveMutex.unlock();

		return Messengers.back();
	}

	void MessengerServer::DeactivateActiveMessenger(Messenger* aMessenger)
	{
		MessageServerAddRemoveMutex.lock();
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
		MessageServerAddRemoveMutex.unlock();
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
