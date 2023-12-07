#pragma once
#include <vector>
#include <map>
#include <string>

namespace DFSInternet
{

	class HTMLDocument
	{
	private:
		std::vector<HTMLElement> HTMLElements;
	public:


		HTMLDocument();
		~HTMLDocument();
	};

	/* HTML Elements */

	class HTMLElement
	{
	public:
		std::vector<HTMLElement> childElements; // Unused in the base class, but can be used in derived classes
		std::map<std::string,std::string> attributes;

		std::string produceElement(void);
	};

	class HTMLHead : public HTMLElement
	{
	public:

		virtual std::string produceElement(void);
	};
	}