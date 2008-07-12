// Copyright (C) 2003-2008 Dolphin Project.

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
/////////////////////////////////////////////////////////////////////////////////////////////////////
// M O D U L E  B E G I N ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "IniFile.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I M P L E M E N T A T I O N //////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// ________________________________________________________________________________________ __________
// constructor
//
IniFile::IniFile(void)
{
}

// ________________________________________________________________________________________ __________
// destructor
//
IniFile::~IniFile(void)
{
}

// ________________________________________________________________________________________ __________
// SetFile
//
void 
IniFile::SetFile(const TCHAR* _filename)
{
	if (_filename)
	{
		char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
		char fname[_MAX_FNAME],ext[_MAX_EXT];

		GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		_makepath( filename, drive, dir, _filename, ".ini");
	}
}

// ________________________________________________________________________________________ __________
// SetSection
//
void 
IniFile::SetSection(const TCHAR *_section)
{
	_tcscpy(section,_section);
}

// ________________________________________________________________________________________ __________
// ReadInt
//
int  
IniFile::ReadInt(const TCHAR *key, int def)
{
	return GetPrivateProfileInt(section, key, def, filename);
}

// ________________________________________________________________________________________ __________
// WriteInt
//
void 
IniFile::WriteInt(const TCHAR *key, int value)
{
	char temp[256];
	WritePrivateProfileString(section, key, _itoa(value,temp,10), filename);	
}

// ________________________________________________________________________________________ __________
// ReadBool
//
bool 
IniFile::ReadBool(const TCHAR *key, bool def)
{
	return ReadInt(key,def?1:0) == 0 ? false : true;
}

// ________________________________________________________________________________________ __________
// WriteBool
//
void 
IniFile::WriteBool(const TCHAR *key, bool value)
{
	WriteInt(key,value?1:0);
}

// ________________________________________________________________________________________ __________
// ReadString
//
void 
IniFile::ReadString(const TCHAR *key, const TCHAR *def, TCHAR *out, int size)
{
	GetPrivateProfileString(section, key, def, out, size, filename);	
}

// ________________________________________________________________________________________ __________
// WriteString
//
void 
IniFile::WriteString(const TCHAR *key, const TCHAR *value)
{
	WritePrivateProfileString(section, key, value, filename);	
}

// ________________________________________________________________________________________ __________
// ReadStringList
//
void 
IniFile::ReadStringList(const TCHAR *key, std::vector<std::string> &list)
{
	int count = ReadInt(key);
	for (int i=0; i<count; i++)
	{
		char temp[256], temp2[256]; 
		sprintf(temp,"%s%i",key,i);
		ReadString(temp,"",temp2,256);
		list.push_back(std::string(temp2));
	}
}

// ________________________________________________________________________________________ __________
// WriteStringList
//
void 
IniFile::WriteStringList(const TCHAR *key, std::vector<std::string> &list)
{
	WriteInt(key,(int)list.size());
	int i=0;
	for (std::vector<std::string>::iterator iter = list.begin(); iter!=list.end(); iter++)
	{
		char temp[256];
		sprintf(temp,"%s%i",key,i);
		WriteString(temp,iter->c_str());
		i++;
	}
}