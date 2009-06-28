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

#ifndef _BPMEMORY_H
#define _BPMEMORY_H

#include "Common.h"

#pragma pack(4)

#define BPMEM_GENMODE          0x00
#define BPMEM_DISPLAYCOPYFILER 0x01 // 0x01 + 4
#define BPMEM_IND_MTXA         0x06 // 0x06 + (3 * 3)
#define BPMEM_IND_MTXB         0x07 // 0x07 + (3 * 3)
#define BPMEM_IND_MTXC         0x08 // 0x08 + (3 * 3)
#define BPMEM_IND_IMASK        0x0F
#define BPMEM_IND_CMD          0x10 // 0x10 + 16
#define BPMEM_SCISSORTL        0x20
#define BPMEM_SCISSORBR        0x21
#define BPMEM_LINEPTWIDTH      0x22
#define BPMEM_SU_COUNTER       0x23
#define BPMEM_RAS_COUNTER      0x24
#define BPMEM_RAS1_SS0         0x25 
#define BPMEM_RAS1_SS1         0x26
#define BPMEM_IREF             0x27
#define BPMEM_TREF             0x28 // 0x28 + 8
#define BPMEM_SU_SSIZE         0x30 // 0x30 + (2 * 8)
#define BPMEM_SU_TSIZE         0x31 // 0x31 + (2 * 8)
#define BPMEM_ZMODE            0x40
#define BPMEM_BLENDMODE        0x41
#define BPMEM_CONSTANTALPHA    0x42
#define BPMEM_ZCOMPARE         0x43
#define BPMEM_FIELDMASK        0x44
#define BPMEM_SETDRAWDONE      0x45
#define BPMEM_CLOCK0           0x46
#define BPMEM_PE_TOKEN_ID	   0x47
#define BPMEM_PE_TOKEN_INT_ID  0x48
#define BPMEM_EFB_TL           0x49
#define BPMEM_EFB_BR           0x4A
#define BPMEM_EFB_ADDR         0x4B
#define BPMEM_MIPMAP_STRIDE    0x4D
#define BPMEM_COPYYSCALE       0x4E
#define BPMEM_CLEAR_AR         0x4F
#define BPMEM_CLEAR_GB         0x50
#define BPMEM_CLEAR_Z          0x51
#define BPMEM_TRIGGER_EFB_COPY 0x52
#define BPMEM_COPYFILTER0      0x53
#define BPMEM_COPYFILTER1      0x54
#define BPMEM_CLEARBBOX1       0x55 
#define BPMEM_CLEARBBOX2       0x56
// what about 0x57?
#define BPMEM_UNKNOWN          0x58
#define BPMEM_SCISSOROFFSET    0x59
#define BPMEM_UNKNOWN1         0x60
#define BPMEM_UNKNOWN2         0x61
#define BPMEM_UNKNOWN3         0x62
#define BPMEM_UNKNOWN4         0x63
#define BPMEM_LOADTLUT0        0x64
#define BPMEM_LOADTLUT1        0x65
#define BPMEM_TEXINVALIDATE    0x66
#define BPMEM_SETGPMETRIC      0x67
#define BPMEM_FIELDMODE        0x68
#define BPMEM_CLOCK1           0x69
#define BPMEM_TX_SETMODE0      0x80 // 0x80 + 4
#define BPMEM_TX_SETMODE1      0x84 // 0x84 + 4
#define BPMEM_TX_SETIMAGE0     0x88 // 0x88 + 4
#define BPMEM_TX_SETIMAGE1     0x8C // 0x8C + 4
#define BPMEM_TX_SETIMAGE2     0x90 // 0x90 + 4
#define BPMEM_TX_SETIMAGE3     0x94 // 0x94 + 4
#define BPMEM_TX_SETTLUT       0x98 // 0x98 + 4
#define BPMEM_TX_SETMODE0_4    0xA0 // 0xA0 + 4
#define BPMEM_TX_SETMODE1_4    0xA4 // 0xA4 + 4
#define BPMEM_TX_SETIMAGE0_4   0xA8 // 0xA8 + 4
#define BPMEM_TX_SETIMAGE1_4   0xAC // 0xA4 + 4
#define BPMEM_TX_SETIMAGE2_4   0xB0 // 0xB0 + 4
#define BPMEM_TX_SETIMAGE3_4   0xB4 // 0xB4 + 4
#define BPMEM_TX_SETLUT_4      0xB8 // 0xB8 + 4
#define BPMEM_TEV_COLOR_ENV    0xC0 // 0xC0 + (2 * 16)
#define BPMEM_TEV_ALPHA_ENV    0xC1 // 0xC1 + (2 * 16)
#define BPMEM_TEV_REGISTER_L   0xE0 // 0xE0 + (2 * 4)
#define BPMEM_TEV_REGISTER_H   0xE1 // 0xE1 + (2 * 4)
#define BPMEM_FOGRANGE         0xE8
#define BPMEM_FOGPARAM0        0xEE
#define BPMEM_FOGBMAGNITUDE    0xEF
#define BPMEM_FOGBEXPONENT     0xF0
#define BPMEM_FOGPARAM3        0xF1
#define BPMEM_FOGCOLOR         0xF2
#define BPMEM_ALPHACOMPARE     0xF3
#define BPMEM_BIAS			   0xF4
#define BPMEM_ZTEX2			   0xF5
#define BPMEM_TEV_KSEL         0xF6 // 0xF6 + 8
#define BPMEM_BP_MASK          0xFE

