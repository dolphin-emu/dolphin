
#pragma once

#include "CPStructs.h"
#include "VertexLoader.h"
#include "DecodedVArray.h"

extern float shiftLookup[32];

struct UV
{
	float u,v,w;
};

struct D3DVertex {
	Vec3 pos;
	Vec3 normal;
	u32 colors[2];
	UV uv[8];
};

enum Collection
{
	C_NOTHING=0,
	C_TRIANGLES=1,
	C_LINES=2,
	C_POINTS=3
};
extern const Collection collectionTypeLUT[8];

class CVertexHandler
{
private:
	static Collection collection;
	// Pipeline

	static void PrepareRender();
	static void AddIndices(int _primitive, int _numVertices);

public:
	static void Init();
	static void Shutdown();

	static void BeginFrame();
	
	static void CreateDeviceObjects();
	static void DestroyDeviceObjects();

	static void DrawVertices(int _primitive, int _numVertices, const DecodedVArray *varray);
	static void Flush();
};
