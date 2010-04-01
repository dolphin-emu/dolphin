#ifndef _INIFILE_H_
#define _INIFILE_H_

#include <fstream>
#include <map>
#include <string>
#include <sstream>

//
//		IniFile
//
class IniSection : public std::map< std::string, std::string >
{
public:
	void Set( const std::string& key, const std::string& val, const std::string& def = "" );
	std::string Get( const std::string& key, const std::string& def = "" );

	template <typename V, typename D>
	void Set( const std::string& key, const V& val, const D& def = 0 )
	{
		if ( val != def )
		{
			std::ostringstream ss;
			ss << long(val);
			operator[](key) = ss.str();
		}
		else
		{
			iterator f = find(key);
			if ( f != end() )
				erase( f );	
		}
	}
	template <typename V>
	V Get( const std::string& key, const V& def = 0 )
	{
		const const_iterator f = find(key);
		if ( f != end() )
		if ( false == f->second.empty() )
		{
			std::istringstream ss(f->second);
			int val;
			ss >> val;
			return V(val);
		}
		return def;
	}
};

class IniFile : public std::map< std::string, IniSection >
{
public:
	typedef IniSection Section;

	void Clean();
	void Save( std::ostream& file );
	void Load( std::istream& file );
};


#endif
