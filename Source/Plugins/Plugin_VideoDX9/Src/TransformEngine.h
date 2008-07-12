#ifndef _TRANSFORMENGINE_H
#define _TRANSFORMENGINE_H

//T&L Engine
//as much work as possible will be delegated to vertex shaders later
//to take full advantage of current PC HW

#include "VertexHandler.h"	
#include "DecodedVArray.h"

class CTransformEngine
{
	static float* m_pPosMatrix;
	static float* m_pNormalMatrix;
	static float* m_pTexMatrix[8];
	static float* m_pTexPostMatrix[8];

public:
	static size_t SaveLoadState(char *ptr, bool save);
	static void TransformVertices(int _numVertices, 
		                          const DecodedVArray *varray, 
								  D3DVertex *vbuffer);

	static void SetPosNormalMatrix(float *p, float *n)
	{
		m_pPosMatrix = p;
		m_pNormalMatrix = n;
	}
	static void SetTexMatrix(int i, float *f)
	{
		m_pTexMatrix[i] = f;
	}
	static void SetTexPostMatrix(int i, float *f)
	{
		m_pTexPostMatrix[i] = f;
	}
};

#endif
