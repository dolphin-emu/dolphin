
#ifndef _VERTEXMANAGERBASE_H
#define _VERTEXMANAGERBASE_H

class NativeVertexFormat;
class PointerWrap;

class VertexManager
{
public:

	enum
	{
		// values from OGL backend
		//MAXVBUFFERSIZE = 0x1FFFF,
		//MAXIBUFFERSIZE = 0xFFFF,

		// values from DX9 backend
		//MAXVBUFFERSIZE = 0x50000,
		//MAXIBUFFERSIZE = 0xFFFF,

		// values from DX11 backend
		MAXVBUFFERSIZE = 0x50000,
		MAXIBUFFERSIZE = 0xFFFF,
	};

	VertexManager();
	virtual ~VertexManager();	// needs to be virtual for DX11's dtor

	static void AddVertices(int _primitive, int _numVertices);

	// TODO: protected?
	static u8 *s_pCurBufferPointer;
	static u8 *s_pBaseBufferPointer;

	static int GetRemainingSize();
	static int GetRemainingVertices(int primitive);

	static void Flush();

	virtual ::NativeVertexFormat* CreateNativeVertexFormat() = 0;

	static u16* GetTriangleIndexBuffer() { return TIBuffer; }
	static u16* GetLineIndexBuffer() { return LIBuffer; }
	static u16* GetPointIndexBuffer() { return PIBuffer; }
	static u8* GetVertexBuffer() { return LocalVBuffer; }

	static void DoState(PointerWrap& p);
	virtual void CreateDeviceObjects(){};
	virtual void DestroyDeviceObjects(){};
protected:
	// TODO: make private after Flush() is merged
	static void ResetBuffer();

	static u8 *LocalVBuffer;
	static u16 *TIBuffer;
	static u16 *LIBuffer;
	static u16 *PIBuffer;

	static bool Flushed;

	virtual void vDoState(PointerWrap& p) { DoStateShared(p); }
	void DoStateShared(PointerWrap& p);

private:
	static void AddIndices(int primitive, int numVertices);
	//virtual void Draw(u32 stride, bool alphapass) = 0;
	// temp
	virtual void vFlush() = 0;

};

extern VertexManager *g_vertex_manager;

#endif
