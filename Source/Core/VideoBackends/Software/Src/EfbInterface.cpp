// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "EfbInterface.h"
#include "BPMemLoader.h"
#include "LookUpTables.h"
#include "SWPixelEngine.h"


u8 efb[EFB_WIDTH*EFB_HEIGHT*6];


namespace EfbInterface
{

	u8 efbColorTexture[EFB_WIDTH*EFB_HEIGHT*4];

	inline u32 GetColorOffset(u16 x, u16 y)
	{
		return (x + y * EFB_WIDTH) * 3;
	}

	inline u32 GetDepthOffset(u16 x, u16 y)
	{
		return (x + y * EFB_WIDTH) * 3 + DEPTH_BUFFER_START;
	}

	void DoState(PointerWrap &p)
	{
		p.DoArray(efb, EFB_WIDTH*EFB_HEIGHT*6);
		p.DoArray(efbColorTexture, EFB_WIDTH*EFB_HEIGHT*4);
	}

	void SetPixelAlphaOnly(u32 offset, u8 a)
	{
			switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_Z24:
		case PIXELFMT_RGB565_Z16:
			// do nothing
			break;
		case PIXELFMT_RGBA6_Z24:
			{
				u32 a32 = a;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xffffffc0;
				val |= (a32 >> 2) & 0x0000003f;
				*dst = val;
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}
	}

	void SetPixelColorOnly(u32 offset, u8 *rgb)
	{
		switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_Z24:
			{
				u32 src = *(u32*)rgb;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= src >> 8;
				*dst = val;
			}
			break;
		case PIXELFMT_RGBA6_Z24:
			{
				u32 src = *(u32*)rgb;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff00003f;
				val |= (src >> 4) & 0x00000fc0;	// blue
				val |= (src >> 6) & 0x0003f000;	// green
				val |= (src >> 8) & 0x00fc0000;	// red
				*dst = val;
			}
			break;
		case PIXELFMT_RGB565_Z16:
			{
				INFO_LOG(VIDEO, "PIXELFMT_RGB565_Z16 is not supported correctly yet");
				u32 src = *(u32*)rgb;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= src >> 8;
				*dst = val;
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}
	}

	void SetPixelAlphaColor(u32 offset, u8 *color)
	{
			switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_Z24:
			{
				u32 src = *(u32*)color;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= src >> 8;
				*dst = val;
			}
			break;
		case PIXELFMT_RGBA6_Z24:
			{
				u32 src = *(u32*)color;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= (src >> 2) & 0x0000003f;	// alpha
				val |= (src >> 4) & 0x00000fc0;	// blue
				val |= (src >> 6) & 0x0003f000;	// green
				val |= (src >> 8) & 0x00fc0000;	// red
				*dst = val;
			}
			break;
		case PIXELFMT_RGB565_Z16:
			{
				INFO_LOG(VIDEO, "PIXELFMT_RGB565_Z16 is not supported correctly yet");
				u32 src = *(u32*)color;
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= src >> 8;
				*dst = val;
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}
	}

		void GetPixelColor(u32 offset, u8 *color)
	{
		switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_Z24:
			{
				u32 src = *(u32*)&efb[offset];
				u32 *dst = (u32*)color;
				u32 val = 0xff | ((src & 0x00ffffff) << 8);
				*dst = val;
			}
			break;
		case PIXELFMT_RGBA6_Z24:
			{
				u32 src = *(u32*)&efb[offset];
				color[ALP_C] = Convert6To8(src & 0x3f);
				color[BLU_C] = Convert6To8((src >> 6) & 0x3f);
				color[GRN_C] = Convert6To8((src >> 12) & 0x3f);
				color[RED_C] = Convert6To8((src >> 18) & 0x3f);
			}
			break;
		case PIXELFMT_RGB565_Z16:
			{
				INFO_LOG(VIDEO, "PIXELFMT_RGB565_Z16 is not supported correctly yet");
				u32 src = *(u32*)&efb[offset];
				u32 *dst = (u32*)color;
				u32 val = 0xff | ((src & 0x00ffffff) << 8);
				*dst = val;
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}
	}

	void SetPixelDepth(u32 offset, u32 depth)
	{
		switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_RGBA6_Z24:
		case PIXELFMT_Z24:
			{
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= depth & 0x00ffffff;
				*dst = val;				
			}
			break;
		case PIXELFMT_RGB565_Z16:
			{
				INFO_LOG(VIDEO, "PIXELFMT_RGB565_Z16 is not supported correctly yet");
				u32 *dst = (u32*)&efb[offset];
				u32 val = *dst & 0xff000000;
				val |= depth & 0x00ffffff;
				*dst = val;		
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}
	}

