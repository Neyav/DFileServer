#include <iostream>
#include <ctime>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "InterProcessMessaging.hxx"

namespace DFSMessaging
{
	void Messanger::PokeServer(void)
	{
		parentServer->queueCondition.notify_one();
	}

	Messanger::Messanger(unsigned int aKey, MessangerServer *aParent)
	{
		securityKey = aKey;
		parentServer = aParent;
	}
	Messanger::~Messanger()
	{

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