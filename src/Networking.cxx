#include <iostream>

#include "Networking.hxx"
#include "Version.hxx"
#include "DirectoryIndexing.hxx"
#include "CPathResolver.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"

extern int ActiveConnections;
extern bool ServerShutdown;
extern bool ServerLockdown;

namespace DFSNetworking
{
	void NetworkDaemon::IncomingConnection(void)
	{
		ClientConnection IncomingClient;

		// Accept the connection and break if there was an error.
		if (IncomingClient.AcceptConnection(NetworkSocket) == -1)
			return;

		// If we're in serverlockdown, close the connection right away.
		if (ServerLockdown)
		{
			IncomingClient.DisconnectClient();

			return;
		}

		ActiveConnections++;

		// Find the next free slot.
		for (int x = 0; x < 2048; x++)
			if (PollStruct[x].fd <= 0)
			{
				IncomingClient.PollIterator = x;
				break;
			}

		if (IncomingClient.PollIterator > HighestPollIterator)
			HighestPollIterator = IncomingClient.PollIterator;

		// Add it to the Poll Structure so it gets polled.
		PollStruct[IncomingClient.PollIterator].fd = IncomingClient.GetSocket();
		PollStruct[IncomingClient.PollIterator].events = (POLLIN | POLLOUT);

		// Add it to the linked list.
		ConnectionList.push_back(IncomingClient);

	}

	void NetworkDaemon::TerminateConnection(int aConnectionIndex)
	{
		int NewHighestPollIterator = 0;
		int OldPollIterator = 0;

		ActiveConnections--;

		// If serverlockdown was established and this is the last connection, do a complete shutdown.
		if (ServerLockdown)
		{
			if (ActiveConnections == 0)
				ServerShutdown = true;
			else
				printf("Client Disconnected; %i remaining...\n", ActiveConnections);
		}

		// Keep track of the old iterator. Previously I just used ArgClient->PollIterator in this function, but that doesn't
		// appear safe to do after the ArgList->erase command. It's causing crashes on some platforms (i.e. FreeBSD 6.x )
		OldPollIterator = ConnectionList[aConnectionIndex].PollIterator;

		// Remove this client from the Poll structure so it doesn't get polled.
		PollStruct[OldPollIterator].fd = 0;
		PollStruct[OldPollIterator].events = 0;

		// Close the socket and possibly do some other Misc shutdown stuff.
		ConnectionList[aConnectionIndex].DisconnectClient();

		// Remove the client from the linked list.
		ConnectionList.erase(ConnectionList.begin() + aConnectionIndex);

		// This Client had the highest descriptor..
		if (OldPollIterator == HighestPollIterator)
		{
			// Traverse through the active connections to find the new highest file descriptor
			for (int ConnectionListIterator = 0;
				ConnectionListIterator < ConnectionList.size();
				ConnectionListIterator++)
			{
				if (ConnectionList[ConnectionListIterator].PollIterator > NewHighestPollIterator)
					NewHighestPollIterator = ConnectionList[ConnectionListIterator].PollIterator;
			}

			HighestPollIterator = NewHighestPollIterator;
		}
		return;
	}

	void NetworkDaemon::Buffer404(ClientConnection* ArgClient)
	{
		// This fills the buffer with the 404 error.

		ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Resource not found</TITLE></HEAD><BODY><H1>Resource not found</H1>";
		ArgClient->SendBuffer += "The resource you were trying to locate doesn't exist on this server.<br><br><HR>";
		ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "]</I>";
		ArgClient->SendBuffer += "</BODY></HTML>";
	}

	void NetworkDaemon::Buffer401(ClientConnection* ArgClient)
	{
		// This fills the buffer with the 401 error.

		ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Authorization Required</TITLE></HEAD><BODY><H1>Authorization Required</H1>";
		ArgClient->SendBuffer += "The resource you were trying to locate requires authorization on this server.<br><br><HR>";
		ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "]</I>";
		ArgClient->SendBuffer += "</BODY></HTML>";
	}

