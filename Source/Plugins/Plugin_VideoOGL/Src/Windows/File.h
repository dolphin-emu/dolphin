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

#ifndef __LAMEFILE_H__
#define __LAMEFILE_H__

#include "stdafx.h"

namespace W32Util
{

	enum eFileMode
	{
		FILE_READ=5,
		FILE_WRITE=6,
		FILE_ERROR=0xff
	};

	class File  
	{
		HANDLE fileHandle;
		eFileMode mode;
		bool isOpen;
	public:
		File();
		virtual ~File();

		bool Open(const TCHAR *filename, eFileMode mode);
		void Adopt(HANDLE h) { fileHandle = h;}
		void Close();

		void WriteInt(int i);
		void WriteChar(char i);
		int  Write(void *data, int size);


		int  ReadInt();
		char ReadChar();
		int  Read(void *data, int size);

		int  WR(void *data, int size); //write or read depending on open mode
		bool MagicCookie(int cookie);

		int  GetSize();
		eFileMode GetMode() {return mode;}
		void SeekBeg(int pos)
		{
			if (isOpen)	SetFilePointer(fileHandle,pos,0,FILE_BEGIN);
		}
		void SeekEnd(int pos)
		{
			if (isOpen)	SetFilePointer(fileHandle,pos,0,FILE_END);
		}
		void SeekCurrent(int pos)
		{
			if (isOpen)	SetFilePointer(fileHandle,pos,0,FILE_CURRENT);
		}
	};

}


#endif //__LAMEFILE_H__