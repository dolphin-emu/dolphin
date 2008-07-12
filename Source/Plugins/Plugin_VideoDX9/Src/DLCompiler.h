#pragma once

#if 0

#include "D3DBase.h"
#include "DecodedVArray.h"
#include <map>


class CompiledDList
{
	u32 *data;
	int dataSize;
	int pass;
	int codeSize;
	u8  *code;

	struct Batch
	{
		DecodedVArray varray;
		LPDIRECT3DINDEXBUFFER9 ibuffer;
		int numDraws;
	};
	Batch *batches;
	int numBatches;

	u32 addr, size;
	bool Compile();
	bool Pass1();
	void Pass2();
	void Run();

public:
	CompiledDList(u32 _addr, u32 _size);
	~CompiledDList();
	bool Call();
	static void DrawHelperHelper(CompiledDList *dl, int vno, int prim);
};

class DListCache
{
	struct DLCacheEntry
	{
		CompiledDList *dlist;
		int frameCount;
		int pass;
		u32 size;
		DLCacheEntry()
		{
			pass=0;
			dlist=0;
			frameCount=0;
		}
		void Destroy()
		{
			if (dlist)
				delete dlist;
		}
	};

	typedef std::map<DWORD,DLCacheEntry> DLCache;

	static DLCache dlists;

public:
	static void Init();
	static void Cleanup();
	static void Shutdown();
	static void Call(u32 _addr, u32 _size);
};

#endif