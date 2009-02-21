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

#include "Common.h"
#include "Globals.h"
#include "XFMemory.h"
#include "VertexManager.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"

// LoadXFReg 0x10
void LoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
{
    u32 address = baseAddress;
    for (int i = 0; i < (int)transferSize; i++)
    {
        address = baseAddress + i;

        // Setup a Matrix
        if (address < 0x1000)
        {
            VertexManager::Flush();
			VertexShaderManager::InvalidateXFRange(address, address + transferSize);
            //PRIM_LOG("xfmem write: 0x%x-0x%x\n", address, address+transferSize);

            u32* p1 = &xfmem[address];
            memcpy_gc(p1, &pData[i], transferSize*4);
            i += transferSize;
        }
        else if (address<0x2000)
        {
            u32 data = pData[i];
            switch (address)
            {
            case 0x1000: // error
                break;
            case 0x1001: // diagnostics
                break;
            case 0x1002: // internal state 0
                break;
            case 0x1003: // internal state 1
                break;
            case 0x1004: // xf_clock
                break;
            case 0x1005: // clipdisable
                if (data & 1) { // disable clipping detection
                }
                if (data & 2) { // disable trivial rejection
                }
                if (data & 4) { // disable cpoly clipping acceleration
                }
                break;
            case 0x1006: //SetGPMetric
                break;

            case 0x1008: //__GXXfVtxSpecs, wrote 0004
                xfregs.hostinfo = *(INVTXSPEC*)&data;
                break;

            case 0x1009: //GXSetNumChans (no)
				if ((u32)xfregs.nNumChans != (data&3)) {
                    VertexManager::Flush();
                    xfregs.nNumChans = data&3;
                }
                break;
            case 0x100a: //GXSetChanAmbientcolor
                if (xfregs.colChans[0].ambColor != data) {
                    VertexManager::Flush();
                    xfregs.colChans[0].ambColor = data;
					VertexShaderManager::SetMaterialColor(0, data);
                }
                break;
            case 0x100b: //GXSetChanAmbientcolor
                if (xfregs.colChans[1].ambColor != data) {
                    VertexManager::Flush();
                    xfregs.colChans[1].ambColor = data;
					VertexShaderManager::SetMaterialColor(1, data);
                }
                break;
            case 0x100c: //GXSetChanMatcolor (rgba)
                if (xfregs.colChans[0].matColor != data) {
                    VertexManager::Flush();
                    xfregs.colChans[0].matColor = data;
					VertexShaderManager::SetMaterialColor(2, data);
                }
                break;
            case 0x100d: //GXSetChanMatcolor (rgba)
                if (xfregs.colChans[1].matColor != data) {
                    VertexManager::Flush();
                    xfregs.colChans[1].matColor = data;
					VertexShaderManager::SetMaterialColor(3, data);
                }
                break;

            case 0x100e: // color0
                if (xfregs.colChans[0].color.hex != (data & 0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[0].color.hex = data;
                }
                break;
            case 0x100f: // color1
                if (xfregs.colChans[1].color.hex != (data & 0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[1].color.hex = data;
                }
                break;
            case 0x1010: // alpha0
                if (xfregs.colChans[0].alpha.hex != (data & 0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[0].alpha.hex = data;
                }
                break;
            case 0x1011: // alpha1
                if (xfregs.colChans[1].alpha.hex != (data & 0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[1].alpha.hex = data;
                }
                break;
            case 0x1012: // dual tex transform
                if (xfregs.bEnableDualTexTransform != (data & 1)) {
                    VertexManager::Flush();
                    xfregs.bEnableDualTexTransform = data & 1;
                }
                break;

            case 0x1013:
            case 0x1014:
            case 0x1015:
            case 0x1016:
            case 0x1017:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            case 0x1018:
                //_assert_msg_(GX_XF, 0, "XF matrixindex0");
				VertexShaderManager::SetTexMatrixChangedA(data); //?
                break;
            case 0x1019:
                //_assert_msg_(GX_XF, 0, "XF matrixindex1");
                VertexShaderManager::SetTexMatrixChangedB(data); //?
                break;

            case 0x101a:
                VertexManager::Flush();
                VertexShaderManager::SetViewport((float*)&pData[i]);
				PixelShaderManager::SetViewport((float*)&pData[i]);
                i += 6;
                break;

            case 0x101c: // paper mario writes 16777216.0f, 1677721.75
                break;
            case 0x101f: // paper mario writes 16777216.0f, 5033165.0f
                break;

            case 0x1020:
                VertexManager::Flush();
                VertexShaderManager::SetProjection((float*)&pData[i]);
                i += 7;
                return;

            case 0x103f: // GXSetNumTexGens
                if ((u32)xfregs.numTexGens != data) {
                    VertexManager::Flush();
                    xfregs.numTexGens = data;
                }
                break;

            case 0x1040: xfregs.texcoords[0].texmtxinfo.hex = data; break;
            case 0x1041: xfregs.texcoords[1].texmtxinfo.hex = data; break;
            case 0x1042: xfregs.texcoords[2].texmtxinfo.hex = data; break;
            case 0x1043: xfregs.texcoords[3].texmtxinfo.hex = data; break;
            case 0x1044: xfregs.texcoords[4].texmtxinfo.hex = data; break;
            case 0x1045: xfregs.texcoords[5].texmtxinfo.hex = data; break;
            case 0x1046: xfregs.texcoords[6].texmtxinfo.hex = data; break;
            case 0x1047: xfregs.texcoords[7].texmtxinfo.hex = data; break;

            case 0x1048:
            case 0x1049:
            case 0x104a:
            case 0x104b:
            case 0x104c:
            case 0x104d:
            case 0x104e:
            case 0x104f:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            case 0x1050: xfregs.texcoords[0].postmtxinfo.hex = data; break;
            case 0x1051: xfregs.texcoords[1].postmtxinfo.hex = data; break;
            case 0x1052: xfregs.texcoords[2].postmtxinfo.hex = data; break;
            case 0x1053: xfregs.texcoords[3].postmtxinfo.hex = data; break;
            case 0x1054: xfregs.texcoords[4].postmtxinfo.hex = data; break;
            case 0x1055: xfregs.texcoords[5].postmtxinfo.hex = data; break;
            case 0x1056: xfregs.texcoords[6].postmtxinfo.hex = data; break;
            case 0x1057: xfregs.texcoords[7].postmtxinfo.hex = data; break;

            default:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            }
        }
        else if (address >= 0x4000)
        {
            // MessageBox(NULL, "1", "1", MB_OK);
            //4010 __GXSetGenMode
        }
    }
}

// TODO - verify that it is correct. Seems to work, though.
void LoadIndexedXF(u32 val, int array)
{
    int index = val >> 16;
    int address = val & 0xFFF; //check mask
    int size = ((val >> 12) & 0xF) + 1;
    //load stuff from array to address in xf mem

    VertexManager::Flush();
    VertexShaderManager::InvalidateXFRange(address, address + size);
    //PRIM_LOG("xfmem iwrite: 0x%x-0x%x\n", address, address + size);

    for (int i = 0; i < size; i++)
        xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array] * index + i * 4);
}
