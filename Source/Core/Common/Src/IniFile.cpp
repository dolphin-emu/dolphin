// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
// see IniFile.h

#include "IniFile.h"

template <typename S>
void StripChars(std::string& str, const S space)
{
	const size_t start = str.find_first_not_of(space);

	if (str.npos == start)
		str.clear();
	else
		str = str.substr(start, str.find_last_not_of(space) - start + 1);
}

bool Section::Get(const std::string& key, std::string* const val, const std::string& def) const
{
	const const_iterator f = find(key);
	if (f != end())
	{
		*val = f->second;
		return true;
	}
	if (false == def.empty())
		*val = def;
	return false;
}

void Section::Set(const std::string& key, const std::string& val, const std::string& def)
{
	if (val != def)
		operator[](key) = val;
	else
	{
		iterator f = find(key);
		if (f != end())
			erase(f);
	}
}

bool IniFile::Save(const std::string& filename) const
{
	return Save(filename.c_str());
}

bool IniFile::Save(const char filename[]) const
{
	std::ofstream file;
	file.open(filename);
	if (file.is_open())
	{
		Save(file);
		file.close();
		return true;
	}
	else
		return false;
}

void IniFile::Save(std::ostream& file) const
{
	const_iterator
		si = begin(),
		se = end();
	for ( ; si != se; ++si )
	{
		// skip a line at new sections
		file << "\n[" << si->first << "]\n";
		si->second.Save(file);
	}
}

void Section::Save(std::ostream& file) const
{
	if (m_use_lines)	// this is used when GetLines or SetLines has been called
	{
		std::vector<std::string>::const_iterator
			i = m_lines.begin(),
			e = m_lines.end();
		for ( ; i!=e; ++i)
			file << *i << '\n';
	}
	else
	{
		Section::const_iterator
			vi = begin(),
			ve = end();
		for ( ; vi!=ve; ++vi)
		{
			file << vi->first << " = ";
			// if value has quotes or whitespace, surround it with quotes
			if (vi->second.find_first_of("\"\t ") != std::string::npos)
				file << '"' << vi->second << '"';
			else
				file << vi->second;
			file << '\n';
		}
	}
}

bool IniFile::Load(const std::string& filename)
{
	return Load(filename.c_str());
}

bool IniFile::Load(const char filename[])
{
	std::ifstream file;
	file.open(filename);
	if (file.is_open())
	{
		Load(file);
		file.close();
		return true;
	}
	else
		return false;
}

void IniFile::Load(std::istream& file)
{
	std::vector<std::string> lines;

	Section sectmp;
	Section* section = &sectmp;

	std::string line;
	while (std::getline(file, line)) // read a line
	{
		if (line.size())
		{
			switch (line[0])
			{
			// section
			case '[' :
				section->m_lines = lines;
				// kinda odd trimming
				StripChars(line, "][\t\r ");
				section = &(*this)[line];
				lines.clear();
				break;

			// key/value
			default :
				{
					std::istringstream ss(line);

					std::string key; std::getline(ss, key, '=');
					std::string val; std::getline(ss, val);
					
					StripChars(val, "\t\r ");
					// handle quote surrounded values
					if (val.length() > 1)
						if ('"' == val[0])
							val.assign(val.begin()+1, val.end()-1);

					StripChars(key, "\t\r ");
					(*section)[key] = val;
				}
				//break;	// no break

			// comment
			case '#' :
			case ';' :
				lines.push_back(line);
				break;
			}
		}
	}
	//Clean();
}

//
//		IniFile :: Clean
//
// remove empty key/values and sections
// after trying to access ini sections/values with the [] operator, they are automatically allocated
// this deletes the empty stuff
//
void IniFile::Clean()
{
	iterator
		i = begin(),
		e = end();
	for ( ; i != e; )
	{
		Section::iterator
			si = i->second.begin(),
			se = i->second.end();
		for ( ; si != se; )
		{		
			if (si->second.empty())
				i->second.erase( si++ );
			else
				++si;
		}
		if (i->second.empty() && i->second.m_lines.empty())
			erase( i++ );
		else
			++i;
	}
}

bool IniFile::Exists(const std::string& section) const
{
	return find(section) != end();
}

void IniFile::Delete(const std::string& section)
{
	const iterator f = find(section);
	if (end() != f)
		erase(f);
}

bool Section::Exists(const std::string& key) const
{
	return find(key) != end();
}

void Section::Delete(const std::string& key)
{
	const iterator f = find(key);
	if (end() != f)
		erase(f);
}

void Section::SetLines(const std::vector<std::string>& lines)
{
	m_lines = lines;
	m_use_lines = true;
}

void Section::GetLines(std::vector<std::string>& lines)
{
	lines = m_lines;
	m_use_lines = true;
}
