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
	virtual u8  Read8 (void) = NULL;
	virtual u16 Read16(void) = NULL;
	virtual u32 Read32(void) = NULL;	
};

// =================================================================================================
// CDataReader_Fifo
// =================================================================================================

class CDataReader_Fifo : public IDataReader
{
private:

public:
	CDataReader_Fifo(void);
		
	virtual u8 Read8(void);
	virtual u16 Read16(void);
	virtual u32 Read32(void);
};

// =================================================================================================
// CDataReader_Memory
// =================================================================================================

class CDataReader_Memory : public IDataReader
{
private:

//	u8* m_pMemory;
	u32 m_uReadAddress;

public:

	CDataReader_Memory(u32 _uAddress);

	u32 GetReadAddress(void);

	virtual u8 Read8(void);
	virtual u16 Read16(void);
	virtual u32 Read32(void);
};

extern IDataReader* g_pDataReader;

#endif
