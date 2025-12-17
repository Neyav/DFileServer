/*
** ($Header: /var/www/cvsroot/DFileServer/src/DirectoryIndexing.cxx,v 1.41.2.6 2005/10/04 07:45:52 incubus Exp $)
**
** Copyright 2005, 2018 Chris Laverdure
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
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#define _close close
#define _open open
#endif
#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>

#include "Version.hxx"

using namespace std;

struct DirectoryEntryStruct 
{ 
	bool Folder;
	std::string Name;
	std::string CompletePath;
	int Size;
}; 
 
void URLEncode( std::string & );
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

int FileSize ( const char *ArgFileName )
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

char *ConvertSizeToFriendly ( int ArgFileSize )
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

static std::string FullPath ( std::string ArgPath, const char *ArgFile )
{
	std::string Buffer;

	// First copy the path into the buffer.
	Buffer = ArgPath;

	// Does the path end with a slash?
	if ( Buffer[Buffer.size() - 1] != '/' )
		Buffer += "/";

	// Now tag on the filename at the end.
	Buffer += std::string(ArgFile);

	return Buffer;
}

std::string InsertFile ( const char *ArgPath, const char *ArgFile )
{
	std::string FileName;
	std::string FileData;
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
                 
        delete TempSpace;

	return FileData;
}

static std::string InsertIndexTable ( std::string *ArgVirtualPath, std::string ArgPath, set<std::string> &ArgHidden )
{
    struct dirent *DirentPointer = NULL;
    bool AlternatingVariable = false;
	std::string Buffer;
    std::vector<DirectoryEntryStruct> Directoryvector;
         
	// Ensure we have a .. entry, unless we are at the root.
	if (*ArgVirtualPath != "/")
	{
		DirectoryEntryStruct DirectoryEntry;
		DirectoryEntry.Name = "..";
		DirectoryEntry.CompletePath = FullPath(*ArgVirtualPath, "..");
		DirectoryEntry.Folder = true;
		DirectoryEntry.Size = 0;
		Directoryvector.push_back(DirectoryEntry);
	}

	// Walk through the folder grabbing files and adding them to our std::vector, using the new filesystem library.
    for (const auto& entry : filesystem::directory_iterator(ArgPath))
	{
		// In UNIX, hidden files start with periods. This is a good policy.
		if ( entry.path().filename().string()[0] == '.')
		{
			// We must of course allow for .., so make a special case.
			if ( !(entry.path().filename().string().size() == 2
					&& entry.path().filename().string()[1] == '.') )
				continue; // Don't display it.
		}

		DirectoryEntryStruct DirectoryEntry;

		DirectoryEntry.Name = entry.path().filename().string();

		// Check through the ArgHidden std::vector for files that shouldn't be displayed.
		if ( ArgHidden.find( DirectoryEntry.Name ) != ArgHidden.end() )
			continue;

		DirectoryEntry.CompletePath = FullPath( *ArgVirtualPath, DirectoryEntry.Name.c_str() );

		// URL encode the link
		URLEncode (DirectoryEntry.CompletePath);
		
		DirectoryEntry.Folder = entry.is_directory();

		// If it's not a folder, grab the size.
		if ( !DirectoryEntry.Folder )
			DirectoryEntry.Size = FileSize ( entry.path().string().c_str() );
		else
			DirectoryEntry.CompletePath += "/"; // Add a trailing slash to the folder.

		// Is the file accessible?
		if ( DirectoryEntry.Size == -1 )
			continue; // Don't add it to the std::vector.

		Directoryvector.push_back ( DirectoryEntry );
	}



	// Now sort it!
	sort(Directoryvector.begin(), Directoryvector.end());

	// Scan through all the files in it.
	for ( unsigned int Iterator = 0; Iterator < Directoryvector.size(); Iterator++ )
	{
		// Resize it if we're about to run out of space.
		if ( Buffer.capacity() - Buffer.size() < 100 )
			Buffer.reserve(10000);
     
		Buffer += "{ name: '" + Directoryvector[Iterator].Name + "'";

		if (Directoryvector[Iterator].Folder)
		{
			Buffer += ", type : 'folder'";
		}
		else
		{
			Buffer += ", size : '" + std::string(ConvertSizeToFriendly(Directoryvector[Iterator].Size)) + "'";
			Buffer += ", type : 'file'";
		}

		Buffer += ", src : '" + Directoryvector[Iterator].CompletePath + "' },\n";
	}

	// Spit the completed index table back into the fuction that called us.
	return Buffer;
}

char GenerateFolderIndex( std::string ArgVirtualPath, char *ArgPath, std::string &ArgBuffer )
{
	int FileTestDescriptor;
	ifstream FileStream;
	std::vector<DirectoryEntryStruct> Directoryvector;

	// Determine whether it's a directory or a filesystem. Now using C++17 filesystem library.
	if (!filesystem::is_directory(ArgPath))
	{
		// Try to open it as a file now.
		FileTestDescriptor = _open(ArgPath, O_RDONLY);

		if (FileTestDescriptor < 1)
			return -1; // Path is invalid.
		else
		{
			_close(FileTestDescriptor);
			return 0;  // Path points to a file.
		}
	}

	// Check for index.htm and index.html
	if ( ( FileTestDescriptor = _open( FullPath( ArgPath, "index.html" ).c_str(), O_RDONLY ) ) > 0 )
	{ // Index.html found.
		_close ( FileTestDescriptor );
		strcat( ArgPath, "/index.html");
		return 0; 
	}
	if ( ( FileTestDescriptor = _open( FullPath( ArgPath, "index.htm" ).c_str(), O_RDONLY ) ) > 0 )
	{ // Index.htm found.
		_close ( FileTestDescriptor );
		strcat( ArgPath, "/index.htm");
		return 0;
	}

	if ( ArgBuffer.empty() )
		ArgBuffer.reserve(10000);

	set<std::string> Filler;

	ArgBuffer = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" 
          content="width=device-width, 
                   initial-scale=1.0">
    <title>)";
	ArgBuffer += "DFileServer [" + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "] " + ArgVirtualPath;
	ArgBuffer += R"(</title>
<style>
    body {
        font-family: "Segoe UI", Arial, sans-serif;
        background-color: #f3f3f3;
        margin: 0;
        padding: 0;
    }

    .window {
        width: 80%;
        margin: 50px auto;
        border-radius: 8px; /* Rounded corners */
        border: 1px solid #ddd;
        box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        background-color: #fff;
        overflow: hidden;
    }

    .window-header {
        background-color: #0078d7; /* Windows accent blue */
        color: #fff;
        padding: 12px;
        font-weight: 500;
        display: flex;
        justify-content: space-between;
        align-items: center;
        border-top-left-radius: 8px;
        border-top-right-radius: 8px;
    }

    .window-header .title {
        font-size: 16px;
    }

    .window-location {
        padding: 10px;
        background-color: #fafafa;
        border-bottom: 1px solid #ddd;
        font-size: 14px;
        color: #555;
    }

    /* Breadcrumb style for path */
    .window-location span {
        color: #0078d7;
        cursor: pointer;
        margin-right: 5px;
    }
    .window-location span:hover {
        text-decoration: underline;
    }
    .window-location .separator {
        color: #999;
        margin-right: 5px;
    }

    .window-content {
        padding: 20px;
    }

    .file-grid {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
        gap: 15px;
    }

    .file-item {
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 15px;
        text-align: left;
        background-color: #fff;
        box-shadow: 0 2px 6px rgba(0,0,0,0.05);
        transition: transform 0.1s ease, box-shadow 0.1s ease, background-color 0.1s ease;
    }

	.file-item a {
		display: block;        /* fills the parent */
		width: 100%;
		height: 100%;
		text-decoration: none; /* remove underline */
		color: inherit;        /* keep text color */
	}

    .file-item:hover {
        transform: scale(1.02);
        box-shadow: 0 4px 10px rgba(0,0,0,0.1);
        background-color: #f9f9f9;
    }

    .file-name {
        font-weight: 500;
        margin-bottom: 5px;
        display: flex;
        align-items: center;
        gap: 8px;
    }

    .file-size {
        font-size: 12px;
        color: #777;
    }

    /* Folder styling */
    .file-item.folder {
        background-color: #f0f6ff;
        border: 1px solid #cce0ff;
    }