	char NetworkDaemon::LocateResource(std::string Resource, ClientConnection* ArgClient, char* DstResource, char* DstResourceType)
	{
		FILE* FileStream;
		char FieldName[31];
		char ResolveIdentifier[MAXIDENTIFIERLEN];
		char RealIdentifier[MAXIDENTIFIERLEN];
		char IPRange[17];
		char ResourceType[MAXIDENTIFIERLEN];

		// No resource defined.
		if (Resource.empty())
		{
			return -1;
		}

		if ((FileStream = fopen("resource.cfg", "r")) == NULL)
		{
			// Resource.cfg file couldn't be opened, so just make a dummy index for where we are.
			try
			{
				CPathResolver PathResolver;

				PathResolver.Add("/", ".");  // Make the root of the tree our current location.

				strcpy(DstResource, PathResolver(Resource).c_str());
				strcpy(DstResourceType, "redirect");

				return 1;
			}
			catch (CPathResolver::exn_t exn)
			{ // Invalid path.
				return -1;
			}
		}

		// Find the resource identifier.
		while (!feof(FileStream))
		{
			fscanf(FileStream, "%30s %150s %150s", FieldName, ResolveIdentifier, ResourceType);

			if (strcmp(FieldName, "RESOURCE") == 0) // A resource.
			{
				if (strcmp(ResourceType, "redirect") == 0) // redirect
				{
					if (strncmp(ResolveIdentifier, Resource.c_str(), strlen(ResolveIdentifier)) == 0)
					{
						strcpy(DstResourceType, ResourceType);
						break;
					}
				}

				if (strcmp(ResolveIdentifier, Resource.c_str()) == 0) // A match.
				{
					strcpy(DstResourceType, ResourceType);
					break;
				}
			}
		}

		// Find the first IP match.
		while (!feof(FileStream))
		{
			fscanf(FileStream, "%30s %16s %150s", FieldName, IPRange, RealIdentifier);

			// Slipped past into another resource without a match.
			if (strcmp(FieldName, "RESOURCE") == 0)
			{
				fclose(FileStream);
				return -1;
			}

			// We've got a match.
			if ((strncmp(ArgClient->GetIP(), IPRange, strlen(IPRange)) == 0) || // Direct match.
				(strcmp("x.x.x.x", IPRange) == 0)) // Wildcarded.
			{
				fclose(FileStream);

				if (strcmp(DstResourceType, "redirect") == 0) // Resolve actual path.
				{
					try
					{

						CPathResolver PathResolver;

						PathResolver.Add(ResolveIdentifier, RealIdentifier);

						strcpy(DstResource, PathResolver(Resource).c_str());

					}
					catch (CPathResolver::exn_t exn)
					{
						// Illegal path.
						return -1;
					}
				}
				else  // Direct Resource.
					strcpy(DstResource, RealIdentifier);

				return 1;
			}
		}

		// Reached end of file without a match.
		fclose(FileStream);
		return -1;

	}

	void NetworkDaemon::ParseURLEncoding(char* ArgBuffer)
	{
		size_t BufferLength = strlen(ArgBuffer);
		size_t iterator1;
		int iterator2 = 0;

		// Iterator through the entire buffer.
		for (iterator1 = 0; iterator1 < BufferLength; iterator1++)
		{
			ArgBuffer[iterator2] = ArgBuffer[iterator1]; // Copy it, character by character.

			// Check for a % sign. They indicates URL encoding.
			if (ArgBuffer[iterator1] == '%')
			{
				if ((iterator1 + 2) < BufferLength) // Make sure we don't go past the end.
				{
					char HexCode[2];
					char* CharPointer;
					long Value;

					CharPointer = &ArgBuffer[iterator1 + 1];

					strncpy(HexCode, CharPointer, 2);

					if ((Value = strtol(HexCode, NULL, 16)) != 0 && Value < 256) // It's a valid hex code.
					{
						// Replace the value at iterator2.
						ArgBuffer[iterator2] = (char)Value;

						// Increment iterator1 by 2
						iterator1 += 2;
					}

				}

			} // [END] -- if ( ArgBuffer... )

			iterator2++;
		} // [END] -- for ( ...

		// Null terminate it.
		ArgBuffer[iterator2] = '\0';

	}

