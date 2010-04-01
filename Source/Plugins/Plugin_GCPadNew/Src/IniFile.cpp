
#include "IniFile.h"

//
//		TrimChars
//
// trim whitespace, or any, chars from both ends
//
template <typename S>
std::string TrimChars( const std::string& str, const S space )
{
	const size_t start = str.find_first_not_of( space );

	if ( str.npos == start )
		return "";

	return str.substr( start, str.find_last_not_of( space ) - start + 1 );
}

//
//		IniSection :: Set
//
// set key's value if it doesn't match the default
// otherwise remove the key from the section if it exists
//
void IniSection::Set( const std::string& key, const std::string& val, const std::string& def )
{
	if ( val != def )
		operator[](key) = val;
	else
	{
		iterator f = find(key);
		if ( f != end() )
			erase( f );	
	}
}

//
//		IniSection :: Get
//
// return a key's value if it exists
// otherwise return the default
//
std::string IniSection::Get( const std::string& key, const std::string& def )
{
	const const_iterator f = find(key);
	if ( f != end() )
		if ( false == f->second.empty() )
			return f->second;
	return def;
}

//
//		IniFile :: Save
//
// save a file
//
void IniFile::Save( std::ostream& file )
{
	const_iterator i = begin(),
		e = end();
	for ( ; i != e; ++i )
	{
		// skip a line at new sections
		file << "\n[" << i->first << "]\n";
		Section::const_iterator si = i->second.begin(),
			se = i->second.end();
		for ( ; si != se; ++si )
			file << si->first << " = " << si->second << '\n';
	}
}

//
//		IniFile :: Load
//
// load a file
//
void IniFile::Load( std::istream& file )
{
	const char* const space = "\t\r ";
	std::string line;
	// start off with an empty section
	Section* section = &(*this)[""];
	while ( std::getline( file, line ).good() ) // read a line
	{
		line = TrimChars(line,space);
		if ( line.size() )
		{
			switch ( line[0] )
			{
			// comment
			case '#' :
			case ';' :
				break;
			// section
			case '[' :
				// kinda odd trimming
				section = &(*this)[ TrimChars(line,"][\t\r ") ];
				break;
			// key/value
			default :
				{
					std::istringstream ss(line);
					std::string key; std::getline( ss, key, '=' );
					std::string val; std::getline( ss, val );
					(*section)[ TrimChars(key,space) ] = TrimChars(val,space);
					break;
				}
			}
		}
	}
	Clean();
}

//
//		IniFile :: Clean
//
// remove empty key/values and sections
// after trying to access ini sections/values, they are automatically allocated
// this deletes the empty stuff
//
void IniFile::Clean()
{
	iterator i = begin(),
		e = end();
	for ( ; i != e; )
	{
		Section::iterator si = i->second.begin(),
			se = i->second.end();
		for ( ; si != se; )
		{		
			if ( si->second.empty() )
				i->second.erase( si++ );
			else
				++si;
		}
		if ( i->second.empty() )
			erase( i++ );
		else
			++i;
	}
}
