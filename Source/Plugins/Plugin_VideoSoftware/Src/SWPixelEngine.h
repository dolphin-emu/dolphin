// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _PIXELENGINE_H
#define _PIXELENGINE_H

#include "Common.h"
#include "VideoCommon.h"

class PointerWrap;

namespace SWPixelEngine
{
	// internal hardware addresses
	enum
	{
		PE_ZCONF		 = 0x000, // Z Config
		PE_ALPHACONF	 = 0x002, // Alpha Config
		PE_DSTALPHACONF  = 0x004, // Destination Alpha Config
		PE_ALPHAMODE	 = 0x006, // Alpha Mode Config
		PE_ALPHAREAD	 = 0x008, // Alpha Read
		PE_CTRL_REGISTER = 0x00a, // Control
		PE_TOKEN_REG	 = 0x00e, // Token
		PE_BBOX_LEFT	 = 0x010, // Flip Left
		PE_BBOX_RIGHT	 = 0x012, // Flip Right
		PE_BBOX_TOP		 = 0x014, // Flip Top
		PE_BBOX_BOTTOM	 = 0x016, // Flip Bottom

		// NOTE: Order not verified
		// These indicate the number of quads that are being used as input/output for each particular stage
		PE_PERF_ZCOMP_INPUT_ZCOMPLOC_L  = 0x18,
		PE_PERF_ZCOMP_INPUT_ZCOMPLOC_H  = 0x1a,
		PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_L = 0x1c,
		PE_PERF_ZCOMP_OUTPUT_ZCOMPLOC_H = 0x1e,
		PE_PERF_ZCOMP_INPUT_L           = 0x20,
		PE_PERF_ZCOMP_INPUT_H           = 0x22,
		PE_PERF_ZCOMP_OUTPUT_L          = 0x24,
		PE_PERF_ZCOMP_OUTPUT_H          = 0x26,
		PE_PERF_BLEND_INPUT_L           = 0x28,
		PE_PERF_BLEND_INPUT_H           = 0x2a,
		PE_PERF_EFB_COPY_CLOCKS_L       = 0x2c,
		PE_PERF_EFB_COPY_CLOCKS_H       = 0x2e,
	};

	union UPEZConfReg
	{
		u16 Hex;
		struct 
		{
			u16 ZCompEnable		: 1; // Z Comparator Enable
			u16 Function		: 3;
			u16 ZUpdEnable		: 1;
			u16					: 11;
		};
	};

	union UPEAlphaConfReg
	{
		u16 Hex;
		struct 
		{
			u16 BMMath			: 1; // GX_BM_BLEND || GX_BM_SUBSTRACT
			u16 BMLogic			: 1; // GX_BM_LOGIC
			u16 Dither			: 1;
			u16 ColorUpdEnable	: 1;
			u16 AlphaUpdEnable	: 1;
			u16 DstFactor		: 3;
			u16 SrcFactor		: 3;
			u16 Substract		: 1; // Additive mode by default
			u16 BlendOperator	: 4;
		};
	};

	union UPEDstAlphaConfReg
	{
		u16 Hex;
		struct 
		{
			u16 DstAlpha		: 8;
			u16 Enable			: 1;
			u16					: 7;
		};
	};

	union UPEAlphaModeConfReg
	{
		u16 Hex;
		struct 
		{
			u16 Threshold		: 8;
			u16 CompareMode		: 8;
		};
	};

		union UPEAlphaReadReg
	{
		u16 Hex;
		struct 
		{
			u16 ReadMode	: 3;
			u16				: 13;
		};
	};

		union UPECtrlReg
	{
		struct 
		{
			u16 PETokenEnable	:	1;
			u16 PEFinishEnable	:	1;
			u16 PEToken			:	1; // write only
			u16 PEFinish		:	1; // write only
			u16					:	12;
		};
		u16 Hex;
		UPECtrlReg() {Hex = 0; }
		UPECtrlReg(u16 _hex) {Hex = _hex; }
	};

	struct PEReg
	{
		UPEZConfReg zconf;
		UPEAlphaConfReg alphaConf;
		UPEDstAlphaConfReg dstAlpha;
		UPEAlphaModeConfReg alphaMode;
		UPEAlphaReadReg alphaRead;
		UPECtrlReg ctrl;
		u16 unk0;
		u16 token;

		u16 boxLeft;
		u16 boxRight;
		u16 boxTop;
		u16 boxBottom;

		u16 perfZcompInputZcomplocLo;
		u16 perfZcompInputZcomplocHi;
		u16 perfZcompOutputZcomplocLo;
		u16 perfZcompOutputZcomplocHi;
		u16 perfZcompInputLo;
		u16 perfZcompInputHi;
		u16 perfZcompOutputLo;
		u16 perfZcompOutputHi;
		u16 perfBlendInputLo;
		u16 perfBlendInputHi;
		u16 perfEfbCopyClocksLo;
		u16 perfEfbCopyClocksHi;

		// NOTE: hardware doesn't process individual pixels but quads instead. Current software renderer architecture works on pixels though, so we have this "quad" hack here to only increment the registers on every fourth rendered pixel
		void IncZInputQuadCount(bool early_ztest)
		{
			static int quad = 0;
			if (++quad != 3)
				return;
			quad = 0;

			if (early_ztest)
			{
				if (++perfZcompInputZcomplocLo == 0)
					perfZcompInputZcomplocHi++;
			}
			else
			{
				if (++perfZcompInputLo == 0)
					perfZcompInputHi++;
			}
		}
		void IncZOutputQuadCount(bool early_ztest)
		{
			static int quad = 0;
			if (++quad != 3)
				return;
			quad = 0;

			if (early_ztest)
			{
				if (++perfZcompOutputZcomplocLo == 0)
					perfZcompOutputZcomplocHi++;
			}
			else
			{
				if (++perfZcompOutputLo == 0)
					perfZcompOutputHi++;
			}
		}
		void IncBlendInputQuadCount()
		{
			static int quad = 0;
			if (++quad != 3)
				return;
			quad = 0;

			if (++perfBlendInputLo == 0)
				perfBlendInputHi++;
		}
	};

	extern PEReg pereg;

	void Init();
	void DoState(PointerWrap &p);

	// Read
	void Read16(u16& _uReturnValue, const u32 _iAddress);

	// Write
	void Write16(const u16 _iValue, const u32 _iAddress);
	void Write32(const u32 _iValue, const u32 _iAddress);

	// gfx backend support
	void SetToken(const u16 _token, const int _bSetTokenAcknowledge);
	void SetFinish(void);
	bool AllowIdleSkipping();

} // end of namespace SWPixelEngine

#endif
