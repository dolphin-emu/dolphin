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

#pragma once

#include "stdafx.h"

//TO REMEMBER WHEN USING:

//EITHER a chunk contains ONLY data
//OR it contains ONLY other chunks
//otherwise the scheme breaks...
#include "File.h"

namespace W32Util
{
	inline unsigned int flipID(unsigned int id)
	{
		return ((id>>24)&0xFF) | ((id>>8)&0xFF00) | ((id<<8)&0xFF0000) | ((id<<24)&0xFF000000);
	}

	class ChunkFile
	{
		File file;
		struct ChunkInfo
		{
			int startLocation;
			int parentStartLocation;
			int parentEOF;
			unsigned int ID;
			int length;
		};
		ChunkInfo stack[8];
		int numLevels;

		char *data;
		int pos,eof;
		bool fastMode;
		bool read;
		bool didFail;

		void SeekTo(int _pos);
		int GetPos() {return pos;}
	public:
		ChunkFile(const TCHAR *filename, bool _read);
		~ChunkFile();

		bool Descend(unsigned int id);
		void Ascend();

		int  ReadInt();
		void ReadInt(int &i) {i = ReadInt();}
		void ReadData(void *data, int count);
//		String ReadString();

		void WriteInt(int i);
		//void WriteString(String str);
		void WriteData(void *data, int count);

		int GetCurrentChunkSize();
		bool Failed() {return didFail;}
	};

}
