// Copyright (C) 2003 Dolphin Project.

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
#include "VideoCommon.h"
#include "XFMemory.h"
#include "CPMemory.h"
#include "NativeVertexWriter.h"
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
        if (address < XFMEM_ERROR)
        {
            VertexManager::Flush();
			VertexShaderManager::InvalidateXFRange(address, address + transferSize);
            //PRIM_LOG("xfmem write: 0x%x-0x%x\n", address, address+transferSize);

            memcpy_gc((u32*)&xfmem[address], &pData[i], transferSize*4);
            i += transferSize;
        }
		else if (address >= XFMEM_SETTEXMTXINFO && address <= XFMEM_SETTEXMTXINFO+7)
		{
			xfregs.texcoords[address - XFMEM_SETTEXMTXINFO].texmtxinfo.hex = pData[i];
		}
		else if (address >= XFMEM_SETPOSMTXINFO && address <= XFMEM_SETPOSMTXINFO+7)
		{
			xfregs.texcoords[address - XFMEM_SETPOSMTXINFO].postmtxinfo.hex = pData[i];
		}
        else if (address < 0x2000)
        {
            u32 data = pData[i];
            switch (address)
            {
            case XFMEM_ERROR:
            case XFMEM_DIAG:
            case XFMEM_STATE0: // internal state 0
            case XFMEM_STATE1: // internal state 1
            case XFMEM_CLOCK:
			case XFMEM_SETGPMETRIC:
                break;

            case XFMEM_CLIPDISABLE:
				//if (data & 1) {} // disable clipping detection
				//if (data & 2) {} // disable trivial rejection
				//if (data & 4) {} // disable cpoly clipping acceleration
                break;

            case XFMEM_VTXSPECS: //__GXXfVtxSpecs, wrote 0004
                xfregs.hostinfo = *(INVTXSPEC*)&data;
                break;

            case XFMEM_SETNUMCHAN:
				if ((u32)xfregs.nNumChans != (data & 3))
				{
                    VertexManager::Flush();
                    xfregs.nNumChans = data & 3;
                }
                break;

            case XFMEM_SETCHAN0_AMBCOLOR: // Channel Ambient Color
			case XFMEM_SETCHAN1_AMBCOLOR:
				{
					u8 chan = address - XFMEM_SETCHAN0_AMBCOLOR;
					if (xfregs.colChans[chan].ambColor != data) 
					{
						VertexManager::Flush();
						xfregs.colChans[chan].ambColor = data;
						VertexShaderManager::SetMaterialColor(chan, data);
					}
					break;
				}

            case XFMEM_SETCHAN0_MATCOLOR: // Channel Material Color
			case XFMEM_SETCHAN1_MATCOLOR:
				{
					u8 chan = address - XFMEM_SETCHAN0_MATCOLOR;
					if (xfregs.colChans[chan].matColor != data)
					{
						VertexManager::Flush();
						xfregs.colChans[chan].matColor = data;
						VertexShaderManager::SetMaterialColor(address - XFMEM_SETCHAN0_AMBCOLOR, data);
					}
					break;
				}

            case XFMEM_SETCHAN0_COLOR: // Channel Color
			case XFMEM_SETCHAN1_COLOR:
				{
					u8 chan = address - XFMEM_SETCHAN0_COLOR;
					if (xfregs.colChans[chan].color.hex != (data & 0x7fff))
					{
						VertexManager::Flush();
						xfregs.colChans[chan].color.hex = data;
					}
					break;
				}

            case XFMEM_SETCHAN0_ALPHA: // Channel Alpha
			case XFMEM_SETCHAN1_ALPHA:
				{
					u8 chan = address - XFMEM_SETCHAN0_ALPHA;
					if (xfregs.colChans[chan].alpha.hex != (data & 0x7fff))
					{
						VertexManager::Flush();
						xfregs.colChans[chan].alpha.hex = data;
					}
					break;
				}

            case XFMEM_DUALTEX:
                if (xfregs.bEnableDualTexTransform != (data & 1)) 
				{
                    VertexManager::Flush();
                    xfregs.bEnableDualTexTransform = data & 1;
                }
                break;


            case XFMEM_SETMATRIXINDA:
                //_assert_msg_(GX_XF, 0, "XF matrixindex0");
				VertexShaderManager::SetTexMatrixChangedA(data); // ?
                break;
            case XFMEM_SETMATRIXINDB:
                //_assert_msg_(GX_XF, 0, "XF matrixindex1");
                VertexShaderManager::SetTexMatrixChangedB(data); // ?
                break;

            case XFMEM_SETVIEWPORT:
                VertexManager::Flush();
                VertexShaderManager::SetViewport((float*)&pData[i]);
				PixelShaderManager::SetViewport((float*)&pData[i]);
                i += 6;
                break;

            case XFMEM_SETPROJECTION:
                VertexManager::Flush();
                VertexShaderManager::SetProjection((float*)&pData[i]);
                i += 7;
                break;

            case XFMEM_SETNUMTEXGENS: // GXSetNumTexGens
                if ((u32)xfregs.numTexGens != data)
				{
                    VertexManager::Flush();
                    xfregs.numTexGens = data;
                }
                break;

			// Maybe these are for Normals?
			case 0x1048: //xfregs.texcoords[0].nrmmtxinfo.hex = data; break; ??
            case 0x1049:
            case 0x104a:
            case 0x104b:
            case 0x104c:
            case 0x104d:
            case 0x104e:
            case 0x104f:
				DEBUG_LOG(VIDEO, "Possible Normal Mtx XF reg?: %x=%x\n", address, data);
				break;

			// --------------
			// Unknown Regs
			// --------------
			case 0x1013:
            case 0x1014:
            case 0x1015:
            case 0x1016:
            case 0x1017:

			case 0x101c:
				// paper mario writes 16777216.0f, 1677721.75
				// Killer 7 writes 0x4b800000 here on 3D rendering only
            case 0x101f:
				// paper mario writes 16777216.0f, 5033165.0f
				// Killer 7 alterns this between 0x4b800000 and 0x4b7ef9db on 3D rendering
            default:
                WARN_LOG(VIDEO, "Unknown XF Reg: %x=%x\n", address, data);
                break;
            }
        }
    }
}

// TODO - verify that it is correct. Seems to work, though.
void LoadIndexedXF(u32 val, int array)
{
    int index = val >> 16;
    int address = val & 0xFFF; // check mask
    int size = ((val >> 12) & 0xF) + 1;
    //load stuff from array to address in xf mem

    VertexManager::Flush();
    VertexShaderManager::InvalidateXFRange(address, address+size);
    //PRIM_LOG("xfmem iwrite: 0x%x-0x%x\n", address, address+size);

    for (int i = 0; i < size; i++)
        xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array] * index + i * 4);
}
