#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"


namespace DFSMessaging
{
	std::mutex MessengerwaitingMessageIDsMutex;
	std::mutex MessengerRegisteredChannelsMutex;
	std::mutex MessengerServerMessengersMutex;
	std::mutex MessengerServerMessageQueueMutex;
	std::mutex MessengerServerSentMessagesMutex;

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

		MessengerServerSentMessagesMutex.lock();
		// Check if the message is in the map, if so, delete it and return true.
		if (parentServer->sentMessages.find(messageID) != parentServer->sentMessages.end())
		{
			parentServer->sentMessages.erase(messageID);
			MessengerServerSentMessagesMutex.unlock();
			return true;
		}
		MessengerServerSentMessagesMutex.unlock();
				

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

		MessengerServerSentMessagesMutex.lock();
		// Check if the message is in the map, if so, decrease the pending count.
		if (parentServer->sentMessages.find(messageID) != parentServer->sentMessages.end())
		{
			parentServer->sentMessages[messageID].Pending--;
			if (parentServer->sentMessages[messageID].Pending == 0)
			{	// This message has been rejected by everyone that looked at it. Erase it from the map.
				parentServer->sentMessages.erase(messageID);
			}
		}
		MessengerServerSentMessagesMutex.unlock();
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
		Pending = 0;
		OriginName = "Invalid Message";
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
		MessengerRegisteredChannelsMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessengerRegisteredChannelsMutex.unlock();
				return;
			}
		}

		// If we are not, register on it.
		RegisteredChannels.push_back(aChannel);
		MessengerRegisteredChannelsMutex.unlock();
	}

	bool Messenger::isRegisteredOnChannel(unsigned int aChannel)
	{
		// Check if we are registered on this channel.
		MessengerRegisteredChannelsMutex.lock();
		for (auto& channel : RegisteredChannels)
		{
			if (channel == aChannel)
			{
				MessengerRegisteredChannelsMutex.unlock();
				return true;
			}
		}
		MessengerRegisteredChannelsMutex.unlock();

		return false;
	}

	void Messenger::unRegisterAllChannels(void)
	{
		MessengerRegisteredChannelsMutex.lock();
		RegisteredChannels.clear();
		MessengerRegisteredChannelsMutex.unlock();
	}

	// AlertMessageID is used to notify the messenger that a message is waiting for it.
	// PRESUME THAT THE CALLER HAS LOCKED THE MessengerwaitingMessageIDsMutex
	void Messenger::AlertMessageID(unsigned int aMessageID)
	{
		//MessengerwaitingMessageIDsMutex.lock();
		waitingMessageIDs.push_back(aMessageID);
		//MessengerwaitingMessageIDsMutex.unlock();
		// If we're waiting for a message, notify us.
		messengerCondition.notify_all();
	}

	void Messenger::pauseForMessage(unsigned int aTimeout)
	{
		// If we have no messages, wait for one on our condition variable for aTimeout milliseconds, or indefinitely if aTimeout is 0.
		int waitingMessages = 0;

		MessengerwaitingMessageIDsMutex.lock();
		waitingMessages = (int)waitingMessageIDs.size();
		MessengerwaitingMessageIDsMutex.unlock();

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

    void Messenger::sendMessage(unsigned int aChannel, std::string aMessage)
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
		MessengerwaitingMessageIDsMutex.lock();
		if (waitingMessageIDs.size() > 0)
		{
			newMessage = parentServer->GetMessage(waitingMessageIDs[0]);
			waitingMessageIDs.erase(waitingMessageIDs.begin());
		}
		else
			std::cout << " -=ERROR: AcceptMessages called while no messages to accept." << std::endl;
		MessengerwaitingMessageIDsMutex.unlock();
		return newMessage;
	}

	bool Messenger::HasMessages(void)
	{
		int MessageCount = 0;

		MessengerwaitingMessageIDsMutex.lock();
		MessageCount = (int)this->waitingMessageIDs.size();
		MessengerwaitingMessageIDsMutex.unlock();

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
		MessengerServerMessageQueueMutex.lock();
		MessageQueue.push(aMessage);
		MessengerServerMessageQueueMutex.unlock();
		queueCondition.notify_all();
	}

	bool MessengerServer::ValidateMessenger(Messenger *aMessenger)
	{
		MessengerServerMessengersMutex.lock();
		for (auto& messenger : Messengers)
		{
			if (messenger == aMessenger)
			{
				MessengerServerMessengersMutex.unlock();
				return true;
			}
		}
		MessengerServerMessengersMutex.unlock();
		return false;
	}

	Message MessengerServer::GetMessage(unsigned int aMessageID)
	{
		Message newMessage;
		MessengerServerSentMessagesMutex.lock();
		if (sentMessages.find(aMessageID) != sentMessages.end())
		{
			newMessage = sentMessages[aMessageID];
			if (!sentMessages[aMessageID].isTask)
			{  // If it's not a task messages are accepted as received.
				sentMessages[aMessageID].Pending--;
				if (sentMessages[aMessageID].Pending == 0)
				{
					sentMessages.erase(aMessageID);
				}
			}	
		}
		else
		{
			//std::cout << " -=ERROR: GetMessage called with invalid message ID." << std::endl;
		}

		MessengerServerSentMessagesMutex.unlock();
		return newMessage;
	}

	// Any messages that have been around for longer than the timeout will be removed.
	// PRESUME the sent messages mutex has been locked.
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
			MessengerServerMessageQueueMutex.lock();
			MessengerServerSentMessagesMutex.lock();
			while (MessageQueue.size() > 0)
			{
				Message newMessage = MessageQueue.front();
				MessageQueue.pop();
				MessengerServerMessageQueueMutex.unlock();

				// Add the message to the map.
				newMessage.sendTime = (unsigned int)std::time(nullptr);
				newMessage.messageID = nextMessageID;
				sentMessages[nextMessageID] = newMessage;

				if (newMessage.channel == MSG_TARGET_ALL)
				{ // Send to all clients
					MessengerServerMessengersMutex.lock();
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.identifyOrigin())
						{
							continue;
						}

						messenger->AlertMessageID(nextMessageID);
						sentMessages[nextMessageID].Pending++;
					}
					MessengerServerMessengersMutex.unlock();
				}
				else
				{ // Send to specific channel
					MessengerServerMessengersMutex.lock();
					for (auto& messenger : Messengers)
					{
						if (messenger == newMessage.identifyOrigin())
						{
							continue;
						}
						if (messenger->isRegisteredOnChannel(newMessage.channel))
						{
							messenger->AlertMessageID(nextMessageID);
							sentMessages[nextMessageID].Pending++;
						}
					}
					MessengerServerMessengersMutex.unlock();
				}

				nextMessageID++;

				// Dispose of any messages that have been around for longer than 60 seconds.
				this->PruneOldMessages(60);
				MessengerServerMessageQueueMutex.lock();
			}
			MessengerServerSentMessagesMutex.unlock();
			MessengerServerMessageQueueMutex.unlock();
			
		}
	}

	void MessengerServer::ShutdownServer(void)
	{
		Message ShutdownMessage;

		serverShutdown = true;

		ShutdownMessage.channel = MSG_TARGET_ALL;
		ShutdownMessage.message = "SHUTDOWN";
		ShutdownMessage.OriginName = "Messaging Service";

		this->DistributeMessage(ShutdownMessage);
	}

	Messenger* MessengerServer::ReceiveActiveMessenger(void)
	{
		Messenger* newMessenger;
		
		newMessenger = new Messenger(securityKey, this);

		MessengerServerMessengersMutex.lock();
		Messengers.push_back(newMessenger);
		MessengerServerMessengersMutex.unlock();

		return Messengers.back();
	}

	void MessengerServer::DeactivateActiveMessenger(Messenger* aMessenger)
	{
		MessengerServerMessengersMutex.lock();
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
		MessengerServerMessengersMutex.unlock();
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
		serverShutdown = false;

		// Start Messenger Server Runtime
		std::thread MessengerServerThread(&MessengerServer::MessengerServerRuntime, this);
		MessengerServerThread.detach();
	}
	MessengerServer::~MessengerServer()
	{
		// Destroy Messenger Server
		// Start by destroying all Messengers
		std::cout << " -=Messaging Service shutting down..." << std::endl;

		MessengerServerMessengersMutex.lock();
		while (Messengers.size() > 0)
		{
			delete Messengers.back();
			Messengers.pop_back();
		}
		MessengerServerMessengersMutex.unlock();

		std::cout << " -=Messaging Service shutdown complete." << std::endl;
	}
}