		u32 GetPixelDepth(u32 offset)
	{
		u32 depth = 0;

		switch (bpmem.zcontrol.pixel_format)
		{
		case PIXELFMT_RGB8_Z24:
		case PIXELFMT_RGBA6_Z24:
		case PIXELFMT_Z24:
			{
				depth = (*(u32*)&efb[offset]) & 0x00ffffff;
			}
			break;
		case PIXELFMT_RGB565_Z16:
			{
				INFO_LOG(VIDEO, "PIXELFMT_RGB565_Z16 is not supported correctly yet");
				depth = (*(u32*)&efb[offset]) & 0x00ffffff;
			}
			break;
		default:
			ERROR_LOG(VIDEO, "Unsupported pixel format: %i", bpmem.zcontrol.pixel_format);
		}

		return depth;
	}

	u32 GetSourceFactor(u8 *srcClr, u8 *dstClr, int mode)
	{
		switch (mode) {
			case 0:  // zero
				return 0;
			case 1:  // one
				return 0xffffffff;
			case 2:  // dstclr
				return *(u32*)dstClr;
			case 3:  // invdstclr
				return 0xffffffff - *(u32*)dstClr;
			case 4:  // srcalpha
				{
					u8 alpha = srcClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 5:  // invsrcalpha
				{
					u8 alpha = 0xff - srcClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 6:  // dstalpha
				{
					u8 alpha = dstClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 7:  // invdstalpha
				{
					u8 alpha = 0xff - dstClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
		}

		return 0;
	}

	u32 GetDestinationFactor(u8 *srcClr, u8 *dstClr, int mode)
	{
		switch (mode) {
			case 0:  // zero
				return 0;
			case 1:  // one
				return 0xffffffff;
				case 2:  // srcclr
				return *(u32*)srcClr;
			case 3:  // invsrcclr
				return 0xffffffff - *(u32*)srcClr;
			case 4:  // srcalpha
				{
					u8 alpha = srcClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 5:  // invsrcalpha
				{
					u8 alpha = 0xff - srcClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 6:  // dstalpha
				{
					u8 alpha = dstClr[ALP_C] & 0xff;
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
			case 7:  // invdstalpha
				{
					u8 alpha = 0xff - dstClr[ALP_C];
					u32 factor = alpha << 24 | alpha << 16 | alpha << 8 | alpha;
					return factor;
				}
		}

		return 0;
	}

	void BlendColor(u8 *srcClr, u8 *dstClr)
	{
		u32 srcFactor = GetSourceFactor(srcClr, dstClr, bpmem.blendmode.srcfactor);
		u32 dstFactor = GetDestinationFactor(srcClr, dstClr, bpmem.blendmode.dstfactor);

		for (int i = 0; i < 4; i++)
		{
			// add MSB of factors to make their range 0 -> 256
			u32 sf = (srcFactor & 0xff);
			sf += sf >> 7;

			u32 df = (dstFactor & 0xff);
			df += df >> 7;

			u32 color = (srcClr[i] * sf + dstClr[i] * df) >> 8;
			dstClr[i] = (color>255)?255:color;

			dstFactor >>= 8;
			srcFactor >>= 8;
		}
	}

	void LogicBlend(u32 srcClr, u32 &dstClr, int op)
	{
		switch (op)
		{
			case 0:  // clear
				dstClr = 0;
				break;
			case 1:  // and
				dstClr = srcClr & dstClr;
				break;
			case 2:  // revand
				dstClr = srcClr & (~dstClr);
				break;
			case 3:  // copy
				dstClr = srcClr;
				break;
			case 4:  // invand
				dstClr = (~srcClr) & dstClr;
				break;
			case 5:  // noop
		// Do nothing
				break;
			case 6:  // xor
				dstClr = srcClr ^ dstClr;
				break;
			case 7:  // or
				dstClr = srcClr | dstClr;
				break;
			case 8:  // nor
				dstClr = ~(srcClr | dstClr);
				break;
			case 9:  // equiv
				dstClr = ~(srcClr ^ dstClr);
				break;
			case 10: // inv
				dstClr = ~dstClr;
				break;
			case 11: // revor
				dstClr = srcClr | (~dstClr);
				break;
			case 12: // invcopy
				dstClr = ~srcClr;
				break;
			case 13: // invor
				dstClr = (~srcClr) | dstClr;
				break;
			case 14: // nand
				dstClr = ~(srcClr & dstClr);
				break;
			case 15: // set
				dstClr = 0xffffffff;
				break;
		}
	}

	void SubtractBlend(u8 *srcClr, u8 *dstClr)
	{
		for (int i = 0; i < 4; i++)
		{
			int c = (int)dstClr[i] - (int)srcClr[i];
			dstClr[i] = (c < 0)?0:c;
		}
	}

	void BlendTev(u16 x, u16 y, u8 *color)
	{
		u32 dstClr;
		u32 offset = GetColorOffset(x, y);

		u8 *dstClrPtr = (u8*)&dstClr;

		GetPixelColor(offset, dstClrPtr);

		if (bpmem.blendmode.blendenable)
		{
			if (bpmem.blendmode.subtract)
				SubtractBlend(color, dstClrPtr);
			else
				BlendColor(color, dstClrPtr);
		}
		else if (bpmem.blendmode.logicopenable)
		{
			LogicBlend(*((u32*)color), dstClr, bpmem.blendmode.logicmode);
		}
		else
		{
			dstClrPtr = color;
		}

		if (bpmem.dstalpha.enable)
			dstClrPtr[ALP_C] = bpmem.dstalpha.alpha;

		if (bpmem.blendmode.colorupdate)
		{
			if (bpmem.blendmode.alphaupdate)
				SetPixelAlphaColor(offset, dstClrPtr);
			else
				SetPixelColorOnly(offset, dstClrPtr);
		}
		else if (bpmem.blendmode.alphaupdate)
		{
			SetPixelAlphaOnly(offset, dstClrPtr[ALP_C]);
		}

		// branchless bounding box update
		SWPixelEngine::pereg.boxLeft = SWPixelEngine::pereg.boxLeft>x?x:SWPixelEngine::pereg.boxLeft;
		SWPixelEngine::pereg.boxRight = SWPixelEngine::pereg.boxRight<x?x:SWPixelEngine::pereg.boxRight;
		SWPixelEngine::pereg.boxTop = SWPixelEngine::pereg.boxTop>y?y:SWPixelEngine::pereg.boxTop;
		SWPixelEngine::pereg.boxBottom = SWPixelEngine::pereg.boxBottom<y?y:SWPixelEngine::pereg.boxBottom;
	}

	void SetColor(u16 x, u16 y, u8 *color)
	{
		u32 offset = GetColorOffset(x, y);
		if (bpmem.blendmode.colorupdate)
		{
			if (bpmem.blendmode.alphaupdate)
				SetPixelAlphaColor(offset, color);
			else
				SetPixelColorOnly(offset, color);
		}
		else if (bpmem.blendmode.alphaupdate)
		{
			SetPixelAlphaOnly(offset, color[ALP_C]);
		}
	}

	void SetDepth(u16 x, u16 y, u32 depth)
	{
		if (bpmem.zmode.updateenable)
			SetPixelDepth(GetDepthOffset(x, y), depth);
	}

	void GetColor(u16 x, u16 y, u8 *color)
	{
		u32 offset = GetColorOffset(x, y);
		GetPixelColor(offset, color);
	}

	u32 GetDepth(u16 x, u16 y)
	{
		u32 offset = GetDepthOffset(x, y);
		return GetPixelDepth(offset);
	}

	u8 *GetPixelPointer(u16 x, u16 y, bool depth)
	{
		if (depth)
			return &efb[GetDepthOffset(x, y)];
		return &efb[GetColorOffset(x, y)];
	}

	void UpdateColorTexture()
	{
		u32 color;
		u8* colorPtr = (u8*)&color;
		u32* texturePtr = (u32*)efbColorTexture;
		u32 textureAddress = 0;
		u32 efbOffset = 0;

		for (u16 y = 0; y < EFB_HEIGHT; y++)
		{
			for (u16 x = 0; x < EFB_WIDTH; x++)
			{
				GetPixelColor(efbOffset, colorPtr);
				efbOffset += 3;
				texturePtr[textureAddress++] = Common::swap32(color);  // ABGR->RGBA
			}
		}
	}

	bool ZCompare(u16 x, u16 y, u32 z)
	{
		u32 offset = GetDepthOffset(x, y);
		u32 depth = GetPixelDepth(offset);

		bool pass;

		switch (bpmem.zmode.func)
		{
			case COMPARE_NEVER:
				pass = false;
				break;
			case COMPARE_LESS:
				pass = z < depth;
				break;
			case COMPARE_EQUAL:
				pass = z == depth;
				break;
			case COMPARE_LEQUAL:
				pass = z <= depth;
				break;
			case COMPARE_GREATER:
				pass = z > depth;
				break;
			case COMPARE_NEQUAL:
				pass = z != depth;
				break;
			case COMPARE_GEQUAL:
				pass = z >= depth;
				break;
			case COMPARE_ALWAYS:
				pass = true;
				break;
			default:
				pass = false;
				ERROR_LOG(VIDEO, "Bad Z compare mode %i", bpmem.zmode.func);
		}

		if (pass && bpmem.zmode.updateenable)
		{
			SetPixelDepth(offset, z);
		}

		return pass;
	}
}
