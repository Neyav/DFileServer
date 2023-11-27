/* CVS $Id: CPathResolver.cxx,v 1.3 2005/06/11 23:47:22 incubus Exp $ */

/** @file
 *  A simple virtual path resolver (Definitions).
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



#include "CPathResolver.hxx"

using std::ifstream;
using std::ios;

using std::list;
using std::map;



/* Quick fix for the ..\.. security hole on Windows systems.
 * Return true if c is a path seperator in the current OS, else false.
 */
#ifdef _WINDOWS
	inline bool IS_SEP( char &c ) { return ( c == '/' || c == '\\' ); }
#else
	inline bool IS_SEP( char &c ) { return ( c == '/' ); }
#endif



/** Create empty resolver object. The mapping can be filled then with Add. */
CPathResolver::CPathResolver() {
	mPaths = new SPathNode();
}



/** Add a virtual/real path pair to the mapping.
 *
 *  Throws EXN_ILLEGAL if one of the paths is illegal.
 *
 *  @param  Fake  Virtual path to be mapped.
 *  @param  Real  Real path to be mapped to.
 */
void CPathResolver::Add( std::string Fake, std::string Real ) {

	list<std::string> Parts = PathExplode( Fake );

	SPathNode *Node = mPaths;
	for( list<std::string>::iterator i = Parts.begin(); i != Parts.end(); i++ ) {
		if( Node->Children[*i] == NULL ) {
			Node->Children[*i] = new SPathNode;
		}
		Node = Node->Children[*i];
	}
	Node->Target = Real;

} /* End of CPathResolver::Add. */



/** Create path resolver object and load a config file.
 *
 *  Throws EXN_IO if the configuration file cannot be opened.
 *  Throws EXN_MALFORMED if the configuration file is malformed.
 *  Throws EXN_ILLEGAL if any of the paths are illegal.
 *
 *  @param  ConfigFile  Full path to the configuration file to be used.
 *
 *  @todo  Much more stable config file reading. :)
 */
CPathResolver::CPathResolver( std::string ConfigFile ) {

	mPaths = new SPathNode();

	ifstream Config( ConfigFile.c_str(), ios::in );
	if( !Config.is_open() ) throw( (exn_t)EXN_IO );

	while( !Config.eof() ) {
		std::string Fake, Real;
		Config >> Fake >> Real;
		Add( Fake, Real );
	}

} /* End of CPathResolver::CPathResolver. */



/** Resolve virtual path std::strings.
 *
 *  Resolves path std::strings based on the currently stored mapping.
 *  Throws EXN_ILLEGAL if there is no valid resolution for the path.
 *
 *  @param   Path  Path to be resolved.
 *  @return  Resolved path.
 */
std::string CPathResolver::operator()( std::string Path ) {

	list<std::string> Parts = PathExplode( PathClean( Path ) );

	list<std::string>::iterator iPart = Parts.begin();
	SPathNode *Node = mPaths;

	std::string NewPath;
	NewPath.reserve( 4096 );

	while( iPart != Parts.end() && Node->Children[*iPart] != NULL ) {
		Node = Node->Children[*iPart];
		iPart++;
	} 

	if( Node->Target != "" ) {
		NewPath += Node->Target;
	} else {
		throw( (exn_t)EXN_ILLEGAL );
	}

	while( iPart != Parts.end() ) {
		NewPath += "/" + *iPart;
		iPart++;
	}

	return NewPath;
	
} /* End of CPathResolver::operator(). */
 


/** Destructor. */
CPathResolver::~CPathResolver() {
	delete mPaths;
}



/** Clean path std::strings.
 *
 *  Removes @c . and @c .. from path std::strings (properly evaluating them).
 *  All paths are required to use @c / for directory seperation.
 *  Throws @c EXN_ILLEGAL if the resulting path is empty or negative.
 *
 *  @param   Path  Path std::string to be cleaned.
 *  @return  cleaned path std::string.
 */
std::string CPathResolver::PathClean( std::string Path ) {

	list<std::string> Parts = PathExplode( Path );

	if( Parts.empty() ) return "/";

	std::string NewPath;
	NewPath.reserve( 4096 );
	for( list<std::string>::iterator i = Parts.begin(); i != Parts.end(); i++ ) {
		NewPath += "/" + *i;
	}

	return NewPath;

} /* End of CPathResolver::PathClean. */



/** Split path std::strings into individual parts.
 *
 *  Splits paths into parts, automatically resolving "." and "..".
 *  All paths are required to use @c / for directory seperation.
 *  Throws @c EXN_ILLEGAL if the resulting path is empty or negative.
 *
 *  @param   Path  Path std::string to be split up.
 *  @return  List of path parts.
 */
list<std::string> CPathResolver::PathExplode( std::string Path ) {

	list<std::string> Parts;

	std::string Current;
	Current.reserve( 256 );
	for( unsigned int i = 0; i < Path.length(); i++ ) {
		if( !IS_SEP( Path[i] ) ) Current += Path[i];
		if( IS_SEP( Path[i] ) || i == Path.length() - 1 ) {
			if( Current == ".." ) {
				if( Parts.empty() ) throw( (exn_t)EXN_ILLEGAL );
				Parts.pop_back();
			} else if( !Current.empty() && Current != "." ) {
				Parts.push_back( Current );
			}
			Current.clear();
		}
	}

	return Parts;

} /* End of CPathResolver::PathExplode. */

