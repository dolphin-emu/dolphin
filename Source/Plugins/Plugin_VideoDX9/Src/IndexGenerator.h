#pragma once

class IndexGenerator
{
	unsigned short *ptr;
	int numPrims;
	int index;
public:
	void Start(unsigned short *startptr);
	void AddList(int numVerts);
	void AddStrip(int numVerts);
	void AddLineList(int numVerts);
	void AddPointList(int numVerts); //dummy for counting vertices
	void AddLineStrip(int numVerts);
	void AddFan(int numVerts);
	void AddQuads(int numVerts);
	int GetNumPrims() {return numPrims;} //returns numprimitives
	int GetNumVerts() {return index;} //returns numprimitives
};