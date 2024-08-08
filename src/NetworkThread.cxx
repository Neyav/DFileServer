
#include "NetworkThread.hxx"
#include "CPathResolver.hxx"
#include "Version.hxx"
#include "ClientConnection.hxx"
#include "DirectoryIndexing.hxx"
#include "InterProcessMessaging.hxx"
#include "MimeTypes.hxx"
#include "contrib/Base64.h"

#include <thread>

namespace DFSNetworking
{
	void NetworkThread::TerminateConnection(int aConnectionIndex)
	{
		int NewHighestPollIterator = 0;
		ClientConnection* deleteClient = ConnectionList[aConnectionIndex]; // TODO: This may not be necessary now...

		localConnections--;

		// Iterate through the poll structure to find one that matches our clients socket, and remove it.
		for (int PollStructIterator = 0; PollStructIterator < PollStruct.size(); PollStructIterator++)
		{
			if (PollStruct[PollStructIterator].fd == deleteClient->GetSocket())
			{
				PollStruct.erase(PollStruct.begin() + PollStructIterator);
				break;
			}
		}

		// Remove the client from the linked list.
		ConnectionList.erase(ConnectionList.begin() + aConnectionIndex);

		delete deleteClient; // Had to reference it seperately to avoid chicken/egg problem with remove from list versus
		// delete or delete then remove from list.

		return;
	}

	void NetworkThread::Buffer404(ClientConnection* ArgClient)
	{
		// This fills the buffer with the 404 error.

		ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Resource not found</TITLE></HEAD><BODY><H1>Resource not found</H1>";
		ArgClient->SendBuffer += "The resource you were trying to locate doesn't exist on this server.<br><br><HR>";
		ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "] -{" + Version::VERSIONTITLE + "}- </I>";
		ArgClient->SendBuffer += "</BODY></HTML>";
	}

	void NetworkThread::Buffer401(ClientConnection* ArgClient)
	{
		// This fills the buffer with the 401 error.

		ArgClient->SendBuffer = "<HTML><HEAD><TITLE>Error: Authorization Required</TITLE></HEAD><BODY><H1>Authorization Required</H1>";
		ArgClient->SendBuffer += "The resource you were trying to locate requires authorization on this server.<br><br><HR>";
		ArgClient->SendBuffer += "<I>DFileServer [Version " + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "] -{" + Version::VERSIONTITLE + "}- </I>";
		ArgClient->SendBuffer += "</BODY></HTML>";
	}

	char NetworkThread::LocateResource(std::string Resource, ClientConnection* ArgClient, char* DstResource, char* DstResourceType)
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

	void NetworkThread::ParseURLEncoding(char* ArgBuffer)
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

	void NetworkThread::NetworkThreadLoop()
	{
		while (1)
		{
			// We have no connections, if we're a prime thread, wait forever, otherwise wait for 2 minutes, after 2 minutes with no messages, self destruct.
			if (ConnectionList.size() == 0)
			{ 				
				if (primeThread)
				{
					NetworkThreadMessenger->pauseForMessage();
				}
				else
				{
					NetworkThreadMessenger->pauseForMessage(120000);
					if (!NetworkThreadMessenger->HasMessages())
					{
						NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, "Network Thread[" + std::to_string(NetworkThreadID) + "] has been idle for 2 minutes and is self destructing.");
						delete this;
						return;
					}
				}
			}

			// Check for messages.
			if (NetworkThreadMessenger && NetworkThreadMessenger->HasMessages())
			{
				DFSMessaging::Message NewMessage = NetworkThreadMessenger->AcceptMessage();

				if (NewMessage.Pointer && NewMessage.acceptTask())
				{
					ClientConnection* NewConnection = (ClientConnection*)NewMessage.Pointer;

					// Add the new connection to the poll structure.
					struct pollfd NewPollStruct;
					NewPollStruct.fd = NewConnection->GetSocket();
					NewPollStruct.events = (POLLIN | POLLOUT);
					PollStruct.push_back(NewPollStruct);

					// Add the new connection to the linked list.
					ConnectionList.push_back(NewConnection);

					localConnections++;

					// Send a message to the console.
					NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, "New connection accepted from " + std::string(NewConnection->GetIP()));
				}				
			}
			
