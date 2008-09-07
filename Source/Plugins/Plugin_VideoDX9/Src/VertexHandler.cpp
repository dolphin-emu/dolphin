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

#include "D3DBase.h"

#include "Common.h"
#include "Globals.h"
#include "VertexHandler.h"
#include "OpcodeDecoding.h"
#include "TransformEngine.h"
#include "IndexGenerator.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "ShaderManager.h"
#include "Utils.h"

using namespace D3D;

//TODO(ector): remove and calculate inline?
float shiftLookup[32];

IndexGenerator indexGen;
Collection CVertexHandler::collection;

LPDIRECT3DVERTEXDECLARATION9 vDecl;

D3DVertex *fakeVBuffer;
u16 *fakeIBuffer;

#define MAXVBUFFERSIZE 65536*3
#define MAXIBUFFERSIZE 65536*3

const D3DVERTEXELEMENT9 decl[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
    { 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    { 0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },
	{ 0, 32+12*0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 32+12*1, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
	{ 0, 32+12*2, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
	{ 0, 32+12*3, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },
	{ 0, 32+12*4, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4 },
	{ 0, 32+12*5, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 5 },
	{ 0, 32+12*6, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 6 },
	{ 0, 32+12*7, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 7 },
    D3DDECL_END()
};

void CVertexHandler::Init()
{
	collection = C_NOTHING;

	for (int i = 0; i < 31; i++)
		shiftLookup[i] = 1.0f / float(1 << i);

	fakeVBuffer = new D3DVertex[65536];
	fakeIBuffer = new u16[65536];
	CreateDeviceObjects();
}


void CVertexHandler::Shutdown()
{
	DestroyDeviceObjects();
	delete [] fakeVBuffer;
	delete [] fakeIBuffer;
}

void CVertexHandler::CreateDeviceObjects()
{
	HRESULT hr;
	if( FAILED( hr = D3D::dev->CreateVertexDeclaration( decl, &vDecl) ) )
    {
		MessageBox(0,"Failed to create vertex declaration","sdfsd",0);
        return;
    }
}

void CVertexHandler::BeginFrame()
{
	D3D::dev->SetVertexDeclaration(vDecl);
	//D3D::dev->SetStreamSource(0,vBuffer,0,sizeof(D3DVertex));
}

void CVertexHandler::DestroyDeviceObjects()
{
	if (vDecl)
		vDecl->Release();
	vDecl=0;
}


void CVertexHandler::AddIndices(int _primitive, int _numVertices)
{
	switch(_primitive) {
	case GX_DRAW_QUADS:          indexGen.AddQuads(_numVertices); return;    
	case GX_DRAW_TRIANGLES:      indexGen.AddList(_numVertices);  return;    
	case GX_DRAW_TRIANGLE_STRIP: indexGen.AddStrip(_numVertices);     return;
	case GX_DRAW_TRIANGLE_FAN:   indexGen.AddFan(_numVertices);       return;
	case GX_DRAW_LINE_STRIP:     indexGen.AddLineStrip(_numVertices); return;
	case GX_DRAW_LINES:		     indexGen.AddLineList(_numVertices);  return;
	case GX_DRAW_POINTS:         indexGen.AddPointList(_numVertices); return;
	}
}

const Collection collectionTypeLUT[8] =
{
	C_TRIANGLES,//quads
	C_NOTHING,  //nothing
	C_TRIANGLES,//triangles
	C_TRIANGLES,//strip
	C_TRIANGLES,//fan
	C_LINES,    //lines
	C_LINES,    //linestrip
	C_POINTS    //guess :P
};

D3DVertex *vbufferwrite;

void CVertexHandler::DrawVertices(int _primitive, int _numVertices, const DecodedVArray *varray)
{
	if (_numVertices <= 0) //This check is pretty stupid... 
		return;
	
	Collection type = collectionTypeLUT[_primitive];
	if (type == C_NOTHING)
		return;

    DVSTARTPROFILE();
	_assert_msg_(type != C_NOTHING, "type == C_NOTHING!!", "WTF");
	
	if (indexGen.GetNumVerts() > 1000) // TODO(ector): Raise?
		Flush();

	ADDSTAT(stats.thisFrame.numPrims, _numVertices);

	if (collection != type)
	{
		//We are NOT collecting the right type.
		Flush();
		collection = type;
		u16 *ptr = 0;
	
		if (type != C_POINTS)
		{
			ptr = fakeIBuffer;
			indexGen.Start((unsigned short*)ptr);
			AddIndices(_primitive,_numVertices);
		}

		vbufferwrite = fakeVBuffer;

		if (_numVertices >= MAXVBUFFERSIZE)
			MessageBox(NULL, "To much vertices for the buffer", "Video.DLL", MB_OK);

		CTransformEngine::TransformVertices(_numVertices,varray,vbufferwrite);
	}
	else //We are collecting the right type, keep going
	{
		_assert_msg_(vbufferwrite!=0, "collecting: vbufferwrite == 0!","WTF");
		INCSTAT(stats.numJoins);
		//Success, keep adding to unlocked buffer
		int last=indexGen.GetNumVerts();
		AddIndices(_primitive,_numVertices);

		if (_numVertices >= MAXVBUFFERSIZE)
			MessageBox(NULL, "Too many vertices for the buffer", "Video.DLL", MB_OK);
		CTransformEngine::TransformVertices(_numVertices,varray,vbufferwrite + last);
	}
}

const D3DPRIMITIVETYPE pts[3] = 
{
	D3DPT_POINTLIST, //DUMMY
	D3DPT_TRIANGLELIST, 
	D3DPT_LINELIST,
};

void CVertexHandler::Flush()
{
    DVSTARTPROFILE();

    if (collection != C_NOTHING)
	{
		ActivateTextures();

		int numVertices = indexGen.GetNumVerts();
		if (numVertices != 0)
		{
			PShaderCache::SetShader(); // TODO(ector): only do this if shader has changed
			VShaderCache::SetShader(); // TODO(ector): only do this if shader has changed
			if (collection != C_POINTS)
			{
				int numPrimitives = indexGen.GetNumPrims();
				D3D::dev->DrawIndexedPrimitiveUP(pts[(int)collection], 
					                             0, 
												 numVertices, 
												 numPrimitives,
												 fakeIBuffer,
												 D3DFMT_INDEX16,
												 fakeVBuffer,
												 sizeof(D3DVertex));
			}
			else
			{
				D3D::dev->SetIndices(0);

				// D3D::dev->DrawPrimitiveUP(D3DPT_POINTLIST,
				//	                      numVertices,
				//						  fakeVBuffer,
				//						  sizeof(D3DVertex));
				Renderer::DrawPrimitiveUP( D3DPT_POINTLIST, numVertices, fakeVBuffer, sizeof(D3DVertex) );
			}
		}
		collection = C_NOTHING;
	}
}


/*
	char szTmp[256];
	sprintf(szTmp, "Batch size : %i",_numVertices);
	g_VideoInitialize.pLog(szTmp);


	char szTemp[256];
	sprintf(szTemp, "count: %i", count);
	SendMessage(g_VideoInitialize.hStatusBar, SB_SETTEXT,0,(LPARAM)szTemp);	

	static bool bWaitStep = false;
	if (count == 24800)
	{
		bWaitStep = true;
		Renderer::Flush();
	}
	if (GetAsyncKeyState('J'))
		bWaitStep = true;
	if (GetAsyncKeyState('U'))
		bWaitStep = false;

	if (bWaitStep)
	{	
		static bool bKey = false;
		while (GetAsyncKeyState('H') == 0)
		{			
			if (GetAsyncKeyState(VK_SPACE))
				_asm int 3;
			if (GetAsyncKeyState('U'))
			{
				bWaitStep = false;
				break;
			}
		}
		Renderer::Flush();
		// wait for key release
		while (GetAsyncKeyState ('H')){
		};
	}
}
*/
//	old = bpmem.combiners[0].colorC.hex;
