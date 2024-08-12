/* CVS $Id: CPathResolver.hxx,v 1.2 2005/06/11 23:46:01 incubus Exp $ */

/** @file
 *  A simple virtual path resolver (Declarations).
 *
 *  @author  Markus Thiele
 *
 *  @version 0.3
 *  @date    2005-06-03
 *
 *  Copyright (c) 2005 Markus Thiele
 *
 *  This software is provided 'as-is', without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software in
 *     a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *  3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#pragma once

#include <fstream>

#include <string>
#include <list>
#include <map>



/* Symbolic constants for exceptions (type CPathResolver::exn_t). */
#define  EXN_IO        1
#define  EXN_MALFORMED 2
#define  EXN_ILLEGAL   3



/** Path tree node structure.
 *  This structure holds a node of the path tree. For internal use only!
 */
struct SPathNode {
	std::string                       Target;
	std::map<std::string, SPathNode*> Children;

	~SPathNode() {
		std::map<std::string, SPathNode*>::iterator i;
		for( i = Children.begin(); i != Children.end(); i++ )
			delete i->second;
	}
};



/** Path resolver class.
 *  This class provides functions to read a virtual path mapping from a
 *  configuration file and apply that mapping to path std::strings.
 */
class CPathResolver {

	public:

		/** Exception type; just in case it ever changes. */
		typedef  unsigned int  exn_t;

		/** Create empty resolver object. */
		CPathResolver();

		/** Add a virtual/real path pair to the mapping. */
		void Add( std::string Fake, std::string Real );

		/** Create path resolver object and load a config file. */
		CPathResolver( std::string ConfigFile );

		/** Resolve virtual path std::strings. */
		std::string operator()( std::string Path );

		~CPathResolver();

	private:

		/** Clean path std::strings. */
		std::string PathClean( std::string Path );

		/** Split path std::strings into individual parts. */
		std::list<std::string> PathExplode( std::string Path );

		SPathNode *mPaths;

}; /* end of CPathResolver */
