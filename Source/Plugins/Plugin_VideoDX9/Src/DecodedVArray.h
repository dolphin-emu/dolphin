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

#ifndef _DECODED_VARRAY_H
#define _DECODED_VARRAY_H

#include "Vec3.h"
#include "Common.h"

typedef Vec3 DecPos;
typedef Vec3 DecNormal;

struct DecUV
{
	float u,v;	
};

typedef u32 DecColor;
typedef u8  DecMtxInd;

int ComputeVertexSize(u32 components);

//TODO(ector): Change the internal implementation to pack it tight according to components
// The tight packing will be fed directly to the gfx card in the mystic future.
class DecodedVArray
{	
	int size;
	u32 components;
	int vertexSize;

public:
	int count;
	DecodedVArray();
	~DecodedVArray();
	void SetComponents(u32 comps) {components = comps; vertexSize = ComputeVertexSize(components);
	                               ComputeComponents(); }
	u32  GetComponents() const {return components;}
	void Create(int _size, int pmcount, int tmcount, int nrmcount, int colcount, int tccount);
	void Zero();
	void Destroy();
	void Reset() {count=0;}
	int  GetSize()  {return size;}
	int  GetCount() {return count;}
	void Next()    {count++;}
	void SetPosNrmIdx(int i)    {posMtxInds[count] = i;}
	void SetTcIdx(int n, int i) {texMtxInds[n][count] = i;}
	void SetPosX(float x) {positions[count].x=x;}
	void SetPosY(float y) {positions[count].y=y;}
	void SetPosZ(float z) {positions[count].z=z;}

	void SetNormalX(int n,float x) {normals[n][count].x=x;}
	void SetNormalY(int n,float y) {normals[n][count].y=y;}
	void SetNormalZ(int n,float z) {normals[n][count].z=z;}
	void SetU(int n, float u) {uvs[n][count].u = u;}
	void SetV(int n, float v) {uvs[n][count].v = v;}
	void SetPosition(float x, float y, float z) {
		positions[count].x=x;
		positions[count].y=y;
		positions[count].z=z;
	}
	void SetNormal(int n, float x, float y, float z) {
		normals[n][count].x=x;
		normals[n][count].y=y;
		normals[n][count].z=z;
	}
	void SetColor(int n, u32 c)
	{
		colors[n][count] = c;
	}
	void SetUV(int n, float u, float v) {
		uvs[n][count].u=u;
		uvs[n][count].v=v;
	}
	
	void ComputeComponents() {
		hasPosMatIdx = (components & (1 << 1)) != 0;
		for(int i = 0; i < 8; i++) 
			hasTexMatIdx[i] = (components & (1 << (i + 2))) != 0;
		hasNrm = (components & (1 << 10)) != 0;
	}

	const DecPos &GetPos(int n) const { return positions[n]; }
	const DecNormal &GetNormal(int i, int n) const { return normals[i][n]; }
	const DecColor &GetColor(int i, int n) const { return colors[i][n]; }
	const DecUV &GetUV(int i, int n) const { return uvs[i][n]; }
	const DecMtxInd &GetPosMtxInd(int n) const { return posMtxInds[n]; }
	const DecMtxInd &GetTexMtxInd(int i, int n) const { return texMtxInds[i][n]; }
//private:
	DecPos *positions;
	DecNormal *normals[3];
	DecColor *colors[2];
	DecUV *uvs[8];
	DecMtxInd *posMtxInds;
	DecMtxInd *texMtxInds[8];

	// Component data
	bool hasPosMatIdx, hasTexMatIdx[8], hasNrm;
};

#endif
