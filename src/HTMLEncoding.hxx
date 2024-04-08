#pragma once
#include <vector>
#include <map>
#include <string>

namespace DFSInternet
{

	class HTMLHeader
	{
	private:
	public:
		std::string Doctype;
		std::string DocTitle;

		HTMLHeader();
		~HTMLHeader();
	};

	class HTMLDocument
	{
	private:
		std::string htmlText;
	public:
		
		HTMLDocument();
		~HTMLDocument();
	};

	}