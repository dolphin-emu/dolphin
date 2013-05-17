
#ifndef _VERTEXMANAGERBASE_H
#define _VERTEXMANAGERBASE_H

#include "Common.h"
#include <vector>

class NativeVertexFormat;
class PointerWrap;

class VertexManager
{
private:
	static const u32 SMALLEST_POSSIBLE_VERTEX = sizeof(float)*3;                 // 3 pos
	static const u32 LARGEST_POSSIBLE_VERTEX = sizeof(float)*45 + sizeof(u32)*2; // 3 pos, 3*3 normal, 2*u32 color, 8*4 tex, 1 posMat 
	
	static const u32 MAX_PRIMITIVES_PER_COMMAND = (u16)-1;
	
public:
	static const u32 MAXVBUFFERSIZE = ROUND_UP_POW2 (MAX_PRIMITIVES_PER_COMMAND * LARGEST_POSSIBLE_VERTEX);
	
	// We may convert triangle-fans to triangle-lists, almost 3x as many indices.
	static const u32 MAXIBUFFERSIZE = ROUND_UP_POW2 (MAX_PRIMITIVES_PER_COMMAND * 3);

	VertexManager();
	// needs to be virtual for DX11's dtor
	virtual ~VertexManager();

	static void AddVertices(int _primitive, u32 _numVertices);

	static u8 *s_pCurBufferPointer;
	static u8 *s_pBaseBufferPointer;
	static u8 *s_pEndBufferPointer;

	static u32 GetRemainingSize();
	static void PrepareForAdditionalData(int primitive, u32 count, u32 stride);
	static u32 GetRemainingIndices(int primitive);

	static void Flush();

	virtual ::NativeVertexFormat* CreateNativeVertexFormat() = 0;

	static void DoState(PointerWrap& p);
	virtual void CreateDeviceObjects(){};
	virtual void DestroyDeviceObjects(){};
	
protected:
	u16* GetTriangleIndexBuffer() { return &TIBuffer[0]; }
	u16* GetLineIndexBuffer() { return &LIBuffer[0]; }
	u16* GetPointIndexBuffer() { return &PIBuffer[0]; }
	u8* GetVertexBuffer() { return &s_pBaseBufferPointer[0]; }

	virtual void vDoState(PointerWrap& p) { DoStateShared(p); }
	void DoStateShared(PointerWrap& p);

private:
	bool IsFlushed() const;
	
	void ResetBuffer();
	
	//virtual void Draw(u32 stride, bool alphapass) = 0;
	// temp
	virtual void vFlush() = 0;
	
	std::vector<u8> LocalVBuffer;
	std::vector<u16> TIBuffer;
	std::vector<u16> LIBuffer;
	std::vector<u16> PIBuffer;
};

extern VertexManager *g_vertex_manager;

#endif
