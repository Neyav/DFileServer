#include "HTMLEncoding.hxx"

namespace DFSInternet
{

	HTMLDocument::HTMLDocument()
	{

	}

	HTMLDocument::~HTMLDocument()
	{

	}

	std::string HTMLElement::produceElement(void)
	{
		return "";
	}

	std::string HTMLHead::produceElement(void)
	{
		std::string Elements;

		Elements += "<head";

		if (attributes.size() > 0)
		{
			Elements += " ";
			for (auto& Attribute : attributes)
			{
				Elements += Attribute.first + "=\"" + Attribute.second + "\" ";
			}
		}

		Elements += ">\n";

		for (auto& Element : childElements)
		{
			Elements += Element.produceElement();
		}
		Elements += "</head>\n";
	}
}