//////////////////////////////////////////////////////////////////////////
// Tev/combiner things
//////////////////////////////////////////////////////////////////////////

#define TEVOP_ADD 0
#define TEVOP_SUB 1
#define TEVCMP_R8_GT 8
#define TEVCMP_R8_EQ 9
#define TEVCMP_GR16_GT 10
#define TEVCMP_GR16_EQ 11
#define TEVCMP_BGR24_GT 12
#define TEVCMP_BGR24_EQ 13
#define TEVCMP_RGB8_GT  14
#define TEVCMP_RGB8_EQ  15
#define TEVCMP_A8_GT 14
#define TEVCMP_A8_EQ 15

#define TEVCOLORARG_CPREV 0
#define TEVCOLORARG_APREV 1
#define TEVCOLORARG_C0 2
#define TEVCOLORARG_A0 3
#define TEVCOLORARG_C1 4
#define TEVCOLORARG_A1 5
#define TEVCOLORARG_C2 6
#define TEVCOLORARG_A2 7
#define TEVCOLORARG_TEXC 8
#define TEVCOLORARG_TEXA 9
#define TEVCOLORARG_RASC 10
#define TEVCOLORARG_RASA 11
#define TEVCOLORARG_ONE 12
#define TEVCOLORARG_HALF 13
#define TEVCOLORARG_KONST 14
#define TEVCOLORARG_ZERO 15

#define TEVALPHAARG_APREV 0
#define TEVALPHAARG_A0 1
#define TEVALPHAARG_A1 2
#define TEVALPHAARG_A2 3
#define TEVALPHAARG_TEXA 4
#define TEVALPHAARG_RASA 5
#define TEVALPHAARG_KONST 6
#define TEVALPHAARG_ZERO 7

#define ALPHACMP_NEVER 0
#define ALPHACMP_LESS 1
#define ALPHACMP_EQUAL 2
#define ALPHACMP_LEQUAL 3
#define ALPHACMP_GREATER 4
#define ALPHACMP_NEQUAL 5
#define ALPHACMP_GEQUAL 6
#define ALPHACMP_ALWAYS 7

enum Compare
{
	COMPARE_NEVER = 0,
	COMPARE_LESS,
	COMPARE_EQUAL,
	COMPARE_LEQUAL,
	COMPARE_GREATER,
	COMPARE_NEQUAL,
	COMPARE_GEQUAL,
	COMPARE_ALWAYS
};

#define ZTEXTURE_DISABLE 0
#define ZTEXTURE_ADD 1
#define ZTEXTURE_REPLACE 2

enum TevBias
{
    TB_ZERO     = 0,
    TB_ADDHALF  = 1,
    TB_SUBHALF  = 2,
	TB_COMPARE  = 3,
};

enum AlphaOp
{
	ALPHAOP_AND = 0, 
	ALPHAOP_OR,
	ALPHAOP_XOR,
	ALPHAOP_XNOR,
};

