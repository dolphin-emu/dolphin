// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoCommon.h"

#include "XFMemLoader.h"
#include "CPMemLoader.h"
#include "Clipper.h"
#include "HW/Memmap.h"

XFRegisters swxfregs;

void InitXFMemory()
{
	memset(&swxfregs, 0, sizeof(swxfregs));
}

void XFWritten(u32 transferSize, u32 baseAddress)
{
	u32 topAddress = baseAddress + transferSize;

	if (baseAddress <= 0x1026 && topAddress >= 0x1020)
		Clipper::SetViewOffset();

	// fix lights so invalid values don't trash the lighting computations	
	if (baseAddress <= 0x067f && topAddress >= 0x0604)
	{
		u32* x = swxfregs.lights;

		// go through all lights
		for (int light = 0; light < 8; light++)
		{
			// skip to floating point values
			x += 4;

			for (int i = 0; i < 12; i++)
			{
				u32 xVal = *x;

				// if the exponent is 255 then the number is inf or nan
				if ((xVal & 0x7f800000) == 0x7f800000)
					*x = 0;

				x++;
			}
		}
	}
}

void SWLoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
{
	u32 size = transferSize;

	// do not allow writes past registers
	if (baseAddress + transferSize > 0x1058)
	{
		INFO_LOG(VIDEO, "xf load exceeds address space: %x %d bytes\n", baseAddress, transferSize);

		if (baseAddress >= 0x1058)
			size = 0;
		else
			size = 0x1058 - baseAddress;
	}

	if (size > 0)
	{
		memcpy_gc( &((u32*)&swxfregs)[baseAddress], pData, size * 4);
		XFWritten(transferSize, baseAddress);
	}
}

void SWLoadIndexedXF(u32 val, int array)
{
	int index = val >> 16;
	int address = val & 0xFFF; //check mask
	int size = ((val >> 12) & 0xF) + 1;
	//load stuff from array to address in xf mem

	u32 *pData = (u32*)Memory::GetPointer(arraybases[array] + arraystrides[array]*index);

	// byteswap data
	u32 buffer[16];
	for (int i = 0; i < size; ++i)
		buffer[i] = Common::swap32(*(pData + i));

	SWLoadXFReg(size, address, buffer);
}
