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
#ifndef _PIXELENGINE_H
#define _PIXELENGINE_H

#include "Common.h"
#include "pluginspecs_video.h"
#include "VideoCommon.h"
class PointerWrap;

namespace PixelEngine
{
    // internal hardware addresses
    enum
    {
	    PE_ZCONF         = 0x000, // Z Config
	    PE_ALPHACONF     = 0x002, // Alpha Config
	    PE_DSTALPHACONF  = 0x004, // Destination Alpha Config
	    PE_ALPHAMODE     = 0x006, // Alpha Mode Config
	    PE_ALPHAREAD     = 0x008, // Alpha Read
	    PE_CTRL_REGISTER = 0x00a, // Control
	    PE_TOKEN_REG     = 0x00e, // Token
	    PE_BBOX_LEFT	 = 0x010, // Flip Left
	    PE_BBOX_RIGHT	 = 0x012, // Flip Right
	    PE_BBOX_TOP		 = 0x014, // Flip Top
	    PE_BBOX_BOTTOM	 = 0x016, // Flip Bottom
    };

    union UPEZConfReg
    {
	    u16 Hex;
	    struct 
	    {
		    u16 ZCompEnable	    : 1; // Z Comparator Enable
		    u16 Function		: 3;
		    u16 ZUpdEnable		: 1;
		    u16				    : 11;
	    };
    };

    union UPEAlphaConfReg
    {
	    u16 Hex;
	    struct 
	    {
		    u16 BMMath			: 1; // GX_BM_BLEND || GX_BM_SUBSTRACT
		    u16 BMLogic		    : 1; // GX_BM_LOGIC
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
		    u16				    : 7;
	    };
    };

    union UPEAlphaModeConfReg
    {
	    u16 Hex;
	    struct 
	    {
		    u16 Threshold		: 8;
		    u16 CompareMode	    : 8;
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
		    u16 PEToken		:	1; // write only
		    u16 PEFinish		:	1; // write only
		    u16				:	12;
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
    };

    extern PEReg pereg;

    void Init();
    void DoState(PointerWrap &p);

    // Read
    void Read16(u16& _uReturnValue, const u32 _iAddress);

    // Write
    void Write16(const u16 _iValue, const u32 _iAddress);
    void Write32(const u32 _iValue, const u32 _iAddress);

    // gfx plugin support
    void SetToken(const u16 _token, const int _bSetTokenAcknowledge);
    void SetFinish(void);
    bool AllowIdleSkipping();    

} // end of namespace PixelEngine

#endif


