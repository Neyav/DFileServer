#pragma once
#include <string>
#include <cstring>
#include <iostream>

#ifdef _WINDOWS
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strncpy strncpy_s
#endif

#ifndef INFTIM
#define INFTIM -1
#endif

#ifndef _WINDOWS
#include <termios.h>
#include <sys/ioctl.h>
#endif

#define MAXIDENTIFIERLEN 151

// Define color codes
#define FG_COLOUR		"\033[3";
#define BG_COLOUR		"\033[4";
#define CC_BLACK		0
#define CC_RED			1
#define CC_GREEN		2
#define CC_YELLOW		3
#define CC_BLUE			4
#define CC_MAGENTA		5
#define CC_CYAN			6
#define CC_WHITE		7

// Define other formatting codes
#define BOLD      "\033[1m"
#define RESET     "\033[0m"

namespace Version
{
	extern int MAJORVERSION;
	extern int MINORVERSION;
	extern int PATCHVERSION;
	extern std::string VERSIONTITLE;
}

void DFSleep(int milliseconds);

// The method for doing this is different depending on the platform.
// TODO: Considering renaming this file to Configuration.hxx.
#ifdef _WINDOWS
class ConsoleControl
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
	void gotoxy(int x, int y)
	{
		COORD coord;
		coord.X = x;
		coord.Y = y;
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	}
	// Function to set text color
	void setColour(int colourCode) 
	{
		std::cout << FG_COLOUR
		std::cout << colourCode << "m";		
	}

	// Function to set background color
	void setBackground(int colourCode) 
	{
		std::cout << BG_COLOUR
		std::cout << colourCode << "m";
	}

	// Function to set bold text
	void setBold() 
	{
		std::cout << BOLD;
	}

	// Function to reset formatting
	void reset() 
	{
		std::cout << RESET;
	}

	void clearScreen()
	{
		std::cout << "\u001B[2J\u001B[1;1H";
	}
};
#else
class ConsoleControl
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
	void gotoxy(int x, int y)
	{
		std::cout << "\033[" << y << ";" << x << "H";
	}
	// Function to set text color
	void setColour(int colourCode)
	{
		std::cout << FG_COLOUR
		std::cout << colourCode << "m";
	}

	// Function to set background color
	void setBackground(int colourCode)
	{
		std::cout << BG_COLOUR
		std::cout << colourCode << "m";
	}

	// Function to set bold text
	void setBold()
	{
		std::cout << BOLD;
	}

	// Function to reset formatting
	void reset()
	{
		std::cout << RESET;
	}

	void clearScreen()
	{
		std::cout << "\u001B[2J\u001B[1;1H";
	}
};
#endif


struct Configuration_t
{
	int Port;
	int primeThreads;
	int BackLog;
	int MaxConnections;
	int MaxBandwidth;
	bool interactiveConsole;
	bool Verbose;
	ConsoleControl Console;
	std::string BasicCredentials;
	FILE* LogFile;

Configuration_t() // Just some default values.
	{
		Port = 2000;
		primeThreads = 4;
		BackLog = 100;
		MaxConnections = 0;
		MaxBandwidth = 0;
		Verbose = true;
		interactiveConsole = false;
		BasicCredentials = "";
		LogFile = NULL;
	}
};

extern Configuration_t Configuration;