union IND_MTXA
{
    struct
    {
        signed ma : 11;
        signed mb : 11;
        unsigned s0 : 2; // bits 0-1 of scale factor
        unsigned rid : 8;
    };
    u32 hex;
};

union IND_MTXB
{
    struct
    {
        signed mc : 11;
        signed md : 11;
        unsigned s1 : 2; // bits 2-3 of scale factor
        unsigned rid : 8;
    };
    u32 hex;
};

union IND_MTXC
{
    struct
    {
        signed me : 11;
        signed mf : 11;
        unsigned s2 : 2; // bits 4-5 of scale factor
        unsigned rid : 8;
    };
    u32 hex;
};

struct IND_MTX
{
    IND_MTXA col0;
    IND_MTXB col1;
    IND_MTXC col2;
};

union IND_IMASK
{
    struct
    {
        unsigned mask : 24;
        unsigned rid : 8;
    };
    u32 hex;
};

#define TEVSELCC_CPREV 0
#define TEVSELCC_APREV 1
#define TEVSELCC_C0 2
#define TEVSELCC_A0 3
#define TEVSELCC_C1 4
#define TEVSELCC_A1 5
#define TEVSELCC_C2 6
#define TEVSELCC_A2 7
#define TEVSELCC_TEXC 8
#define TEVSELCC_TEXA 9
#define TEVSELCC_RASC 10
#define TEVSELCC_RASA 11
#define TEVSELCC_ONE 12
#define TEVSELCC_HALF 13
#define TEVSELCC_KONST 14
#define TEVSELCC_ZERO 15

#define TEVSELCA_APREV 0
#define TEVSELCA_A0 1
#define TEVSELCA_A1 2
#define TEVSELCA_A2 3
#define TEVSELCA_TEXA 4
#define TEVSELCA_RASA 5
#define TEVSELCA_KONST 6
#define TEVSELCA_ZERO 7

struct TevStageCombiner
{
    union ColorCombiner
    {
        struct  //abc=8bit,d=10bit
        {
            unsigned d : 4; // TEVSELCC_X
            unsigned c : 4; // TEVSELCC_X
            unsigned b : 4; // TEVSELCC_X
            unsigned a : 4; // TEVSELCC_X

            unsigned bias : 2;
            unsigned op : 1;
            unsigned clamp : 1;

            unsigned shift : 2;
            unsigned dest : 2;  //1,2,3

        };
        u32 hex;
    };
    union AlphaCombiner
    {
        struct 
        {
            unsigned rswap : 2;
            unsigned tswap : 2;
            unsigned d : 3; // TEVSELCA_
            unsigned c : 3; // TEVSELCA_
            unsigned b : 3; // TEVSELCA_
            unsigned a : 3; // TEVSELCA_

            unsigned bias : 2; //GXTevBias
            unsigned op : 1;
            unsigned clamp : 1;

            unsigned shift : 2;
            unsigned dest : 2;  //1,2,3
        };
        u32 hex;
    };

    ColorCombiner colorC;
    AlphaCombiner alphaC;
};

#define ITF_8 0
#define ITF_5 1
#define ITF_4 2
#define ITF_3 3

#define ITB_NONE 0
#define ITB_S 1
#define ITB_T 2
#define ITB_ST 3
#define ITB_U 4
#define ITB_SU 5
#define ITB_TU 6
#define ITB_STU 7

#define ITBA_OFF 0
#define ITBA_S 1
#define ITBA_T 2
#define ITBA_U 3

#define ITW_OFF 0
#define ITW_256 1
#define ITW_128 2
#define ITW_64 3
#define ITW_32 4
#define ITW_16 5
#define ITW_0 6

// several discoveries:
// GXSetTevIndBumpST(tevstage, indstage, matrixind)
//  if( matrix == 2 ) realmat = 6; // 10
//  else if( matrix == 3 ) realmat = 7; // 11
//  else if( matrix == 1 ) realmat = 5; // 9
//  GXSetTevIndirect(tevstage, indstage, 0, 3, realmat, 6, 6, 0, 0, 0)
//  GXSetTevIndirect(tevstage+1, indstage, 0, 3, realmat+4, 6, 6, 1, 0, 0)
//  GXSetTevIndirect(tevstage+2, indstage, 0, 0, 0, 0, 0, 1, 0, 0)