	bool NetworkDaemon::initalizeNetwork(unsigned int aPort, unsigned int aBackLog)
	{
		struct sockaddr_in ListenAddr;
		const char yes = 1;
#ifdef _WINDOWS
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
		{
			perror("InitalizeNetwork -- WSAStartup()");
			return false;
		}
#endif
		std::cout << " -=Initalizing Network Daemon..." << std::endl;

		memset(PollStruct, '\0', sizeof(PollStruct));

		// Grab the master socket.
		if ((NetworkSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			perror("InitalizeNetwork -- socket()");
			return false;
		}

		// Clear the socket incase it hasn't been properly closed so that we may use it.
		if (setsockopt(NetworkSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("InitalizeNetwork -- setsockopt()");
			return false;
		}

		// Set the listening network struct up.
		ListenAddr.sin_family = AF_INET;
		ListenAddr.sin_port = htons(aPort);
		ListenAddr.sin_addr.s_addr = INADDR_ANY;
		memset(&(ListenAddr.sin_zero), '\0', 8);

		// Bind the socket to the listening network struct.
		if (bind(NetworkSocket, (struct sockaddr*)&ListenAddr,
			sizeof(struct sockaddr)) == -1)
		{
			perror("InitalizeNetwork -- bind()");
			return false;
		}

		// Start listening
		if (listen(NetworkSocket, aBackLog) == -1)
		{
			perror("InitalizeNetwork -- listen()");
			return false;
		}

		PollStruct[0].fd = NetworkSocket;
		PollStruct[0].events = POLLIN;
		HighestPollIterator = 0;

		// We were successful, return the socket.
		return true;
	}

	void NetworkDaemon::AddMessenger(DFSMessaging::Messanger* aMessanger)
	{
		NetworkMessanger = aMessanger;
		NetworkMessanger->RegisterOnChannel("Network");
	}

	void NetworkDaemon::NetworkLoop(void)
	{
		while (1)
		{
			poll(PollStruct, HighestPollIterator + 1, INFTIM);

			// Do we have an incoming connection?
			if ((PollStruct[0].revents & POLLIN) && (ActiveConnections != Configuration.MaxConnections || !Configuration.MaxConnections))
			{
				// Handle the incoming connection.
				IncomingConnection();
			}

			// Go through the list looking for connections that have incoming data.
			for (int ConnectionListIterator = 0; ConnectionListIterator < ConnectionList.size(); ConnectionListIterator++)
			{
				// This socket has incoming data.
				if (PollStruct[ConnectionList[ConnectionListIterator].PollIterator].revents & POLLIN)
				{
					char DataBuffer[500]; // 500 should be big enough.
					size_t DataRecved;

					if ((DataRecved = ConnectionList[ConnectionListIterator].RecvData(DataBuffer, sizeof(DataBuffer))) < 1)
					{ // Disconnection or error. Terminate client.

						TerminateConnection(ConnectionListIterator);
						ConnectionListIterator--;
						if (ConnectionListIterator < 0)
							ConnectionListIterator = 0;
						continue;
					}
					else
					{ // Incoming data.
						ConnectionList[ConnectionListIterator].BrowserRequest.ImportHeader(std::string(DataBuffer));

						// Find what resource this person is after.
						ConnectionList[ConnectionListIterator].Resource = ConnectionList[ConnectionListIterator].BrowserRequest.AccessPath;
					}

				} // [/END] Incoming Data on socket.

				if (ConnectionListIterator < ConnectionList.size() && !ConnectionList[ConnectionListIterator].Resource.empty() &&
					PollStruct[ConnectionList[ConnectionListIterator].PollIterator].revents & POLLOUT)
				{

					if (!ConnectionList[ConnectionListIterator].FileStream
						&& ConnectionList[ConnectionListIterator].SendBuffer.empty())
					{
						char Resource[151];
						char ResourceType[151];
						size_t ResourceSize = 0;

						// We need to locate this resource.
						if (LocateResource(ConnectionList[ConnectionListIterator].Resource, &ConnectionList[ConnectionListIterator], Resource, ResourceType) == -1)
						{ // Resource wasn't found.
							Buffer404(&ConnectionList[ConnectionListIterator]);
							ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
							strcpy(Resource, "--404--");
							strcpy(ResourceType, "text/html");

							continue;
						}

						ParseURLEncoding(Resource);

						// Is it a redirect?
						if (strcmp(ResourceType, "redirect") == 0)
						{
							char direrror;

							direrror = GenerateFolderIndex(ConnectionList[ConnectionListIterator].Resource, Resource, ConnectionList[ConnectionListIterator].SendBuffer);

							if (direrror == -1)
							{ // File/Folder doesn't exist
								Buffer404(&ConnectionList[ConnectionListIterator]);
								ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
								strcpy(Resource, "--404--");
								strcpy(ResourceType, "text/html");
							}
							else if (direrror)
							{ // It generated an index for us.

								ConnectionList[ConnectionListIterator].SendBufferIterator = 0;

								ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();

								strcpy(ResourceType, "text/html");
							}
							else
							{ // The location points to a file, not a folder.
								// Determine the mimetype by the filename's extention
								strcpy(ResourceType, ReturnMimeType(Resource));
							}
						}
						else if (strcmp(ResourceType, "-") == 0)
						{ // User didn't specify a mimetype. Determine by extension.
							strcpy(ResourceType, ReturnMimeType(Resource));
						}

						if (Configuration.ShowConnections)
						{
							printf("%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionList[ConnectionListIterator].GetIP(),
								ConnectionList[ConnectionListIterator].Resource.c_str(), Resource);
						}

						if (Configuration.LogFile != nullptr)
						{
							fprintf(Configuration.LogFile, "%s %s - [%s] (%s)\n", TimeAndDate(), ConnectionList[ConnectionListIterator].GetIP(),
								ConnectionList[ConnectionListIterator].Resource.c_str(), Resource);

							// Make sure it gets written to disk.
							fflush(Configuration.LogFile);
						}

						// If there is no sendbuffer, try to open the file, and if you can't, terminate the connection.
						if (ConnectionList[ConnectionListIterator].SendBuffer.empty() &&
							((ResourceSize = ConnectionList[ConnectionListIterator].OpenFile(Resource)) == -1))
						{ // File couldn't be opened.
							TerminateConnection(ConnectionListIterator);
							ConnectionListIterator--;
							if (ConnectionListIterator < 0)
								ConnectionListIterator = 0;
							continue;
						}
						else
						{
							// Send the HTTP header.
							char Buffer[500];

							ConnectionList[ConnectionListIterator].ServerResponse.AccessType = "HTTP/1.1";
							ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "200";
							ConnectionList[ConnectionListIterator].ServerResponse.AccessProtocol = "OK";

							if (!Configuration.BasicCredentials.empty()) // Authentication Required
							{
								if (ConnectionList[ConnectionListIterator].BrowserRequest.GetValue("Authorization").empty())
								{ // Authentication wasn't attempted, send the request.
									ConnectionList[ConnectionListIterator].ServerResponse.SetValue("WWW-Authenticate",
										"Basic realm=\"DFileServer\"");
									ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "401"; // Authorization Required
								}
								else
								{ // Try to verify the authentication.
									Base64 Base64Encoding;
									std::string Base64Authorization;

									Base64Authorization = ConnectionList[ConnectionListIterator].BrowserRequest.GetValue("Authorization");

									Base64Authorization.erase(0, 6); // Remove the beginning "Basic "

									if (Base64Encoding.decode(Base64Authorization) != Configuration.BasicCredentials)
									{
										ConnectionList[ConnectionListIterator].ServerResponse.AccessPath = "401"; // Authorization Required

										ConnectionList[ConnectionListIterator].CloseFile(); // Make sure we close the resource.

										// Prepare the 401...
										Buffer401(&ConnectionList[ConnectionListIterator]);
										ResourceSize = ConnectionList[ConnectionListIterator].BytesRemaining = ConnectionList[ConnectionListIterator].SendBuffer.size();
										strcpy(Resource, "--401--");
										strcpy(ResourceType, "text/html");
									}

								}
							}

							sprintf(Buffer, "DFileServer/%i.%i.%i", Version::MAJORVERSION, Version::MINORVERSION, Version::PATCHVERSION);

							ConnectionList[ConnectionListIterator].ServerResponse.SetValue("Server", std::string(Buffer));
							ConnectionList[ConnectionListIterator].ServerResponse.SetValue("Connection", "close");

							sprintf(Buffer, "%i", (int)ResourceSize);

							ConnectionList[ConnectionListIterator].ServerResponse.SetValue("Content-Length", std::string(Buffer));
							ConnectionList[ConnectionListIterator].ServerResponse.SetValue("Content-Type", std::string(ResourceType));

							ConnectionList[ConnectionListIterator].SendData(
								(char*)ConnectionList[ConnectionListIterator].ServerResponse.ExportHeader().c_str(), 0);
						}
					}
					else
					{ // Send 4096 bytes of the file the way of the client.
						char Buffer[4096];
						int BytesToRead, BytesRead;

						memset(Buffer, '\0', sizeof(Buffer));

						BytesToRead = sizeof(Buffer) - 1;

						if (Configuration.MaxBandwidth)
						{
							// -- Bandwidth Control --
							if (ConnectionList[ConnectionListIterator].LastBandReset != time(NULL))
							{
								ConnectionList[ConnectionListIterator].BandwidthLeft = Configuration.MaxBandwidth / ActiveConnections;

								ConnectionList[ConnectionListIterator].LastBandReset = time(NULL);
							}

							if (BytesToRead > ConnectionList[ConnectionListIterator].BandwidthLeft)
								BytesToRead = ConnectionList[ConnectionListIterator].BandwidthLeft;

							ConnectionList[ConnectionListIterator].BandwidthLeft -= BytesToRead;
							// / -- Bandwidth Control --
						}

						if (BytesToRead > ConnectionList[ConnectionListIterator].BytesRemaining)
							BytesToRead = ConnectionList[ConnectionListIterator].BytesRemaining;

						if (ConnectionList[ConnectionListIterator].FileStream)
						{ // Read from a file
							ConnectionList[ConnectionListIterator].FileStream->read(Buffer, BytesToRead);
						}
						else
						{ // Send from the buffer

							strcpy(Buffer, ConnectionList[ConnectionListIterator].SendBuffer.substr(
								ConnectionList[ConnectionListIterator].SendBufferIterator, BytesToRead).c_str());

							// Increment Iterator
							ConnectionList[ConnectionListIterator].SendBufferIterator += BytesToRead;
						}

						BytesRead = ConnectionList[ConnectionListIterator].SendData(Buffer, BytesToRead);

						ConnectionList[ConnectionListIterator].BytesRemaining -= BytesRead;

						// It hasn't sent all the bytes we've read.
						if (BytesRead != BytesToRead)
						{
							if (ConnectionList[ConnectionListIterator].FileStream)
							{
								// Relocate the file pointer to match where we are.

								int LocationOffset = BytesToRead - BytesRead;
								std::ifstream::pos_type CurrentPosition = ConnectionList[ConnectionListIterator].FileStream->tellg();

								ConnectionList[ConnectionListIterator].FileStream->seekg((CurrentPosition - (std::ifstream::pos_type)LocationOffset));
							}
							else
							{
								// Relocate the iterator to match where we are.

								int LocationOffset = BytesToRead - BytesRead;

								ConnectionList[ConnectionListIterator].SendBufferIterator -= LocationOffset;
							}
						}

						// End of the file?
						if (ConnectionList[ConnectionListIterator].BytesRemaining == 0)
						{
							TerminateConnection(ConnectionListIterator);
							ConnectionListIterator--;
							if (ConnectionListIterator < 0)
								ConnectionListIterator = 0;
							continue;
						}
					}


				} // [/END] Socket can accept data.

				// Connection was idle for too long, remove the jerk.
				if (ConnectionList[ConnectionListIterator].SecondsIdle() > 15)
				{
					TerminateConnection(ConnectionListIterator);
					ConnectionListIterator--;
					if (ConnectionListIterator < 0)
						ConnectionListIterator = 0;
					continue;
				}

			} // The linked list for loop.

			// Initate self destruct sequence.
			if (ServerShutdown)
			{
				printf("Initating self destruct sequence...\n");

				printf("Terminating %i Client(s)\n", ActiveConnections);
				// Remove all Client Connections gracefully.
				while (ConnectionList.begin() != ConnectionList.end())
				{
					TerminateConnection(0);
				}

				// Close the main server socket.
#ifdef _WINDOWS
				closesocket(NetworkSocket);

				WSACleanup();
#else
				close(NetworkSocket);
#endif

				printf("Clean shutdown\n");
				break; // Leave the main while loop, effectively terminating the program.
			}

		} // while (1) : The main loop.
	}

	NetworkDaemon::NetworkDaemon()
	{
		listenPort = 0;
		backLog = 0;
		HighestPollIterator = 0;
	}
	NetworkDaemon::~NetworkDaemon()
	{

	}
}