
#ifndef _VERTEXMANAGERBASE_H
#define _VERTEXMANAGERBASE_H

class NativeVertexFormat;
class PointerWrap;

class VertexManager
{
private:
	// What are the actual values?
	static const u32 SMALLEST_POSSIBLE_VERTEX = 1;
	static const u32 LARGEST_POSSIBLE_VERTEX = 188;
	
	static const u32 MAX_PRIMITIVES_PER_COMMAND = (u16)-1;
	
public:
	// values from OGL backend
	//static const u32 MAXVBUFFERSIZE = 0x1FFFF;
	
	// values from DX9/11 backend
	static const u32 MAXVBUFFERSIZE = MAX_PRIMITIVES_PER_COMMAND * LARGEST_POSSIBLE_VERTEX;
	
	// We may convert triangle-fans to triangle-lists, almost 3x as many indices.
	// Watching for a full index buffer would probably be smarter than this calculation.
	static const u32 MAXIBUFFERSIZE = MAXVBUFFERSIZE * 3 / SMALLEST_POSSIBLE_VERTEX;
	//static const u32 MAXIBUFFERSIZE = MAX_PRIMITIVES_PER_COMMAND * 3;

	VertexManager();
	// needs to be virtual for DX11's dtor
	virtual ~VertexManager();

	static void AddVertices(int _primitive, u32 _numVertices);

	// TODO: protected?
	static u8 *s_pBaseBufferPointer;
	static u8 *s_pCurBufferPointer;
	static u8 *s_pEndBufferPointer;

	static int GetRemainingSize();
	static int GetRemainingVertices(int primitive);

	static void Flush();

	virtual ::NativeVertexFormat* CreateNativeVertexFormat() = 0;

	static u16* GetTriangleIndexBuffer() { return TIBuffer; }
	static u16* GetLineIndexBuffer() { return LIBuffer; }
	static u16* GetPointIndexBuffer() { return PIBuffer; }
	static u8* GetVertexBuffer() { return s_pBaseBufferPointer; }

	static void DoState(PointerWrap& p);
	virtual void CreateDeviceObjects(){};
	virtual void DestroyDeviceObjects(){};
protected:
	// TODO: make private after Flush() is merged
	static void ResetBuffer();

	static u16 *TIBuffer;
	static u16 *LIBuffer;
	static u16 *PIBuffer;

	static bool Flushed;

	virtual void vDoState(PointerWrap& p) { DoStateShared(p); }
	void DoStateShared(PointerWrap& p);

private:
	//virtual void Draw(u32 stride, bool alphapass) = 0;
	// temp
	virtual void vFlush() = 0;
	
	static u8 *LocalVBuffer;

};

extern VertexManager *g_vertex_manager;

#endif
