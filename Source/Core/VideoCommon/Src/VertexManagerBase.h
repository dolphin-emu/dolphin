
#ifndef _VERTEXMANAGERBASE_H
#define _VERTEXMANAGERBASE_H

class VertexManager
{
public:

	enum
	{
		// values from OGL plugin
		//MAXVBUFFERSIZE = 0x1FFFF,
		//MAXIBUFFERSIZE = 0xFFFF,

		// values from DX9 plugin
		//MAXVBUFFERSIZE = 0x50000,
		//MAXIBUFFERSIZE = 0xFFFF,

		// values from DX11 plugin
		MAXVBUFFERSIZE = 0x50000,
		MAXIBUFFERSIZE = 0x10000,
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

protected:
	// TODO: make private after Flush() is merged
	static void ResetBuffer();

	static u8 *LocalVBuffer;
	static u16 *TIBuffer;
	static u16 *LIBuffer;
	static u16 *PIBuffer;

	static bool Flushed;

private:
	static void AddIndices(int primitive, int numVertices);
	//virtual void Draw(u32 stride, bool alphapass) = 0;
	// temp
	virtual void vFlush() = 0;

};

extern VertexManager *g_vertex_manager;

#endif
