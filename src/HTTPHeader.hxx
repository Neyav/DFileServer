#pragma once

#include <map>
#include <string>

class HTTPHeader
{
	private:

	std::map<std::string, std::string> 	HeaderFields;

	public:

	std::string 				AccessType;
	std::string 				AccessPath;
	std::string 				AccessProtocol;

	void ImportHeader 	( std::string );
	std::string ExportHeader( void );
	std::string GetValue 	( std::string );
	void SetValue		( std::string, std::string );
};
