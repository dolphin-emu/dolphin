#include "stdafx.h"
#include "D3DShader.h"
#include "VertexShader.h"
#include "BPStructs.h"





//I hope we don't get too many hash collisions :p
//all these magic numbers are primes, it should help a bit
xformhash GetCurrentXForm()
{
	//return 0;
	xformhash hash = bpmem.genMode.numtexgens*8*17;
/*
	for (int i=0; i<bpmem.genMode.numtevstages+1; i++)
	{
		hash = _rotl(hash,3) ^ (bpmem.combiners[i].colorC.hex*13);
		hash = _rotl(hash,7) ^ ((bpmem.combiners[i].alphaC.hex&0xFFFFFFFC)*3);
	}
	for (int i=0; i<bpmem.genMode.numtevstages/2+1; i++)
	{
		hash = _rotl(hash,13) ^ (bpmem.tevorders[i].hex*7);
	}
	for (int i=0; i<8; i++)
	{
		hash = _rotl(hash,3) ^ bpmem.tevksel[i].swap1;
		hash = _rotl(hash,3) ^ bpmem.tevksel[i].swap2;
	}*/

	// to hash: bpmem.tevorders[j/2].getTexCoord(j&1);
	// also texcoords array
	return hash;
}

char text2[65536];
#define WRITE p+=sprintf

void WriteTexgen(char *&p, int n);

LPDIRECT3DVERTEXSHADER9 GenerateVertexShader()
{
	int numColors = 2;
	int numUV = 8;
	int numTexgen = bpmem.genMode.numtexgens;
	int numNormals = 3;
	bool fogEnable = false;
	bool hasNormal = true;

	char *p = text2;
	WRITE(p,"//Vertex Shader\n");
	WRITE(p,"//%i uv->%i texgens, %i colors\n",numUV,numTexgen,numColors);
	WRITE(p,"\n");

	WRITE(p,"struct VS_INPUT {\n");
	WRITE(p,"  float4 pos : POSITION;\n");
	WRITE(p,"  float3 normal : NORMAL;\n");
	if (numColors)
	WRITE(p,"  float4 colors[%i] : COLOR0;\n",numColors);
	if (numUV)
	WRITE(p,"  float3 uv[%i] : TEXCOORD0;\n",numUV);
	WRITE(p,"};\n");
	WRITE(p,"\n");

	WRITE(p,"struct VS_OUTPUT {\n");
	WRITE(p,"  float4 pos : POSITION;\n");
	WRITE(p,"  float4 colors[%i] : COLOR0;\n",numColors);
	if (numTexgen)
	WRITE(p,"  float4 uv[%i] : TEXCOORD0;\n",numTexgen);
	if (fogEnable)
	WRITE(p,"  float fog : FOG;\n",numTexgen);
	WRITE(p,"};\n");
	WRITE(p,"\n");

	WRITE(p,"uniform matrix matWorldViewProj : register(c0);\n");

	WRITE(p,"\n");

	WRITE(p,"VS_OUTPUT main(const VS_INPUT input)\n");
	WRITE(p,"{\n");
	WRITE(p,"  VS_OUTPUT output;");
	WRITE(p,"\n");
	
	WRITE(p,"  output.pos = mul(matWorldViewProj, input.pos);\n");

	for (int i = 0; i < (int)bpmem.genMode.numtexgens; i++)
	{
		//build the equation for this stage
		WriteTexgen(p,i);
	}

	WRITE(p,"  for (int i=0; i<2; i++)\n    output.colors[i] = input.colors[i];\n");
//	WRITE(p,"  output.fog = 0.0f;");
	WRITE(p,"return output;\n");
	WRITE(p,"}\n");
	WRITE(p,"\0");

//	MessageBox(0,text2,0,0);
	return D3D::CompileVShader(text2,(int)(p-text2));
}



/*
 *	xform->vertexshader ideas

*/

void WriteTexgen(char *&p, int n)
{
	WRITE(p,"  output.uv[%i] = float4(input.uv[%i].xy,0,input.uv[%i].z);\n",n,n,n);
}


void WriteLight(int color, int component)
{

}