union TevStageIndirect
{
    // if mid, sw, tw, and addprev are 0, then no indirect stage is used, mask = 0x17fe00
    struct
    {
        unsigned bt        : 2; // indirect tex stage ID
        unsigned fmt       : 2; // format: ITF_X
        unsigned bias      : 3; // ITB_X
        unsigned bs        : 2; // ITBA_X, indicates which coordinate will become the 'bump alpha'
        unsigned mid       : 4; // matrix id to multiply offsets with
        unsigned sw        : 3; // ITW_X, wrapping factor for S of regular coord
        unsigned tw        : 3; // ITW_X, wrapping factor for T of regular coord
        unsigned lb_utclod : 1; // use modified or unmodified texture coordinates for LOD computation
        unsigned fb_addprev : 1; // 1 if the texture coordinate results from the previous TEV stage should be added
        unsigned pad0 : 3;
        unsigned rid : 8;
    };
    struct
    {
        u32 hex : 21; 
        u32 unused : 11;
    };

    bool IsActive() { return (hex&0x17fe00)!=0; }
};

union TwoTevStageOrders
{
    struct 
    {
        unsigned texmap0    : 3; // indirect tex stage texmap
        unsigned texcoord0  : 3;
        unsigned enable0    : 1; // 1 if should read from texture
        unsigned colorchan0 : 3; // RAS1_CC_X

        unsigned pad0       : 2;

        unsigned texmap1    : 3;
        unsigned texcoord1  : 3;
        unsigned enable1    : 1; // 1 if should read from texture
        unsigned colorchan1 : 3; // RAS1_CC_X

        unsigned pad1       : 2;
        unsigned rid        : 8;
    };
    u32 hex;
    int getTexMap(int i){return i?texmap1:texmap0;}
    int getTexCoord(int i){return i?texcoord1:texcoord0;}
    int getEnable(int i){return i?enable1:enable0;}
    int getColorChan(int i){return i?colorchan1:colorchan0;}
};

union TEXSCALE
{
    struct
    {
        unsigned ss0 : 4; // indirect tex stage 0, 2^(-ss0)
        unsigned ts0 : 4; // indirect tex stage 0
        unsigned ss1 : 4; // indirect tex stage 1
        unsigned ts1 : 4; // indirect tex stage 1
        unsigned pad : 8;
        unsigned rid : 8;
    };
    u32 hex;

    float getScaleS(int i){return 1.0f/(float)(1<<(i?ss1:ss0));}
    float getScaleT(int i){return 1.0f/(float)(1<<(i?ts1:ts0));}
};

union RAS1_IREF
{
    struct
    {
        unsigned bi0 : 3; // indirect tex stage 0 ntexmap
        unsigned bc0 : 3; // indirect tex stage 0 ntexcoord
        unsigned bi1 : 3;
        unsigned bc1 : 3;
        unsigned bi2 : 3;
        unsigned bc3 : 3;
        unsigned bi4 : 3;
        unsigned bc4 : 3;
        unsigned rid : 8;
    };
    u32 hex;

    u32 getTexCoord(int i) { return (hex>>(6*i+3))&3; }
    u32 getTexMap(int i) { return (hex>>(6*i))&3; }
};

//////////////////////////////////////////////////////////////////////////
// Texture structs
//////////////////////////////////////////////////////////////////////////
union TexMode0
{
    struct 
    {
        unsigned wrap_s : 2;
        unsigned wrap_t : 2;
        unsigned mag_filter : 1;
        unsigned min_filter : 3;
        unsigned diag_lod : 1;
        signed lod_bias : 10;
        unsigned max_aniso : 2;
        unsigned lod_clamp : 1;
    };
    u32 hex;
};
union TexMode1
{
    struct 
    {
        unsigned min_lod : 8;
        unsigned max_lod : 8;
    };
    u32 hex;
};
union TexImage0
{
    struct 
    {
        unsigned width : 10; //actually w-1
        unsigned height : 10; //actually h-1
        unsigned format : 4;
    };
    u32 hex;
};
union TexImage1
{
    struct 
    {
        unsigned tmem_offset : 15; // we ignore texture caching for now, we do it ourselves
        unsigned cache_width : 3; 
        unsigned cache_height : 3;
        unsigned image_type : 1;
    };
    u32 hex;
};

