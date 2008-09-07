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

#include "stdafx.h"
#include "D3DShader.h"
#include "VertexShader.h"
#include "BPStructs.h"

char text2[65536];
#define WRITE p+=sprintf

void WriteTexgen(char *&p, int n);

const char *GenerateVertexShader()
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
	//WRITE(p,"  output.fog = 0.0f;");
	WRITE(p,"return output;\n");
	WRITE(p,"}\n");
	WRITE(p,"\0");

//	MessageBox(0,text2,0,0);
	return text2;
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
