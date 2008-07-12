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

#include "ChunkFile.h"

namespace W32Util
{
	ChunkFile::ChunkFile(const TCHAR *filename, bool _read)
	{
		data=0;
		fastMode=false;
		if (file.Open(filename,_read ? FILE_READ : FILE_WRITE))
		{
			didFail=false;
		}
		else
		{
			didFail=true;
			return;
		} 

		int fSize = file.GetSize();

		fastMode = _read ? true : false;

		if (fastMode)
		{
			data = new char[fSize];
			file.Read(data,fSize);
			file.Close();
		}

		eof=fSize;
		numLevels=0;
		read=_read;
		pos=0;
		didFail=false;
	}

	ChunkFile::~ChunkFile()
	{
		if (fastMode && data)
			delete [] data;
		else
			file.Close();
	}


	int ChunkFile::ReadInt()
	{
		if (pos<eof)
		{
			/*
			int temp = *(int *)(data+pos);
			pos+=4;
			*/
			pos+=4;
			if (fastMode)
				return *(int *)(data+pos-4);
			else
				return file.ReadInt();
		}
		else
		{
			return 0;
		}
	}


	void ChunkFile::WriteInt(int i)
	{
		/*
		*(int *)(data+pos) = i;
		pos+=4;
		*/
		file.WriteInt(i);
		pos+=4;
	}

	//let's get into the business
	bool ChunkFile::Descend(unsigned int id)
	{
		id=flipID(id);
		if (read)
		{
			bool found = false;
			int startPos = pos;
			ChunkInfo temp = stack[numLevels];

			//save information to restore after the next Ascend
			stack[numLevels].parentStartLocation = pos;
			stack[numLevels].parentEOF = eof;


			int firstID = 0;
			//let's search through children..
			while(pos<eof)
			{
				stack[numLevels].ID = ReadInt();
				if (firstID == 0) firstID=stack[numLevels].ID|1;
				stack[numLevels].length = ReadInt();
				stack[numLevels].startLocation = pos;

				if (stack[numLevels].ID == id)
				{
					found = true;
					break;
				}
				else
				{
					SeekTo(pos + stack[numLevels].length); //try next block
				}
			} 

			//if we found nothing, return false so the caller can skip this
			if (!found)
			{
				/*
				pos = startPos;
				char temp1[5]; TCHAR temp2[5];
				temp1[4]=0; temp2[4]=0;
				*(int *)temp1 =id;
				TCHAR tempx[256];

				for (int i=0; i<4; i++) 
				temp2[i]=temp1[i];

				_stprintf(tempx,TEXT("Couldn't find chunk \"%s\" in file"),temp2);

				MessageBox(theApp->getHWND(),tempx,0,0);
				*/
				stack[numLevels]=temp;
				SeekTo(stack[numLevels].parentStartLocation);
				return false;
			}

			//descend into it
			//pos was set inside the loop above
			eof = stack[numLevels].startLocation + stack[numLevels].length;
			numLevels++;
			return true;
		}
		else
		{
			//write a chunk id, and prepare for filling in length later
			WriteInt(id);
			WriteInt(0); //will be filled in by Ascend
			stack[numLevels].startLocation=pos;
			numLevels++;
			return true;
		}
	}

	void ChunkFile::SeekTo(int _pos)
	{
		if (!fastMode)
			file.SeekBeg(_pos);
		pos=_pos;
	}

	//let's Ascend out
	void ChunkFile::Ascend()
	{
		if (read)
		{
			//Ascend, and restore information
			numLevels--;
			SeekTo(stack[numLevels].parentStartLocation);
			eof = stack[numLevels].parentEOF;
		}
		else 
		{
			numLevels--;
			//now fill in the written length automatically
			int posNow = pos;
			SeekTo(stack[numLevels].startLocation - 4);
			WriteInt(posNow-stack[numLevels].startLocation);
			SeekTo(posNow);
		}
	}

	//read a block
	void ChunkFile::ReadData(void *what, int count)
	{

		if (fastMode)
			memcpy(what,data+pos,count);
		else
			file.Read(what,count);

		pos+=count;
		char temp[4]; //discarded
		count &= 3;
		if (count)
		{
			count=4-count;
			if (!fastMode)
				file.Read(temp,count);
			pos+=count;
		}
	}

	//write a block
	void ChunkFile::WriteData(void *what, int count)
	{
		/*
		memcpy(data+pos,what,count);
		pos += count;
		*/
		file.Write(what,count);
		pos+=count;
		char temp[5]={0,0,0,0,0};
		count &= 3; 
		if (count)
		{
			count=4-count;
			file.Write(temp,count);
			pos+=count;
		}
	}

	/*
	void ChunkFile::WriteString(String str)
	{
	wchar_t *text;
	int len=str.length();
	#ifdef UNICODE
	text = str.getPointer();
	#else
	text=new wchar_t[len+1];
	str.toUnicode(text);
	#endif
	WriteInt(len);
	WriteData((char *)text,len*sizeof(wchar_t));
	#ifndef UNICODE
	delete [] text;
	#endif
	}


	String ChunkFile::readString()
	{
	int len=ReadInt();
	wchar_t *text = new wchar_t[len+1];
	ReadData((char *)text,len*sizeof(wchar_t));
	text[len]=0;
	#ifdef UNICODE
	String s(text);
	delete [] text;
	return s;
	#else
	String temp;
	temp.fromUnicode(text);
	delete [] text;
	return temp;
	#endif
	}
	*/

	int ChunkFile::GetCurrentChunkSize()
	{
		if (numLevels)
			return stack[numLevels-1].length;
		else
			return 0;
	}
}
