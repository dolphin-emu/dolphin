// Copyright (C) 2003-2009 Dolphin Project.

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

#include "../../../Core/VideoCommon/Src/VideoCommon.h"

#include "XFMemLoader.h"
#include "CPMemLoader.h"
#include "Clipper.h"


XFRegisters xfregs;

void InitXFMemory()
{
    memset(&xfregs, 0, sizeof(xfregs));
}

void XFWritten(u32 transferSize, u32 baseAddress)
{
    u32 topAddress = baseAddress + transferSize;

    if (baseAddress <= 0x1026 && topAddress >= 0x1020)
        Clipper::SetViewOffset();
}

void LoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
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

    if (size > 0) {
        memcpy_gc( &((u32*)&xfregs)[baseAddress], pData, size * 4);
        XFWritten(transferSize, baseAddress);
    }

}

void LoadIndexedXF(u32 val, int array)
{
    int index = val >> 16;
    int address = val & 0xFFF; //check mask
    int size = ((val >> 12) & 0xF) + 1;
    //load stuff from array to address in xf mem

    u32* xfmem = (u32*)&xfregs;

    for (int i = 0; i < size; i++)
        xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array]*index + i*4);
}
