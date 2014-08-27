// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/XFMemory.h"

static void XFMemWritten(u32 transferSize, u32 baseAddress)
{
	VertexManager::Flush();
	VertexShaderManager::InvalidateXFRange(baseAddress, baseAddress + transferSize);
}

static void XFRegWritten(int transferSize, u32 baseAddress)
{
	u32 address = baseAddress;
	u32 dataIndex = 0;

	while (transferSize > 0 && address < 0x1058)
	{
		u32 newValue = DataPeek<u32>(dataIndex * sizeof(u32));
		u32 nextAddress = address + 1;

		switch (address)
		{
		case XFMEM_ERROR:
		case XFMEM_DIAG:
		case XFMEM_STATE0: // internal state 0
		case XFMEM_STATE1: // internal state 1
		case XFMEM_CLOCK:
		case XFMEM_SETGPMETRIC:
			nextAddress = 0x1007;
			break;

		case XFMEM_CLIPDISABLE:
			//if (data & 1) {} // disable clipping detection
			//if (data & 2) {} // disable trivial rejection
			//if (data & 4) {} // disable cpoly clipping acceleration
			break;

		case XFMEM_VTXSPECS: //__GXXfVtxSpecs, wrote 0004
			break;

		case XFMEM_SETNUMCHAN:
			if (xfmem.numChan.numColorChans != (newValue & 3))
				VertexManager::Flush();
			break;

		case XFMEM_SETCHAN0_AMBCOLOR: // Channel Ambient Color
		case XFMEM_SETCHAN1_AMBCOLOR:
			{
				u8 chan = address - XFMEM_SETCHAN0_AMBCOLOR;
				if (xfmem.ambColor[chan] != newValue)
				{
					VertexManager::Flush();
					VertexShaderManager::SetMaterialColorChanged(chan, newValue);
				}
				break;
			}

		case XFMEM_SETCHAN0_MATCOLOR: // Channel Material Color
		case XFMEM_SETCHAN1_MATCOLOR:
			{
				u8 chan = address - XFMEM_SETCHAN0_MATCOLOR;
				if (xfmem.matColor[chan] != newValue)
				{
					VertexManager::Flush();
					VertexShaderManager::SetMaterialColorChanged(chan + 2, newValue);
				}
				break;
			}

		case XFMEM_SETCHAN0_COLOR: // Channel Color
		case XFMEM_SETCHAN1_COLOR:
		case XFMEM_SETCHAN0_ALPHA: // Channel Alpha
		case XFMEM_SETCHAN1_ALPHA:
			if (((u32*)&xfmem)[address] != (newValue & 0x7fff))
				VertexManager::Flush();
			break;

		case XFMEM_DUALTEX:
			if (xfmem.dualTexTrans.enabled != (newValue & 1))
				VertexManager::Flush();
			break;


		case XFMEM_SETMATRIXINDA:
			//_assert_msg_(GX_XF, 0, "XF matrixindex0");
			VertexShaderManager::SetTexMatrixChangedA(newValue);
			break;
		case XFMEM_SETMATRIXINDB:
			//_assert_msg_(GX_XF, 0, "XF matrixindex1");
			VertexShaderManager::SetTexMatrixChangedB(newValue);
			break;

		case XFMEM_SETVIEWPORT:
		case XFMEM_SETVIEWPORT+1:
		case XFMEM_SETVIEWPORT+2:
		case XFMEM_SETVIEWPORT+3:
		case XFMEM_SETVIEWPORT+4:
		case XFMEM_SETVIEWPORT+5:
			VertexManager::Flush();
			VertexShaderManager::SetViewportChanged();
			PixelShaderManager::SetViewportChanged();

			nextAddress = XFMEM_SETVIEWPORT + 6;
			break;

		case XFMEM_SETPROJECTION:
		case XFMEM_SETPROJECTION+1:
		case XFMEM_SETPROJECTION+2:
		case XFMEM_SETPROJECTION+3:
		case XFMEM_SETPROJECTION+4:
		case XFMEM_SETPROJECTION+5:
		case XFMEM_SETPROJECTION+6:
			VertexManager::Flush();
			VertexShaderManager::SetProjectionChanged();

			nextAddress = XFMEM_SETPROJECTION + 7;
			break;

		case XFMEM_SETNUMTEXGENS: // GXSetNumTexGens
			if (xfmem.numTexGen.numTexGens != (newValue & 15))
				VertexManager::Flush();
			break;

		case XFMEM_SETTEXMTXINFO:
		case XFMEM_SETTEXMTXINFO+1:
		case XFMEM_SETTEXMTXINFO+2:
		case XFMEM_SETTEXMTXINFO+3:
		case XFMEM_SETTEXMTXINFO+4:
		case XFMEM_SETTEXMTXINFO+5:
		case XFMEM_SETTEXMTXINFO+6:
		case XFMEM_SETTEXMTXINFO+7:
			VertexManager::Flush();

			nextAddress = XFMEM_SETTEXMTXINFO + 8;
			break;

		case XFMEM_SETPOSMTXINFO:
		case XFMEM_SETPOSMTXINFO+1:
		case XFMEM_SETPOSMTXINFO+2:
		case XFMEM_SETPOSMTXINFO+3:
		case XFMEM_SETPOSMTXINFO+4:
		case XFMEM_SETPOSMTXINFO+5:
		case XFMEM_SETPOSMTXINFO+6:
		case XFMEM_SETPOSMTXINFO+7:
			VertexManager::Flush();

			nextAddress = XFMEM_SETPOSMTXINFO + 8;
			break;

		// --------------
		// Unknown Regs
		// --------------

		// Maybe these are for Normals?
		case 0x1048: //xfmem.texcoords[0].nrmmtxinfo.hex = data; break; ??
		case 0x1049:
		case 0x104a:
		case 0x104b:
		case 0x104c:
		case 0x104d:
		case 0x104e:
		case 0x104f:
			DEBUG_LOG(VIDEO, "Possible Normal Mtx XF reg?: %x=%x", address, newValue);
			break;

		case 0x1013:
		case 0x1014:
		case 0x1015:
		case 0x1016:
		case 0x1017:

		default:
			WARN_LOG(VIDEO, "Unknown XF Reg: %x=%x", address, newValue);
			break;
		}

		int transferred = nextAddress - address;
		address = nextAddress;

		transferSize -= transferred;
		dataIndex += transferred;
	}
}

