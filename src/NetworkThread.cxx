
#include "NetworkThread.hxx"
#include "CPathResolver.hxx"
#include "Version.hxx"
#include "ClientConnection.hxx"

namespace DFSNetworking
{
	void NetworkThread::TerminateConnection(int aConnectionIndex)
	{
		int NewHighestPollIterator = 0;
		ClientConnection* deleteClient = ConnectionList[aConnectionIndex]; // TODO: This may not be necessary now...

		/*ActiveConnections--;

		// If serverlockdown was established and this is the last connection, do a complete shutdown.
		if (ServerLockdown)
		{
			if (ActiveConnections == 0)
				ServerShutdown = true;
			else
				printf("Client Disconnected; %i remaining...\n", ActiveConnections);
		}*/

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

	NetworkThread::NetworkThread()
	{
		NetworkThreadID = (size_t)this;
		primeThread = false;
	}

	NetworkThread::NetworkThread(bool aprimeThread)
	{
		NetworkThreadID = (size_t)this;
		primeThread = aprimeThread;
	}

	NetworkThread::~NetworkThread()
	{

	}
}