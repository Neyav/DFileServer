/*
** ($Header: /var/www/cvsroot/DFileServer/src/MimeTypes.cxx,v 1.2 2005/06/14 07:09:57 incubus Exp $)
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

#include "MimeTypes.hxx"

#define MIMETYPES 52

// A list of the mimetypes... I tried organizing them so that the most common ones will be hit first.
mimetype_t MimeTypes[MIMETYPES] = {	
				// Plain Text
				{".txt", "text/plain"},
				{".asc", "text/plain"},
				// Parsed formats
				{".html", "text/html"},
				{".htm", "text/html"},
				{".rtx", "text/richtext"},
				{".rtf", "text/rtf"},
				{".css", "text/css"},
				// Images
				{".bmp", "image/bmp"},
				{".cgm", "image/cgm"},
				{".gif", "image/gif"},
				{".ief", "image/ief"},
				{".jpg", "image/jpeg"},
				{".jpeg", "image/jpeg"},
				{".jpe", "image/jpeg"},
				{".png", "image/png"},
				{".svg", "image/svg"},
				{".tiff", "image/tiff"},
				{".tif", "image/tiff"},
				{".ico", "image/x-icon"},
				{".xpm", "image/x-xpixmap"},
				{".xwd", "image/x-xwindowdump"},
				// Audio
				{".au", "audio/basic"},
				{".snd", "audio/basic"},
				{".mid", "audio/midi"},
				{".midi", "audio/midi"},
				{".kar", "audio/midi"},
				{".mp3", "audio/mpeg"},
				{".mp2", "audio/mpeg"},
				{".mpga", "audio/mpeg"},
				{".aif", "audio/x-aiff"},
				{".aiff", "audio/x-aiff"},
				{".aifc", "audio/x-aiff"},
				{".m3u", "audio/x-mpegurl"},
				{".ram", "audio/x-pn-realaudio"},
				{".rm", "audio/x-pn-realaudio"},
				{".ra", "audio/x-realaudio"},
				{".wav", "audio/x-wav"},
				// Videos
				{".mpeg", "video/mpeg"},
				{".mpg", "video/mpeg"},
				{".mpe", "video/mpeg"},
				{".qt", "video/quicktime"},
				{".mov", "video/quicktime"},
				{".mxu", "video/vnd.mpegurl"},
				{".avi", "video/x-msvideo"},
				{".movie", "video/x-sgi-movie"},
				// Applications
				{".tar", "application/x-tar"},
				{".tgz", "application/x-tar"},
				{".zip", "application/zip"},
				{".swf", "application/x-shockwave-flash"},
				{".doc", "application/msword"},
				{".pdf", "application/pdf"},
				{".ogg", "application/ogg"},
			};
			
char *ReturnMimeType ( char *ArgFileName )
{
	static char mimetype[40];
	char *CharPointer;

	// Set the pointer to the end of the string.
	CharPointer = &ArgFileName[strlen(ArgFileName) - 1];

	// Go backwards until you find the last period, be sure not to go beyond the start of our first variable.
	while ( CharPointer[0] != '.' && &CharPointer[0] != &ArgFileName[0] )
		CharPointer--;

	// Now we have our extension. Let's find out what mimetype it is.

	// Default mimetype.
	strncpy (mimetype, "text/plain", sizeof(mimetype));

	for (int GenericIterator = 0; GenericIterator < MIMETYPES; GenericIterator++)
	{
		// We've got a match.
		if ( strcasecmp(CharPointer, MimeTypes[GenericIterator].extension) == 0 )
		{
			strncpy(mimetype, MimeTypes[GenericIterator].mimetype, sizeof(mimetype) );
			break;
		}
	}

	return mimetype;
}