union TexImage2
{
    struct 
    {
        unsigned tmem_offset : 15; // we ignore texture caching for now, we do it ourselves
        unsigned cache_width : 3; 
        unsigned cache_height : 3;
    };
    u32 hex;
};

union TexImage3
{
    struct 
    {
        unsigned image_base: 24;  //address in memory >> 5 (was 20 for GC)
    };
    u32 hex;
};
union TexTLUT
{
    struct 
    {	
        unsigned tmem_offset : 10;
        unsigned tlut_format : 2;
    };
    u32 hex;
};

union ZTex1
{
    struct 
    {
        unsigned bias : 24;
    };
    u32 hex;
};

union ZTex2
{
    struct 
    {
        unsigned type : 2; // TEV_Z_TYPE_X
        unsigned op : 2; // GXZTexOp
    };
    u32 hex;
};

//  Z-texture types (formats)
#define TEV_ZTEX_TYPE_U8	0
#define TEV_ZTEX_TYPE_U16	1
#define TEV_ZTEX_TYPE_U24	2

#define TEV_ZTEX_DISABLE  0
#define TEV_ZTEX_ADD      1
#define TEV_ZTEX_REPLACE  2


struct FourTexUnits
{
    TexMode0 texMode0[4]; 
    TexMode1 texMode1[4]; 
    TexImage0 texImage0[4]; 
    TexImage1 texImage1[4]; 
    TexImage2 texImage2[4]; 
    TexImage3 texImage3[4]; 
    TexTLUT texTlut[4];   
    u32 unknown[4];   
};


//////////////////////////////////////////////////////////////////////////
// Geometry/other structs
//////////////////////////////////////////////////////////////////////////
union GenMode
{
    struct 
    {
        unsigned numtexgens : 4;    //     0xF
        unsigned numcolchans : 5;   //   0x1E0
        unsigned ms_en : 1;         //   0x200
        unsigned numtevstages : 4;  //  0x3C00
        unsigned cullmode : 2;      //  0xC000
        unsigned numindstages : 3;  // 0x30000
        unsigned zfreeze : 5;       //0x3C0000
    };
    u32 hex;
};

union LPSize
{
    struct 
    {
        unsigned linesize : 8; // in 1/6th pixels
        unsigned pointsize : 8; // in 1/6th pixels
        unsigned lineoff : 3;
        unsigned pointoff : 3;
        unsigned lineaspect : 1;
        unsigned padding : 1;
    };
    u32 hex;
};


union X12Y12
{
    struct 
    {
        unsigned y : 12;
        unsigned x : 12;
    };
    u32 hex;
};
union X10Y10
{
    struct 
    {
        unsigned x : 10;
        unsigned y : 10;
    };
    u32 hex;
};

//////////////////////////////////////////////////////////////////////////
// Framebuffer/pixel stuff (incl fog)
//////////////////////////////////////////////////////////////////////////
union BlendMode
{
    struct 
    {
        unsigned blendenable   : 1;
        unsigned logicopenable : 1;
        unsigned dither : 1;
        unsigned colorupdate : 1;
        unsigned alphaupdate : 1;
        unsigned dstfactor : 3; //BLEND_ONE, BLEND_INV_SRc etc
        unsigned srcfactor : 3;
        unsigned subtract : 1;
        unsigned logicmode : 4;
    };
    u32 hex;
};


union FogParam0
{
    struct 
    {
        unsigned mantissa : 11;
        unsigned exponent : 8;
        unsigned sign : 1;
    };

    float GetA() {
		union { u32 i; float f; } dummy;
        dummy.i = ((u32)sign<<31)|((u32)exponent<<23)|((u32)mantissa<<12);
		return dummy.f;
	}

    u32 hex;
};

