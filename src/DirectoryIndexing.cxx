/*
** ($Header: /var/www/cvsroot/DFileServer/src/DirectoryIndexing.cxx,v 1.41.2.6 2005/10/04 07:45:52 incubus Exp $)
**
** Copyright 2005 Chris Laverdure
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WINDOWS
#include <windows.h>
#include "contrib/win32dirent.h"
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <dirent.h>
#endif
#include <string>
#include <fstream>
#include <vector>
#include <set>

#include "Version.hxx"

using namespace std;

struct DirectoryEntryStruct 
{ 
	bool Folder;
	string Name;
	string CompletePath;
	int Size;
}; 
 
void URLEncode( string & );
void ParseURLEncoding( char * );

bool operator< ( const DirectoryEntryStruct &FirstElement, const DirectoryEntryStruct &SecondElement ) 
{
	// ../ always wins so it stays at the top.
	if ( FirstElement.Name == ".." )
		return true;
	else if ( SecondElement.Name == ".." )
		return false;

	// First check to see if it is a folder.
	if ( FirstElement.Folder != SecondElement.Folder )
		return FirstElement.Folder > SecondElement.Folder;

	// Do a lexical comparison.
	return FirstElement.Name < SecondElement.Name;
}

static int FileSize ( const char *ArgFileName )
{
	ifstream		FilePointer;
	ifstream::pos_type 	FileSize;

	// Try to open the file.
	FilePointer.open( ArgFileName, ios_base::binary | ios_base::in );

	// If something went wrong..
	if ( !FilePointer.good() || !FilePointer.is_open() )
	{
		FilePointer.close();

		return -1;
	}

	// Determine the size of the file.

	FilePointer.seekg(0, ios_base::end);
	FileSize = FilePointer.tellg();
	FilePointer.seekg(0, ios_base::beg);
	FileSize -= FilePointer.tellg();

	FilePointer.close();

	return static_cast<int>(FileSize);
}

static char *ConvertSizeToFriendly ( int ArgFileSize )
{
	static char FriendlyFileSize[50];

	if ( ArgFileSize == -1 )
	{
		snprintf(FriendlyFileSize, sizeof(FriendlyFileSize), "Cannot Access");
		return (char *) FriendlyFileSize;
	}

	// Try to break it down into friendier bits.
	if ( ArgFileSize > 1073741824) // Gigabytes
		snprintf(FriendlyFileSize, sizeof(FriendlyFileSize), "%iGB", ArgFileSize / 1073741824);
	else if ( ArgFileSize > 1048576) // Megabytes
		snprintf(FriendlyFileSize, sizeof(FriendlyFileSize), "%iMB", ArgFileSize / 1048576);
	else if ( ArgFileSize > 1024) // Kilobytes
		snprintf(FriendlyFileSize, sizeof(FriendlyFileSize), "%iKB", ArgFileSize / 1024);
	else
		snprintf(FriendlyFileSize, sizeof(FriendlyFileSize), "< 1KB");

	// Return with the size in bytes.
	return (char *) FriendlyFileSize;
}

static string FullPath ( string ArgPath, const char *ArgFile )
{
	string Buffer;

	// First copy the path into the buffer.
	Buffer = ArgPath;

	// Does the path end with a slash?
	if ( Buffer[Buffer.size() - 1] != '/' )
		Buffer += "/";

	// Now tag on the filename at the end.
	Buffer += string(ArgFile);

	return Buffer;
}

static bool IsAFolder( string &ArgPath )
{
	DIR *DirectoryPointer = NULL;

	DirectoryPointer = opendir( (char *) ArgPath.c_str() );

	if (!DirectoryPointer)
		return false; // It's not a folder, or doesn't exist.

	closedir( DirectoryPointer );

	return true; // It is a folder.
}

static string InsertFile ( const char *ArgPath, const char *ArgFile )
{
	string FileName;
	string FileData;
	ifstream                FilePointer;
	ifstream::pos_type      FileSize;
	char *TempSpace;

	// Get the full file path. 
	FileName = FullPath ( ArgPath, ArgFile );

	// Try to open the file.
	FilePointer.open( FileName.c_str(), ios_base::binary | ios_base::in );

	// If something went wrong..
	if ( !FilePointer.good() || !FilePointer.is_open() )
	{
		FilePointer.close();

		return FileData;
	}

	// Determine the size of the file.

	FilePointer.seekg(0, ios_base::end);
	FileSize = FilePointer.tellg();
	FilePointer.seekg(0, ios_base::beg);
	FileSize -= FilePointer.tellg();

	TempSpace = (char *) malloc( FileSize );  
	TempSpace[FileSize] = '\0';

	// Read all the information from the file into this char.
        FilePointer.read ( TempSpace, FileSize );
	FilePointer.close();

        FileData = TempSpace;
                 
        free ( TempSpace );

	return FileData;
}

static string InsertIndexTable ( DIR *DirectoryPointer, char *ArgVirtualPath, string ArgPath, set<string> &ArgHidden )
{
        struct dirent *DirentPointer = NULL;
        bool AlternatingVariable = false;
	string Buffer;
        vector<DirectoryEntryStruct> DirectoryVector;

	// Top Table row.
	Buffer += "<table id=\"DFS_table\">\n<tr class=\"DFS_headertablerow\"><th class=\"DFS_entrytype\">Entry Type</th><th class=\"DFS_entryname\">Entry Name</th><th class=\"DFS_entrysize\">Entry Size</th></tr>\n";
         
	// Walk through the folder grabbing files and adding them to our vector.
	while ( ( DirentPointer = readdir( DirectoryPointer ) ) != NULL )
	{
		string CompleteVirtualPath;
		string CompleteRealPath;
		DirectoryEntryStruct DirectoryEntry;

		// In UNIX, hidden files start with periods. This is a good policy.
		if ( DirentPointer->d_name[0] == '.')
		{
			// We must of course allow for .., so make a special case.
			if ( !(strlen( DirentPointer->d_name ) == 2
					&& DirentPointer->d_name[1] == '.') )
				continue; // Don't display it.
		}

		DirectoryEntry.Name = string( DirentPointer->d_name );

		// Check through the ArgHidden vector for files that shouldn't be displayed.
		if ( ArgHidden.find( DirectoryEntry.Name ) != ArgHidden.end() )
			continue;

		// Make sure the path has no URL encodings in it so they don't get URL encoded twice.
		ParseURLEncoding ( ArgVirtualPath );

		DirectoryEntry.CompletePath = FullPath( ArgVirtualPath, DirentPointer->d_name );

		// URL encode the link
		URLEncode (DirectoryEntry.CompletePath);
		
		CompleteRealPath = FullPath( ArgPath, DirentPointer->d_name);

		DirectoryEntry.Folder = IsAFolder ( CompleteRealPath );

		// If it's not a folder, grab the size.
		if ( !DirectoryEntry.Folder )
			DirectoryEntry.Size = FileSize ( CompleteRealPath.c_str() );

		// Is the file accessable?
		if ( DirectoryEntry.Size == -1 )
			continue; // Don't add it to the vector.

		DirectoryVector.push_back ( DirectoryEntry );
	}

	// Now sort it!
	sort(DirectoryVector.begin(), DirectoryVector.end());

	// Scan through all the files in it.
	for ( unsigned int Iterator = 0; Iterator < DirectoryVector.size(); Iterator++ )
	{
		// Resize it if we're about to run out of space.
		if ( Buffer.capacity() - Buffer.size() < 100 )
			Buffer.reserve(1000);
            
		// Alternate the CSS class for every other line.
		if ( AlternatingVariable == true )
		{
			Buffer += "<tr class=\"DFS_eventablerow\">";
			AlternatingVariable = false;
		}
		else
		{
			Buffer += "<tr class=\"DFS_oddtablerow\">";
			AlternatingVariable = true;
		}

		// Entry Type
		if ( DirectoryVector[Iterator].Folder )
			Buffer += "<td class=\"DFS_entrytype\">[DIR]</td>";
		else
			Buffer += "<td class=\"DFS_entrytype\">[FILE]</td>";

		// Entry Name
		if ( DirectoryVector[Iterator].Folder )
		{ // Give folders slashes on the end.
			Buffer += "<td class=\"DFS_entryname\"><a href=\"" + DirectoryVector[Iterator].CompletePath +
						"/\" class=\"DFS_direntry\">" + DirectoryVector[Iterator].Name + "/</a></td>";
		}
		else
		{
			Buffer += "<td class=\"DFS_entryname\"><a href=\"" + DirectoryVector[Iterator].CompletePath +
						"\" class=\"DFS_fileentry\">" + DirectoryVector[Iterator].Name + "</a></td>";
		}

		// Entry Size
		if ( DirectoryVector[Iterator].Folder )
			Buffer += "<td class=\"DFS_entrysize\">&nbsp;</td></tr>\n";
		else
			Buffer += "<td class=\"DFS_entrysize\">" + string( ConvertSizeToFriendly( DirectoryVector[Iterator].Size ) ) + "</td></tr>\n";
	}

	// End the table.
	Buffer += "</table>";

	// Spit the completed index table back into the fuction that called us.
	return Buffer;
}
	
static string ParseTemplate ( ifstream &ArgFile, char *ArgVirtualPath, string ArgPath, DIR *DirectoryPointer )
{
	char *TempSpace;
	int FileSize;
	bool ActiveLoop = true;
	string TemplateData;
	string::size_type StringPosition;
	string::size_type LowestMatch = 0;
	set<string> FilesToHide;

	// First determine the size of the file.
        ArgFile.seekg(0, ios_base::end);
        FileSize = static_cast<int>( ArgFile.tellg() );
        ArgFile.seekg(0, ios_base::beg);
        FileSize -= static_cast<int>( ArgFile.tellg() );

	TempSpace = (char *) malloc( FileSize );
	TempSpace[FileSize] = '\0';

	// Read all the information from the file into this char.
	ArgFile.read ( TempSpace, FileSize );

	// Dump the char into a string, parse and replace all "keywords"

	TemplateData = TempSpace;

	free ( TempSpace );
	
	while (ActiveLoop)
	{
		string::size_type StringSize;
		string::size_type LocalLowestMatch = 0;
		int StringOffset = 0;

		StringSize = TemplateData.size();
		ActiveLoop = false;

		// $$LOCATION$$
		StringPosition = TemplateData.find ("$$LOCATION$$", LowestMatch);
		if ( StringPosition != string::npos )
		{
			TemplateData.replace( StringPosition, sizeof("$$LOCATION$$") - 1, string(ArgVirtualPath) );

			if ( StringPosition < LocalLowestMatch || LocalLowestMatch == 0 )
				LocalLowestMatch = StringPosition;

			StringOffset += strlen(ArgVirtualPath) - sizeof("$$LOCATION$$");			
			ActiveLoop = true;
		}
		// $$SERVERVERSION$$
		StringPosition = TemplateData.find ("$$SERVERVERSION$$", LowestMatch);
		if ( StringPosition != string::npos )
		{
			string VersionString("DashFileServer [Version " + string(MAJORVERSION) + "." + string(MINORVERSION) + "." + string(PATCHVERSION) + "]");

			TemplateData.replace( StringPosition, sizeof("$$SERVERVERSION$$") - 1, VersionString );

			if ( StringPosition < LocalLowestMatch || LocalLowestMatch == 0 )
				LocalLowestMatch = StringPosition;

			StringOffset += VersionString.size() - sizeof("$$SERVERVERSION$$");
			ActiveLoop = true;
		}
		// $$INSERTFILE(*)$$
		StringPosition = TemplateData.find ("$$INSERTFILE(", LowestMatch);
		if ( StringPosition != string::npos )
		{
			string FileData;
			string::size_type StringEndPosition;

			// Find where the end of this tag is...
			StringEndPosition = TemplateData.find (")$$", StringPosition);

			// Make sure the tag ends
			if ( StringEndPosition != string::npos )
			{
				string TempFileName;
				string::size_type TempPosition;

				// Now we Seperate the chunck between these two positions.
				TempPosition = StringPosition + sizeof("$$INSERTFILE");				
				TempFileName = TemplateData.substr ( TempPosition, StringEndPosition - TempPosition );

				// Make sure the filename doesn't contain certain elements that may be used
				// to redirect the folder, and make sure that it's not a file that has already
				// been included. ( Included files can have $$INSERTFILE tags, and they will be parsed.
				// 		    If an inserted file inserts itself, it will recurse until the computer
				//		    runs out of ram and the program dies. Not cool. )
				if ( TempFileName.find("/", 0) == string::npos &&
					TempFileName.find("\\", 0) == string::npos && 
					TempFileName.find("~", 0) == string::npos &&
					FilesToHide.find( TempFileName ) == FilesToHide.end() )
				{ 
					// Load the file into our string and add it to the FilesToHide vector
					// so that it doesn't show up in our vector.
					FileData = InsertFile( ArgPath.c_str(), TempFileName.c_str() );
					FilesToHide.insert( TempFileName );
				}
				else
					FileData.clear();

				// We attempt to substitute $$INSERTFILE(*)$$ with the contents of FileData.
				// This will simply remove the tag if FileData is blank.
				TemplateData.replace( StringPosition, (StringEndPosition + 3) - StringPosition,
							FileData );
			}
			else
			{ // The tag is incomplete, cripple it so we don't scan it again.
				TemplateData[StringPosition] = '^';
				// Cancel out any offset detection.
				StringPosition = ( StringEndPosition + 3 );
			}

			if ( StringPosition < LocalLowestMatch || LocalLowestMatch == 0 )
				LocalLowestMatch = StringPosition;

			StringOffset += FileData.size() - ( ( StringEndPosition + 3 ) - StringPosition );
			ActiveLoop = true;
		}

		// Set the global lowest match to the local one.
		// This is to save time on the searches.
		LowestMatch = LocalLowestMatch;

		// Check for a Negative offset.
		if ( StringOffset < 0 )
		{
			string::size_type StringNewEnd;
	
			// Change it to a positive offset.
			StringOffset *= -1;

			// New end of the string.
			StringNewEnd = StringSize - StringOffset;

			// Zap the tail of the string, if it exists.
			TemplateData.erase ( StringNewEnd, StringOffset );			
		}

	}

	// This is a special case for the INDEX. We do it last and after the loop so that noting inside the index gets
	// parsed and replaced. Not that I expect that to happen, but since when were such expectations safe?
	StringPosition = TemplateData.find ("$$INDEXTABLE$$", 0);
	if ( StringPosition != string::npos )
		TemplateData.replace( StringPosition, sizeof("$$INDEXTABLE$$") - 1, 
					InsertIndexTable( DirectoryPointer, ArgVirtualPath, ArgPath, FilesToHide ) );

	return TemplateData;
}

char GenerateFolderIndex( string ArgVirtualPath, char *ArgPath, string &ArgBuffer )
{
	DIR *DirectoryPointer = NULL;
	int FileTestDescriptor;
	ifstream FileStream;
	vector<DirectoryEntryStruct> DirectoryVector;
	char VirtualPath[255];

	// This is a bit stupid, but I don't want to spend all night converting shit to std::string just to get it building again
	// before sunrise.
	strncpy ( VirtualPath, ArgVirtualPath.c_str(), sizeof(VirtualPath) );

	// Open the directory.
	DirectoryPointer = opendir( ArgPath );

	if ( DirectoryPointer == NULL )
	{
		// Try to open it as a file now.
		FileTestDescriptor = open( ArgPath, O_RDONLY );

		if ( FileTestDescriptor < 1 )
			return -1; // Path is invalid.
		else
		{
			close ( FileTestDescriptor );
			return 0;  // Path points to a file.
		}
	}

	// Check for index.htm and index.html
	if ( ( FileTestDescriptor = open( FullPath( ArgPath, "index.html" ).c_str(), O_RDONLY ) ) > 0 )
	{ // Index.html found.
		close ( FileTestDescriptor );
		strcat( ArgPath, "/index.html");
		return 0; 
	}
	if ( ( FileTestDescriptor = open( FullPath( ArgPath, "index.htm" ).c_str(), O_RDONLY ) ) > 0 )
	{ // Index.htm found.
		close ( FileTestDescriptor );
		strcat( ArgPath, "/index.htm");
		return 0;
	}

	if ( ArgBuffer.empty() )
		ArgBuffer.reserve(1000);

	// Toss in a custom template in if one exists. If not, just use a generic form.
	FileStream.open ("indextemplates/default.html", ios::in ); // Open our template header.
	if ( !FileStream.is_open() || !FileStream.good() )
	{ // It doesn't exist, so toss in a generic header.
		set<string> Filler;

		// <html> and <head>
		ArgBuffer = "<html>\n<head>\n<title>Directory listing for " + string(ArgVirtualPath) + "</title>\n";
		// A clone of KDE's public fileserver CSS sheet
		ArgBuffer += "<style type=\"text/css\">\n<!--\n";
		ArgBuffer += "body { font-family: Verdana, Arial, sans-serif;}\n#DFS_table {color: rgb(0, 0, 0);background-color: rgb(234, 233, 232); border: thin outset; width: 100%; }\n";
		ArgBuffer += "#DFS_table td { margin: 0px; white-space: nowrap; }\n#DFS_table a { color: rgb(0, 0, 0); text-decoration: none; }\n";
		ArgBuffer += "#DFS_table th { color: rgb(0, 0, 0); background-color: rgb(230, 240, 249); text-align: left; white-space: nowrap; border: thin outset; }\n";
		ArgBuffer += "tr.DFS_eventablerow { background-color: rgb(255, 255, 255); color: rgb (0, 0, 0); }\n";
		ArgBuffer += "tr.DFS_oddtablerow { background-color: rgb(238, 246, 255); color: rgb(0, 0, 0); }\n";
		ArgBuffer += ".DFS_entrytype { text-align: center; width: 6em; }\ntd.DFS_entrytype { background-color: rgb(255, 255, 255); font-weight: bold; color: rgb(150, 150, 150);}\n";
		ArgBuffer += ".DFS_entrysize { color: rgb(0, 0, 0); text-align: right;}\na.DFS_direntry { font-weight: bold; }\n";
		ArgBuffer += "-->\n</style>\n";
		// Insert the table.
		ArgBuffer += InsertIndexTable( DirectoryPointer, VirtualPath, string( ArgPath ), Filler );
		// Server Version Information.
		ArgBuffer += "<hr><i>DashFileServer [Version " + string(MAJORVERSION) + "." + string(MINORVERSION) + "." + string(PATCHVERSION) + "]</i>\n";
		// End the index
		ArgBuffer += "</body>\n</html>\n"; 
	}
	else
	{ // Let's grab the one from the template and parse it.
		ArgBuffer += ParseTemplate( FileStream, VirtualPath, string( ArgPath ), DirectoryPointer );
		FileStream.close();
	}

	closedir( DirectoryPointer );

	return 1;
}
