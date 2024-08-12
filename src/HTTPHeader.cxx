/*
** ($Header: /var/www/cvsroot/DFileServer/src/HTTPHeader.cxx,v 1.4.2.1 2005/09/30 05:15:19 incubus Exp $)
**
** Copyright 2005, 2024 Chris Laverdure
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
#include <iostream>
#include <sstream>

#include "HTTPHeader.hxx"

using namespace std;

void HTTPHeader::ImportHeader ( std::string ArgHTTPHeader )
{
	std::string Field, Value;
	std::stringstream stringStream( ArgHTTPHeader );

	// Grab the basics first.
	stringStream >> AccessType >> AccessPath >> AccessProtocol;	

	while ( 1 )
	{

		Field = "[FunkyTurkeyMonday]";

		stringStream >> Field;

		if ( Field == "[FunkyTurkeyMonday]" )
		{ // Last legitimate Value has been passed, break
			break;
		}

		getline ( stringStream, Value );

		// Remove the : at the end of the Field, and the space in front of the value.
		Field.replace ( Field.size() - 1, 1, "");
		Value.replace ( 0, 1, "" );

		// Add it to the map
		HeaderFields[Field] = Value;
	}

}

std::string HTTPHeader::ExportHeader ( void )
{
	std::string Buffer;
	std::string Linefeed = "\r\n";
	map<std::string, std::string>::iterator HTTPMap;

	// Put the Access information at the top.
	Buffer = AccessType + " " + AccessPath + " " + AccessProtocol + Linefeed;

	// Dump the map into the std::string.
	for ( HTTPMap = HeaderFields.begin();
		HTTPMap != HeaderFields.end();
			HTTPMap++)
	{
		Buffer += HTTPMap->first + ": " + HTTPMap->second + Linefeed;
	}

	Buffer += Linefeed;

	return Buffer;
}

std::string HTTPHeader::GetValue ( std::string ArgField )
{
	std::string Value = "";
	map<std::string, std::string>::iterator Result;

	// Find the field in the map
	Result = HeaderFields.find( ArgField );

	// Did the search turn up anything?
	if ( Result != HeaderFields.end() )
		Value = Result->second;

	return Value;
}

void HTTPHeader::SetValue ( std::string ArgField, std::string ArgValue )
{
	HeaderFields[ArgField] = ArgValue;
}