union FogParam3
{
    struct
    {
        unsigned c_mant : 11;
        unsigned c_exp : 8;
        unsigned c_sign : 1;
        unsigned proj : 1; // 0 - perspective, 1 - orthographic
        unsigned fsel : 3; // 0 - off, 2 - linear, 4 - exp, 5 - exp2, 6 - backward exp, 7 - backward exp2
    };

    // amount to subtract from eyespacez after range adjustment
    float GetC() { 
		union { u32 i; float f; } dummy;
		dummy.i = ((u32)c_sign << 31) | ((u32)c_exp << 23) | ((u32)c_mant << 12);
		return dummy.f;
	}

    u32 hex;
};

// final eq: ze = A/(B_MAG - (Zs>>B_SHF));
struct FogParams
{
    FogParam0 a;
    u32 b_magnitude;
    u32 b_shift; // b's exp + 1?
    FogParam3 c_proj_fsel;

    union FogColor
    {
        struct
        {
            unsigned b  : 8;
            unsigned g  : 8;
            unsigned r  : 8;
        };
        u32 hex;
    };

    FogColor color;  //0:b 8:g 16:r - nice!
};

union ZMode
{
    struct 
    {
        unsigned testenable		: 1;
        unsigned func			: 3;
        unsigned updateenable	: 1;  //size?
    };
    u32 hex;
};

union ConstantAlpha
{
    struct 
    {
        unsigned alpha : 8;
        unsigned enable : 1;
    };
    u32 hex;
};

#define PIXELFMT_RGB8_Z24 0
#define PIXELFMT_RGBA6_Z24 1
#define PIXELFMT_RGB565_Z16 2
#define PIXELFMT_Z24 3
#define PIXELFMT_Y8 4
#define PIXELFMT_U8 5
#define PIXELFMT_V8 6
#define PIXELFMT_YUV420 7

union PE_CONTROL
{
    struct
    {
        unsigned pixel_format : 3; // PIXELFMT_X
        unsigned zformat : 3; // 0 - linear, 1 - near, 2 - mid, 3 - far
        unsigned zcomploc : 1; // 1: before tex stage
        unsigned unused : 17;
        unsigned rid : 8;
    };
    u32 hex;
};

//////////////////////////////////////////////////////////////////////////
// Texture coordinate stuff
//////////////////////////////////////////////////////////////////////////
union TCInfo
{
    struct 
    {
        unsigned scale_minus_1 : 16;
        unsigned range_bias : 1;
        unsigned cylindric_wrap : 1;
    };
    u32 hex;
};
struct TCoordInfo
{
    TCInfo s;
    TCInfo t;
};


union ColReg
{
    u32 hex;
    struct
    {
        signed a : 11;
        unsigned : 1;
        signed b : 11;
        unsigned type : 1;
    };
};

struct TevReg
{
    ColReg low;
    ColReg high;
};

union TevKSel
{
    struct {
        unsigned swap1 : 2;
        unsigned swap2 : 2;
        unsigned kcsel0 : 5;
        unsigned kasel0 : 5;
        unsigned kcsel1 : 5;
        unsigned kasel1 : 5;
    };
    u32 hex;

    int getKC(int i) {return i?kcsel1:kcsel0;}
    int getKA(int i) {return i?kasel1:kasel0;}
};

union AlphaFunc
{
    struct
    {
        unsigned ref0 : 8;
        unsigned ref1 : 8;
        unsigned comp0 : 3;
        unsigned comp1 : 3;
        unsigned logic : 2;
    };
    u32 hex;
};

union UPE_Copy
{
    u32 Hex;
    struct 
    {
        unsigned clamp0					: 1;
        unsigned clamp1					: 1;
        unsigned                        : 1;
        unsigned target_pixel_format	: 4; // realformat is (fmt/2)+((fmt&1)*8).... for some reason the msb is the lsb
        unsigned gamma					: 2;
        unsigned half_scale             : 1; // real size should be 2x smaller (run a gauss filter?)
        unsigned scale_something		: 1;
        unsigned clear					: 1;
        unsigned frame_to_field			: 2;
        unsigned copy_to_xfb			: 1;
        unsigned intensity_fmt          : 1; // if set, is an intensity format (I4,I8,IA4,IA8)
        unsigned						: 16; // seems to set everything to 1s when target pixel format is invalid
    };
};

