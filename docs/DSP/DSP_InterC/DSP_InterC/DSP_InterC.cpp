// DSP_InterC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "DSPOpcodes.h"

uint16 g_IMemory[0x1000];
uint16 g_currentAddress;

uint16 FetchOpcode()
{
	uint16 value = swap16(g_IMemory[g_currentAddress & 0x0FFF]);
	g_currentAddress++;
	return value;
}

void DecodeOpcode(uint16 op);

void Decode(uint16 startAddress, uint16 endAddress)
{
	g_currentAddress = startAddress;
	
	while (g_currentAddress < endAddress)
	{
		uint16 oldPC = g_currentAddress;
		uint16 op = FetchOpcode();
		
		OutBuffer::Add("// l_%4X:", oldPC);
		DecodeOpcode(op);
	}
}


int _tmain(int argc, _TCHAR* argv[])
{
	FILE* pFile = fopen("c:\\_\\dsp_rom.bin", "rb");
	if (pFile == nullptr)
		return -1;

	fread(g_IMemory, 0x1000, 1, pFile);
	fclose(pFile);


	//////
	OutBuffer::Init();
	Decode(0x80e7, 0x81f9);


	return 0;
}