			// Go through the list looking for connections that have incoming data.
			for (int ConnectionListIterator = 0; ConnectionListIterator < ConnectionList.size(); ConnectionListIterator++)
			{
				// This socket has incoming data. Here I'm counting on PollStruct and ConnectionList being in the same
				// order as each other, as they only EVER get deleted together, and added together. 
				if (PollStruct[ConnectionListIterator].revents & POLLIN)
				{
					char DataBuffer[500]; // 500 should be big enough.
					size_t DataRecved;

					if ((DataRecved = ConnectionList[ConnectionListIterator]->RecvData(DataBuffer, sizeof(DataBuffer))) < 1)
					{ // Disconnection or error. Terminate client.

						NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, std::string(ConnectionList[ConnectionListIterator]->GetIP()) + " - Disconnected w/o complete data.");

						TerminateConnection(ConnectionListIterator);
						ConnectionListIterator--;
						if (ConnectionListIterator < 0)
							ConnectionListIterator = 0;
						continue;
					}
					else
					{ // Incoming data.
						ConnectionList[ConnectionListIterator]->BrowserRequest.ImportHeader(std::string(DataBuffer));

						// Find what resource this person is after.
						ConnectionList[ConnectionListIterator]->Resource = ConnectionList[ConnectionListIterator]->BrowserRequest.AccessPath;
					}

				} // [/END] Incoming Data on socket.

