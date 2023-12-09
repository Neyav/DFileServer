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
	extern std::string VERSIONTITLE;
}

// The method for doing this is different depending on the platform.
// TODO: Considering renaming this file to Configuration.hxx.
#ifdef _WINDOWS
class ConsoleSize
{
private:
	CONSOLE_SCREEN_BUFFER_INFO csbi;

public:
	int Width()
	{
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		return csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
	int Height()
	{
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}
};
#else
class ConsoleSize
{
private:
		struct winsize w;

public:
int Width()
	{
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		return w.ws_col;
	}
	int Height()
	{
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		return w.ws_row;
	}
};
#endif


struct Configuration_t
{
	int Port;
	int BackLog;
	int MaxConnections;
	int MaxBandwidth;
	int ShowConnections;
	ConsoleSize Console;
	std::string BasicCredentials;
	FILE* LogFile;

Configuration_t() // Just some default values.
	{
		Port = 2000;
		BackLog = 100;
		MaxConnections = 0;
		MaxBandwidth = 0;
		ShowConnections = 0;
		BasicCredentials = "";
		LogFile = NULL;
	}
};

extern Configuration_t Configuration;