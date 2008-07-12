#include "stdafx.h"	
#include "IndexGenerator.h"

/*
*	
QUAD simulator

0   2   4   6  
1   3   5   7
021231 243453 
*/


void IndexGenerator::Start(unsigned short *startptr)
{
	ptr=startptr;
	index=0;
	numPrims=0;
}

void IndexGenerator::AddList(int numVerts)
{
	int numTris = numVerts/3;
	if (numTris<=0) return;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index+i*3;
		*ptr++ = index+i*3+1;
		*ptr++ = index+i*3+2;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddStrip(int numVerts)
{
	int numTris = numVerts-2;
	if (numTris<=0) return;
	bool wind = false;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+(wind?2:1);
		*ptr++ = index+i+(wind?1:2);
		wind = !wind;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddLineList(int numVerts)
{
	int numLines= numVerts/2;
	if (numLines<=0) return;
	for (int i=0; i<numLines; i++)
	{
		*ptr++ = index+i*2;
		*ptr++ = index+i*2+1;
	}
	index += numVerts;
	numPrims += numLines;
}

void IndexGenerator::AddLineStrip(int numVerts)
{
	int numLines = numVerts-1;
	if (numLines<=0) return;
	for (int i=0; i<numLines; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+1;
	}
	index += numVerts;
	numPrims += numLines;
}


void IndexGenerator::AddFan(int numVerts)
{
	int numTris = numVerts-2;
	if (numTris<=0) return;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index;
		*ptr++ = index+i+1;
		*ptr++ = index+i+2;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddQuads(int numVerts)
{
	int numTris = (numVerts/4)*2;
	if (numTris<=0) return;
	for (int i=0; i<numTris/2; i++)
	{
		*ptr++=index+i*4;
		*ptr++=index+i*4+1;
		*ptr++=index+i*4+3;
		*ptr++=index+i*4+1;
		*ptr++=index+i*4+2;
		*ptr++=index+i*4+3;
	}
	index += numVerts;
	numPrims += numTris;
}


void IndexGenerator::AddPointList(int numVerts)
{
	index += numVerts;
}