				if (ConnectionListIterator < ConnectionList.size() && !ConnectionList[ConnectionListIterator]->Resource.empty() &&
					PollStruct[ConnectionListIterator].revents & POLLOUT)
				{

					if (!ConnectionList[ConnectionListIterator]->FileStream
						&& ConnectionList[ConnectionListIterator]->SendBuffer.empty())
					{
						char Resource[151];
						char ResourceType[151];
						size_t ResourceSize = 0;

						// We need to locate this resource.
						if (LocateResource(ConnectionList[ConnectionListIterator]->Resource, ConnectionList[ConnectionListIterator], Resource, ResourceType) == -1)
						{ // Resource wasn't found.
							Buffer404(ConnectionList[ConnectionListIterator]);
							ResourceSize = ConnectionList[ConnectionListIterator]->BytesRemaining = ConnectionList[ConnectionListIterator]->SendBuffer.size();
							strcpy(Resource, "--404--");
							strcpy(ResourceType, "text/html");

							continue;
						}

						ParseURLEncoding(Resource);

						// Is it a redirect?
						if (strcmp(ResourceType, "redirect") == 0)
						{
							char direrror;

							direrror = GenerateFolderIndex(ConnectionList[ConnectionListIterator]->Resource, Resource, ConnectionList[ConnectionListIterator]->SendBuffer);

							if (direrror == -1)
							{ // File/Folder doesn't exist
								Buffer404(ConnectionList[ConnectionListIterator]);
								ResourceSize = ConnectionList[ConnectionListIterator]->BytesRemaining = ConnectionList[ConnectionListIterator]->SendBuffer.size();
								strcpy(Resource, "--404--");
								strcpy(ResourceType, "text/html");
							}
							else if (direrror)
							{ // It generated an index for us.

								ConnectionList[ConnectionListIterator]->SendBufferIterator = 0;

								ResourceSize = ConnectionList[ConnectionListIterator]->BytesRemaining = ConnectionList[ConnectionListIterator]->SendBuffer.size();

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

						NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, std::string(ConnectionList[ConnectionListIterator]->GetIP()) + " - [" + ConnectionList[ConnectionListIterator]->Resource + "]");
						NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, "Resolved resource to: -> " + std::string(Resource));

						// If there is no sendbuffer, try to open the file, and if you can't, terminate the connection.
						if (ConnectionList[ConnectionListIterator]->SendBuffer.empty() &&
							((ResourceSize = ConnectionList[ConnectionListIterator]->OpenFile(Resource)) == -1))
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

							ConnectionList[ConnectionListIterator]->ServerResponse.AccessType = "HTTP/1.1";
							ConnectionList[ConnectionListIterator]->ServerResponse.AccessPath = "200";
							ConnectionList[ConnectionListIterator]->ServerResponse.AccessProtocol = "OK";

							if (!Configuration.BasicCredentials.empty()) // Authentication Required
							{
								if (ConnectionList[ConnectionListIterator]->BrowserRequest.GetValue("Authorization").empty())
								{ // Authentication wasn't attempted, send the request.
									ConnectionList[ConnectionListIterator]->ServerResponse.SetValue("WWW-Authenticate",
										"Basic realm=\"DFileServer\"");
									ConnectionList[ConnectionListIterator]->ServerResponse.AccessPath = "401"; // Authorization Required
								}
								else
								{ // Try to verify the authentication.
									Base64 Base64Encoding;
									std::string Base64Authorization;

									Base64Authorization = ConnectionList[ConnectionListIterator]->BrowserRequest.GetValue("Authorization");

									Base64Authorization.erase(0, 6); // Remove the beginning "Basic "
									Base64Authorization.erase(Base64Authorization.length() - 1, 1); // Remove the ending

									if (Base64Encoding.decode(Base64Authorization) != Configuration.BasicCredentials)
									{
										if (Base64Authorization == "")
											NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, std::string(ConnectionList[ConnectionListIterator]->GetIP()) + " - Protected Resource Requested; Authentication Requested.");
										else
											NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, std::string(ConnectionList[ConnectionListIterator]->GetIP()) + " - Protected Resource Requested; Failed Authentication.");

										ConnectionList[ConnectionListIterator]->ServerResponse.AccessPath = "401"; // Authorization Required

										ConnectionList[ConnectionListIterator]->CloseFile(); // Make sure we close the resource.

										// Prepare the 401...
										Buffer401(ConnectionList[ConnectionListIterator]);
										ResourceSize = ConnectionList[ConnectionListIterator]->BytesRemaining = ConnectionList[ConnectionListIterator]->SendBuffer.size();
										strcpy(Resource, "--401--");
										strcpy(ResourceType, "text/html");
									}

								}
							}

							sprintf(Buffer, "DFileServer/%i.%i.%i", Version::MAJORVERSION, Version::MINORVERSION, Version::PATCHVERSION);

							ConnectionList[ConnectionListIterator]->ServerResponse.SetValue("Server", std::string(Buffer));
							ConnectionList[ConnectionListIterator]->ServerResponse.SetValue("Connection", "close");

							sprintf(Buffer, "%i", (int)ResourceSize);

							ConnectionList[ConnectionListIterator]->ServerResponse.SetValue("Content-Length", std::string(Buffer));
							ConnectionList[ConnectionListIterator]->ServerResponse.SetValue("Content-Type", std::string(ResourceType));

