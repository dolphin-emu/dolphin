// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/HW/Memmap.h"
#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/XFMemLoader.h"
#include "VideoCommon/VideoCommon.h"

void InitXFMemory()
{
	memset(&xfregs, 0, sizeof(xfregs));
	memset(&xfmem, 0, sizeof(xfmem));
}

void XFWritten(u32 transferSize, u32 baseAddress)
{
	u32 topAddress = baseAddress + transferSize;

	if (baseAddress <= 0x1026 && topAddress >= 0x1020)
		Clipper::SetViewOffset();

	// fix lights so invalid values don't trash the lighting computations
	if (baseAddress <= 0x067f && topAddress >= 0x0604)
	{
		u32* x = xfmem.lights;

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
	// do not allow writes past registers
	if (baseAddress + transferSize > 0x1058)
	{
		INFO_LOG(VIDEO, "XF load exceeds address space: %x %d bytes", baseAddress, transferSize);

		if (baseAddress >= 0x1058)
			transferSize = 0;
		else
			transferSize = 0x1058 - baseAddress;
	}

	// write to XF mem
	if (baseAddress < 0x1000 && transferSize > 0)
	{
		u32 end = baseAddress + transferSize;

		u32 xfMemBase = baseAddress;
		u32 xfMemTransferSize = transferSize;

		if (end >= 0x1000)
		{
			xfMemTransferSize = 0x1000 - baseAddress;

			baseAddress = 0x1000;
			transferSize = end - 0x1000;
		}
		else
		{
			transferSize = 0;
		}

		memcpy_gc((u32*)(&xfmem) + xfMemBase, pData, xfMemTransferSize * 4);
		XFWritten(xfMemTransferSize, xfMemBase);

		pData += xfMemTransferSize;
	}

	// write to XF regs
	if (transferSize > 0)
	{
		memcpy_gc((u32*)(&xfregs) + (baseAddress - 0x1000), pData, transferSize * 4);
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
