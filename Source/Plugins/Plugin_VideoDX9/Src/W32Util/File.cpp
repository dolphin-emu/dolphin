#include <stdio.h>
#include <stdlib.h>

#include "File.h"


namespace W32Util
{

	File::File()
	{
		fileHandle = INVALID_HANDLE_VALUE;
		isOpen=false;
	}

	File::~File()
	{

	}


	bool File::Open(const TCHAR *filename, eFileMode _mode)
	{
		mode = _mode;
		//it's time to open the file 
		fileHandle = CreateFile(filename, 
			mode==FILE_READ ? GENERIC_READ : GENERIC_WRITE,  //open mode
			mode == FILE_READ ? FILE_SHARE_READ : NULL,  //sharemode
			NULL, //security
			mode==FILE_READ ? OPEN_EXISTING : CREATE_ALWAYS, //create mode
			FILE_ATTRIBUTE_NORMAL,  //atrributes
			NULL); //template

		if (fileHandle == INVALID_HANDLE_VALUE) 
			isOpen=false;
		else
			isOpen=true;

		return isOpen;
	}


	void File::Close()
	{
		if (isOpen)
		{
			//close the file and reset variables
			CloseHandle(fileHandle); 
			fileHandle=INVALID_HANDLE_VALUE;
			isOpen=false;
		}
	}

	int File::GetSize()
	{
		if (!isOpen) //of course
			return 0;
		else
			return GetFileSize(fileHandle,0);
	}

	int File::Write(void *data, int size) //let's do some writing
	{
		if (isOpen)
		{
			DWORD written;
			WriteFile(fileHandle, data, size, &written,0);
			return written; //we return the number of bytes that actually got written
		}
		else
		{
			return 0;
		}
	}

	int File::Read(void *data, int size) 
	{
		if (isOpen)
		{
			DWORD wasRead;
			ReadFile(fileHandle, data, size, &wasRead,0);
			return wasRead; //we return the number of bytes that actually was read
		}
		else
		{
			return 0;
		}
	}

	int File::WR(void *data, int size)
	{
		if (mode==FILE_READ)
			return Read(data,size);
		else
			return Write(data,size);
	}
	bool File::MagicCookie(int cookie)
	{
		if (mode==FILE_READ)
		{
			if (ReadInt()!=cookie)
			{
				char mojs[5],temp[256];
				mojs[4]=0;
				*(int*)mojs=cookie;
				sprintf(temp,"W32Util::File: Magic Cookie %s is bad!",mojs);
				MessageBox(0,temp,"Error reading file",MB_ICONERROR);
				return false;
			}
			else
				return true;
		}
		else if (mode==FILE_WRITE)
		{
			WriteInt(cookie);
			return true;
		}
		return false;
	}

	int File::ReadInt()
	{
		int temp;
		if (Read(&temp, sizeof(int)))
			return temp;
		else
			return 0;
	}

	void File::WriteInt(int i)
	{
		Write(&i,sizeof(int));
	}

	char File::ReadChar()
	{
		char temp;
		if (Read(&temp, sizeof(char)))
			return temp;
		else
			return 0;
	}

	void File::WriteChar(char i)
	{
		Write(&i,sizeof(char));
	}
}