							ConnectionList[ConnectionListIterator]->SendData(
								(char*)ConnectionList[ConnectionListIterator]->ServerResponse.ExportHeader().c_str(), 0);
						}
					}
					else
					{ // Send 4096 bytes of the file the way of the client.
						char Buffer[4096];
						int BytesToRead, BytesRead;

						// Clear the buffer.
						memset(Buffer, '\0', sizeof(Buffer));

						BytesToRead = sizeof(Buffer) - 1;

						if (Configuration.MaxBandwidth)
						{
							// -- Bandwidth Control --
							if (ConnectionList[ConnectionListIterator]->LastBandReset != time(NULL))
							{
								ConnectionList[ConnectionListIterator]->BandwidthLeft = Configuration.MaxBandwidth / localConnections;

								ConnectionList[ConnectionListIterator]->LastBandReset = time(NULL);
							}

							if (BytesToRead > ConnectionList[ConnectionListIterator]->BandwidthLeft)
								BytesToRead = ConnectionList[ConnectionListIterator]->BandwidthLeft;

							ConnectionList[ConnectionListIterator]->BandwidthLeft -= BytesToRead;
							// / -- Bandwidth Control --
						}

						if (BytesToRead > ConnectionList[ConnectionListIterator]->BytesRemaining)
							BytesToRead = ConnectionList[ConnectionListIterator]->BytesRemaining;

						if (ConnectionList[ConnectionListIterator]->FileStream)
						{ // Read from a file
							ConnectionList[ConnectionListIterator]->FileStream->read(Buffer, BytesToRead);
						}
						else
						{ // Send from the buffer

							strcpy(Buffer, ConnectionList[ConnectionListIterator]->SendBuffer.substr(
								ConnectionList[ConnectionListIterator]->SendBufferIterator, BytesToRead).c_str());

							// Increment Iterator
							ConnectionList[ConnectionListIterator]->SendBufferIterator += BytesToRead;
						}

						BytesRead = ConnectionList[ConnectionListIterator]->SendData(Buffer, BytesToRead);

						ConnectionList[ConnectionListIterator]->BytesRemaining -= BytesRead;

						// It hasn't sent all the bytes we've read.
						if (BytesRead != BytesToRead)
						{
							if (ConnectionList[ConnectionListIterator]->FileStream)
							{
								// Relocate the file pointer to match where we are.

								int LocationOffset = BytesToRead - BytesRead;
								std::ifstream::pos_type CurrentPosition = ConnectionList[ConnectionListIterator]->FileStream->tellg();

								ConnectionList[ConnectionListIterator]->FileStream->seekg((CurrentPosition - (std::ifstream::pos_type)LocationOffset));
							}
							else
							{
								// Relocate the iterator to match where we are.

								int LocationOffset = BytesToRead - BytesRead;

								ConnectionList[ConnectionListIterator]->SendBufferIterator -= LocationOffset;
							}
						}

						// End of the file?
						if (ConnectionList[ConnectionListIterator]->BytesRemaining == 0)
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
				if (ConnectionList[ConnectionListIterator]->SecondsIdle() > 15)
				{
					TerminateConnection(ConnectionListIterator);
					ConnectionListIterator--;
					if (ConnectionListIterator < 0)
						ConnectionListIterator = 0;
					continue;
				}

			} // The linked list for loop.

		}
	}

	NetworkThread::NetworkThread()
	{
		NetworkThreadID = (size_t)this;
		primeThread = false;
		localConnections = 0;

		if (MessengerServer)
		{
			NetworkThreadMessenger = MessengerServer->ReceiveActiveMessenger();
			NetworkThreadMessenger->Name = "Network Thread[" + std::to_string(NetworkThreadID) + "]";
		}
		else
			NetworkThreadMessenger = nullptr;

		// Start Network Thread
		std::thread NThread(&NetworkThread::NetworkThreadLoop, this);
		NThread.detach();
	}

	NetworkThread::NetworkThread(bool aprimeThread)
	{
		NetworkThreadID = (size_t)this;
		primeThread = aprimeThread;
		localConnections = 0;

		if (MessengerServer)
		{
			NetworkThreadMessenger = MessengerServer->ReceiveActiveMessenger();
			NetworkThreadMessenger->Name = "Network Thread[" + std::to_string(NetworkThreadID) + "]";
			NetworkThreadMessenger->RegisterOnChannel(MSG_TARGET_NETWORK);
			NetworkThreadMessenger->SendMessage(MSG_TARGET_CONSOLE, "Network Thread initialized.");
		}
		else
			NetworkThreadMessenger = nullptr;

		// Start Network Thread
		std::thread NThread(&NetworkThread::NetworkThreadLoop, this);
		NThread.detach();
	}

	NetworkThread::~NetworkThread()
	{
		if (NetworkThreadMessenger)
			delete NetworkThreadMessenger;
	}
}