void LoadXFReg(u32 transferSize, u32 baseAddress)
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

		XFMemWritten(xfMemTransferSize, xfMemBase);
		for (u32 i = 0; i < xfMemTransferSize; i++)
		{
			((u32*)&xfmem)[xfMemBase + i] = DataRead<u32>();
		}
	}

	// write to XF regs
	if (transferSize > 0)
	{
		XFRegWritten(transferSize, baseAddress);
		for (u32 i = 0; i < transferSize; i++)
		{
			((u32*)&xfmem)[baseAddress + i] = DataRead<u32>();
		}
	}
}

// TODO - verify that it is correct. Seems to work, though.
void LoadIndexedXF(u32 val, int refarray)
{
	int index = val >> 16;
	int address = val & 0xFFF; // check mask
	int size = ((val >> 12) & 0xF) + 1;
	//load stuff from array to address in xf mem

	u32* currData = (u32*)(&xfmem) + address;
	u32* newData = (u32*)Memory::GetPointer(g_main_cp_state.array_bases[refarray] + g_main_cp_state.array_strides[refarray] * index);
	bool changed = false;
	for (int i = 0; i < size; ++i)
	{
		if (currData[i] != Common::swap32(newData[i]))
		{
			changed = true;
			XFMemWritten(size, address);
			break;
		}
	}
	if (changed)
	{
		for (int i = 0; i < size; ++i)
			currData[i] = Common::swap32(newData[i]);
	}
}
