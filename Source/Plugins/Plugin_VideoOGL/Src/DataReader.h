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

#ifndef _DATAREADER_H
#define _DATAREADER_H

// =================================================================================================
// IDataReader
// =================================================================================================

class IDataReader
{
protected:
    const char *m_szName;

public:	
    virtual void Skip(u32) = 0;
    virtual u8  Read8 () = 0;
    virtual u16 Read16() = 0;
    virtual u32 Read32() = 0;

	virtual int GetPosition() = 0; // return values can be anything, as long as relative distances are correct
};

// =================================================================================================
// CDataReader_Fifo
// =================================================================================================

class CDataReader_Fifo : public IDataReader
{
private:

public:
    CDataReader_Fifo();
        
    virtual void Skip(u32);
    virtual u8 Read8();
    virtual u16 Read16();
    virtual u32 Read32();
	virtual int GetPosition();
};

// =================================================================================================
// CDataReader_Memory
// =================================================================================================

class CDataReader_Memory : public IDataReader
{
private:

    //u8* m_pMemory;
    u32 m_uReadAddress;

public:

    CDataReader_Memory(u32 _uAddress);

    u32 GetReadAddress();

    virtual void Skip(u32);
    virtual u8 Read8();
    virtual u16 Read16();
    virtual u32 Read32();
	virtual int GetPosition();
};

extern IDataReader* g_pDataReader;

#endif
