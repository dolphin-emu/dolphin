#pragma once

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