//////////////////////////////////////////////////////////////////////////
// All of BP memory
//////////////////////////////////////////////////////////////////////////

struct Bypass
{
	int address;
	int changes;
	int newvalue;
};

struct BPMemory
{
    GenMode genMode;
    u32 display_copy_filter[4]; // 01-04
    u32 unknown; // 05
    // indirect matrices (set by GXSetIndTexMtx, selected by TevStageIndirect::mid)
    // abc form a 2x3 offset matrix, there's 3 such matrices
    // the 3 offset matrices can either be indirect type, S-type, or T-type
    // 6bit scale factor s is distributed across IND_MTXA/B/C. 
    // before using matrices scale by 2^-(s-17)
    IND_MTX indmtx[3];//06-0e GXSetIndTexMtx, 2x3 matrices
    IND_IMASK imask;//0f
    TevStageIndirect tevind[16];//10 GXSetTevIndirect
    X12Y12 scissorTL; //20
    X12Y12 scissorBR; //21
    LPSize lineptwidth; //22 line and point width
    u32 sucounter; //23
    u32 rascounter; //24
    TEXSCALE texscale[2]; //25-26 GXSetIndTexCoordScale
    RAS1_IREF tevindref; //27 GXSetIndTexOrder
    TwoTevStageOrders tevorders[8]; //28-2F
    TCoordInfo texcoords[8]; //0x30 s,t,s,t,s,t,s,t...
    ZMode zmode; //40
    BlendMode blendmode; //41
    ConstantAlpha dstalpha;  //42
    PE_CONTROL zcontrol; //43 GXSetZCompLoc, GXPixModeSync
    u32 fieldmask; //44
    u32 drawdone;  //45, bit1=1 if end of list
    u32 unknown5;  //46 clock?
    u32 petoken; //47
    u32 petokenint; // 48
    X10Y10 copyTexSrcXY; // 49
    X10Y10 copyTexSrcWH; // 4a
    u32 copyTexDest; //4b// 4b == CopyAddress (GXDispCopy and GXTexCopy use it)
    u32 unknown6; //4c
    u32 copyMipMapStrideChannels; // 4d usually set to 4 when dest is single channel, 8 when dest is 2 channel, 16 when dest is RGBA
                                // also, doubles whenever mipmap box filter option is set (excent on RGBA). Probably to do with number of bytes to look at when smoothing
    u32 dispcopyyscale; //4e
    u32 clearcolorAR; //4f
    u32 clearcolorGB; //50
    u32 clearZValue; //51
    u32 triggerEFBCopy; //52
    u32 copyfilter[2]; //53,54
    u32 boundbox0;//55
    u32 boundbox1;//56
    u32 unknown7[2];//57,58
    X10Y10 scissorOffset; //59
    u32 unknown8[10]; //5a,5b,5c,5d, 5e,5f,60,61, 62,   63 (GXTexModeSync), 0x60-0x63 have to do with preloaded textures?
    u32 tlutXferSrc; //64
    u32 tlutXferDest; //65
    u32 texinvalidate;//66
    u32 metric; //67
    u32 fieldmode;//68
    u32 unknown10[7];//69-6F
    u32 unknown11[16];//70-7F
    FourTexUnits tex[2]; //80-bf
    TevStageCombiner combiners[16]; //0xC0-0xDF
    TevReg tevregs[4];  //0xE0
    u32 fogRangeAdj;   //0xE8
    u32 unknown15[3];  //0xe9,0xea,0xeb - fog related
    u32 tev_range_adj_c; //0xec - screenx center for range adjustment, range adjustment enable
    u32 tev_range_adj_k; //0xed - specifies range adjustment function = sqrt(x*x+k*k)/k
    FogParams fog; //0xEE,0xEF,0xF0,0xF1,0xF2
    AlphaFunc alphaFunc; //0xF3
    ZTex1 ztex1; //0xf4,0xf5
    ZTex2 ztex2;
    TevKSel tevksel[8];//0xf6,0xf7,f8,f9,fa,fb,fc,fd
    u32 bpMask; //0xFE
    u32 unknown18; //ff
};

#pragma pack()

extern BPMemory bpmem;

void LoadBPReg(u32 value0);

#endif
