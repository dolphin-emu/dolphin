#ifndef __LAMEFILE_H__
#define __LAMEFILE_H__

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

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