</style>
</head>
<body>
    <div class="window">
        <div class="window-header">
            <div class="title">)";
	ArgBuffer += "DFileServer [" + std::to_string(Version::MAJORVERSION) + "." + std::to_string(Version::MINORVERSION) + "." + std::to_string(Version::PATCHVERSION) + "]";
	ArgBuffer += R"(
	</div>
            <div class="right-section">
				<div class="version">)";
	ArgBuffer += Version::VERSIONTITLE;
	ArgBuffer += R"(
				</div>
			</div>
        </div>
        <div class="window-location">)";
	ArgBuffer += ArgVirtualPath;
	ArgBuffer += R"(	            
        </div>
        <div class="window-content">
            <div class="file-grid" id="file-grid"></div>
        </div>
    </div>

    <script>
        const fileGrid = document.getElementById('file-grid');

        const items = [
)";

	ArgBuffer += InsertIndexTable(&ArgVirtualPath, std::string(ArgPath), Filler);

	ArgBuffer += R"(
        ];

        items.forEach(item => {
            const fileItem = document.createElement('div');
            fileItem.className = 'file-item';
            if (item.type === 'folder') {
                fileItem.classList.add('folder');
            }
            const link = document.createElement('a');
            link.href = item.src;
            const fileName = document.createElement('div');
            fileName.className = 'file-name';
            fileName.textContent = item.name;
            link.appendChild(fileName);
            if (item.type === 'file') {
                const fileSize = document.createElement('div');
                fileSize.className = 'file-size';
                fileSize.textContent = item.size;
                link.appendChild(fileSize);
            }
            fileItem.appendChild(link);
            fileGrid.appendChild(fileItem);
        });
    </script>
</body>
</html>
)";

	return 1;
}
