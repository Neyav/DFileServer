#pragma once
#include <string>

#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strncpy strncpy_s

#ifndef INFTIM
#define INFTIM -1
#endif

#define MAXIDENTIFIERLEN 151

namespace Version
{
	extern int MAJORVERSION;
	extern int MINORVERSION;
	extern int PATCHVERSION;
}

struct Configuration_t
{
	int Port;
	int BackLog;
	int MaxConnections;
	int MaxBandwidth;
	int ShowConnections;
	std::string BasicCredentials;
	FILE* LogFile;
};

extern Configuration_t Configuration;