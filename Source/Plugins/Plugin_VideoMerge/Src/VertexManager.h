
#ifndef _VERTExMANAGER_H_
#define _VERTExMANAGER_H_

#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"
#include "BPMemory.h"

enum
{
	MAXVBUFFERSIZE = 0x50000,
	MAXIBUFFERSIZE = 0x10000,
};

using VertexManager::s_pBaseBufferPointer;
using VertexManager::s_pCurBufferPointer;

extern NativeVertexFormat *g_nativeVertexFmt;

class VertexManagerBase
{
public:
	VertexManagerBase();
	virtual ~VertexManagerBase();

	static void Flush();
	virtual void Draw(u32 stride, bool alphapass) = 0;

	virtual NativeVertexFormat* CreateNativeVertexFormat() = 0;

	static void AddIndices(int _primitive, int _numVertices);
	static int GetRemainingSize();
	static int GetRemainingVertices(int primitive);
	static void AddVertices(int _primitive, int _numVertices);
	static void ResetBuffer();

protected:
	static u16* TIBuffer;
	static u16* LIBuffer;
	static u16* PIBuffer;
	static bool Flushed;

	static int lastPrimitive;
	static u8* LocalVBuffer;
};

#endif
