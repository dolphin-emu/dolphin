// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

#pragma pack(4)

#define BPMEM_GENMODE           0x00
#define BPMEM_DISPLAYCOPYFILTER 0x01 // 0x01 + 4
#define BPMEM_IND_MTXA          0x06 // 0x06 + (3 * 3)
#define BPMEM_IND_MTXB          0x07 // 0x07 + (3 * 3)
#define BPMEM_IND_MTXC          0x08 // 0x08 + (3 * 3)
#define BPMEM_IND_IMASK         0x0F
#define BPMEM_IND_CMD           0x10 // 0x10 + 16
#define BPMEM_SCISSORTL         0x20
#define BPMEM_SCISSORBR         0x21
#define BPMEM_LINEPTWIDTH       0x22
#define BPMEM_PERF0_TRI         0x23
#define BPMEM_PERF0_QUAD        0x24
#define BPMEM_RAS1_SS0          0x25
#define BPMEM_RAS1_SS1          0x26
#define BPMEM_IREF              0x27
#define BPMEM_TREF              0x28 // 0x28 + 8
#define BPMEM_SU_SSIZE          0x30 // 0x30 + (2 * 8)
#define BPMEM_SU_TSIZE          0x31 // 0x31 + (2 * 8)
#define BPMEM_ZMODE             0x40
#define BPMEM_BLENDMODE         0x41
#define BPMEM_CONSTANTALPHA     0x42
#define BPMEM_ZCOMPARE          0x43
#define BPMEM_FIELDMASK         0x44
#define BPMEM_SETDRAWDONE       0x45
#define BPMEM_BUSCLOCK0         0x46
#define BPMEM_PE_TOKEN_ID       0x47
#define BPMEM_PE_TOKEN_INT_ID   0x48
#define BPMEM_EFB_TL            0x49
#define BPMEM_EFB_BR            0x4A
#define BPMEM_EFB_ADDR          0x4B
#define BPMEM_MIPMAP_STRIDE     0x4D
#define BPMEM_COPYYSCALE        0x4E
#define BPMEM_CLEAR_AR          0x4F
#define BPMEM_CLEAR_GB          0x50
#define BPMEM_CLEAR_Z           0x51
#define BPMEM_TRIGGER_EFB_COPY  0x52
#define BPMEM_COPYFILTER0       0x53
#define BPMEM_COPYFILTER1       0x54
#define BPMEM_CLEARBBOX1        0x55
#define BPMEM_CLEARBBOX2        0x56
#define BPMEM_CLEAR_PIXEL_PERF  0x57
#define BPMEM_REVBITS           0x58
#define BPMEM_SCISSOROFFSET     0x59
#define BPMEM_PRELOAD_ADDR      0x60
#define BPMEM_PRELOAD_TMEMEVEN  0x61
#define BPMEM_PRELOAD_TMEMODD   0x62
#define BPMEM_PRELOAD_MODE      0x63
#define BPMEM_LOADTLUT0         0x64
#define BPMEM_LOADTLUT1         0x65
#define BPMEM_TEXINVALIDATE     0x66
#define BPMEM_PERF1             0x67
#define BPMEM_FIELDMODE         0x68
#define BPMEM_BUSCLOCK1         0x69
#define BPMEM_TX_SETMODE0       0x80 // 0x80 + 4
#define BPMEM_TX_SETMODE1       0x84 // 0x84 + 4
#define BPMEM_TX_SETIMAGE0      0x88 // 0x88 + 4
#define BPMEM_TX_SETIMAGE1      0x8C // 0x8C + 4
#define BPMEM_TX_SETIMAGE2      0x90 // 0x90 + 4
#define BPMEM_TX_SETIMAGE3      0x94 // 0x94 + 4
#define BPMEM_TX_SETTLUT        0x98 // 0x98 + 4
#define BPMEM_TX_SETMODE0_4     0xA0 // 0xA0 + 4
#define BPMEM_TX_SETMODE1_4     0xA4 // 0xA4 + 4
#define BPMEM_TX_SETIMAGE0_4    0xA8 // 0xA8 + 4
#define BPMEM_TX_SETIMAGE1_4    0xAC // 0xA4 + 4
#define BPMEM_TX_SETIMAGE2_4    0xB0 // 0xB0 + 4
#define BPMEM_TX_SETIMAGE3_4    0xB4 // 0xB4 + 4
#define BPMEM_TX_SETTLUT_4      0xB8 // 0xB8 + 4
#define BPMEM_TEV_COLOR_ENV     0xC0 // 0xC0 + (2 * 16)
#define BPMEM_TEV_ALPHA_ENV     0xC1 // 0xC1 + (2 * 16)
#define BPMEM_TEV_COLOR_RA      0xE0 // 0xE0 + (2 * 4)
#define BPMEM_TEV_COLOR_BG      0xE1 // 0xE1 + (2 * 4)
#define BPMEM_FOGRANGE          0xE8 // 0xE8 + 6
#define BPMEM_FOGPARAM0         0xEE
#define BPMEM_FOGBMAGNITUDE     0xEF
#define BPMEM_FOGBEXPONENT      0xF0
#define BPMEM_FOGPARAM3         0xF1
#define BPMEM_FOGCOLOR          0xF2
#define BPMEM_ALPHACOMPARE      0xF3
#define BPMEM_BIAS              0xF4
#define BPMEM_ZTEX2             0xF5
#define BPMEM_TEV_KSEL          0xF6 // 0xF6 + 8
#define BPMEM_BP_MASK           0xFE


// Tev/combiner things

#define TEVSCALE_1  0
#define TEVSCALE_2  1
#define TEVSCALE_4  2
#define TEVDIVIDE_2 3

#define TEVCMP_R8    0
#define TEVCMP_GR16  1
#define TEVCMP_BGR24 2
#define TEVCMP_RGB8  3

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
#define TEVALPHAARG_A0    1
#define TEVALPHAARG_A1    2
#define TEVALPHAARG_A2    3
#define TEVALPHAARG_TEXA  4
#define TEVALPHAARG_RASA  5
#define TEVALPHAARG_KONST 6
#define TEVALPHAARG_ZERO  7

#define GX_TEVPREV       0
#define GX_TEVREG0       1
#define GX_TEVREG1       2
#define GX_TEVREG2       3

#define ZTEXTURE_DISABLE 0
#define ZTEXTURE_ADD 1
#define ZTEXTURE_REPLACE 2

#define TevBias_ZERO     0
#define TevBias_ADDHALF  1
#define TevBias_SUBHALF  2
#define TevBias_COMPARE  3

union IND_MTXA
{
	struct
	{
		s32 ma : 11;
		s32 mb : 11;
		u32 s0 : 2; // bits 0-1 of scale factor
		u32 rid : 8;
	};
	u32 hex;
};

union IND_MTXB
{
	struct
	{
		s32 mc : 11;
		s32 md : 11;
		u32 s1 : 2; // bits 2-3 of scale factor
		u32 rid : 8;
	};
	u32 hex;
};

union IND_MTXC
{
	struct
	{
		s32 me : 11;
		s32 mf : 11;
		u32 s2 : 2; // bits 4-5 of scale factor
		u32 rid : 8;
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
		u32 mask : 24;
		u32 rid : 8;
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
			u32 d : 4; // TEVSELCC_X
			u32 c : 4; // TEVSELCC_X
			u32 b : 4; // TEVSELCC_X
			u32 a : 4; // TEVSELCC_X

			u32 bias : 2;
			u32 op : 1;
			u32 clamp : 1;

			u32 shift : 2;
			u32 dest : 2;  //1,2,3

		};
		u32 hex;
	};
	union AlphaCombiner
	{
		struct
		{
			u32 rswap : 2;
			u32 tswap : 2;
			u32 d : 3; // TEVSELCA_
			u32 c : 3; // TEVSELCA_
			u32 b : 3; // TEVSELCA_
			u32 a : 3; // TEVSELCA_

			u32 bias : 2; //GXTevBias
			u32 op : 1;
			u32 clamp : 1;

			u32 shift : 2;
			u32 dest : 2;  //1,2,3
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
#define ITB_S    1
#define ITB_T    2
#define ITB_ST   3
#define ITB_U    4
#define ITB_SU   5
#define ITB_TU   6
#define ITB_STU  7

#define ITBA_OFF 0
#define ITBA_S   1
#define ITBA_T   2
#define ITBA_U   3

#define ITW_OFF 0
#define ITW_256 1
#define ITW_128 2
#define ITW_64  3
#define ITW_32  4
#define ITW_16  5
#define ITW_0   6

// several discoveries:
// GXSetTevIndBumpST(tevstage, indstage, matrixind)
//  if ( matrix == 2 ) realmat = 6; // 10
//  else if ( matrix == 3 ) realmat = 7; // 11
//  else if ( matrix == 1 ) realmat = 5; // 9
//  GXSetTevIndirect(tevstage, indstage, 0, 3, realmat, 6, 6, 0, 0, 0)
//  GXSetTevIndirect(tevstage+1, indstage, 0, 3, realmat+4, 6, 6, 1, 0, 0)
//  GXSetTevIndirect(tevstage+2, indstage, 0, 0, 0, 0, 0, 1, 0, 0)

	union TevStageIndirect
	{
		struct
		{
			u32 bt          : 2; // Indirect tex stage ID
			u32 fmt         : 2; // Format: ITF_X
			u32 bias        : 3; // ITB_X
			u32 bs          : 2; // ITBA_X, indicates which coordinate will become the 'bump alpha'
			u32 mid         : 4; // Matrix ID to multiply offsets with
			u32 sw          : 3; // ITW_X, wrapping factor for S of regular coord
			u32 tw          : 3; // ITW_X, wrapping factor for T of regular coord
			u32 lb_utclod   : 1; // Use modified or unmodified texture coordinates for LOD computation
			u32 fb_addprev  : 1; // 1 if the texture coordinate results from the previous TEV stage should be added
			u32 pad0        : 3;
			u32 rid         : 8;
		};
		struct
		{
			u32 hex : 21;
			u32 unused : 11;
		};

		// If bs and mid are zero, the result of the stage is independent of
		// the texture sample data, so we can skip sampling the texture.
		bool IsActive() { return bs != ITBA_OFF || mid != 0; }
	};

	union TwoTevStageOrders
	{
		struct
		{
			u32 texmap0    : 3; // Indirect tex stage texmap
			u32 texcoord0  : 3;
			u32 enable0    : 1; // 1 if should read from texture
			u32 colorchan0 : 3; // RAS1_CC_X

			u32 pad0       : 2;

			u32 texmap1    : 3;
			u32 texcoord1  : 3;
			u32 enable1    : 1; // 1 if should read from texture
			u32 colorchan1 : 3; // RAS1_CC_X

			u32 pad1       : 2;
			u32 rid        : 8;
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
		u32 ss0 : 4; // Indirect tex stage 0, 2^(-ss0)
		u32 ts0 : 4; // Indirect tex stage 0
		u32 ss1 : 4; // Indirect tex stage 1
		u32 ts1 : 4; // Indirect tex stage 1
		u32 pad : 8;
		u32 rid : 8;
	};
	u32 hex;
};

union RAS1_IREF
{
	struct
	{
		u32 bi0 : 3; // Indirect tex stage 0 ntexmap
		u32 bc0 : 3; // Indirect tex stage 0 ntexcoord
		u32 bi1 : 3;
		u32 bc1 : 3;
		u32 bi2 : 3;
		u32 bc3 : 3;
		u32 bi4 : 3;
		u32 bc4 : 3;
		u32 rid : 8;
	};
	u32 hex;

	u32 getTexCoord(int i) { return (hex>>(6*i+3))&7; }
	u32 getTexMap(int i) { return (hex>>(6*i))&7; }
};


// Texture structs

union TexMode0
{
	struct
	{
		u32 wrap_s     : 2;
		u32 wrap_t     : 2;
		u32 mag_filter : 1;
		u32 min_filter : 3;
		u32 diag_lod   : 1;
		s32 lod_bias   : 8;
		u32 pad0       : 2;
		u32 max_aniso  : 2;
		u32 lod_clamp  : 1;
	};
	u32 hex;
};
union TexMode1
{
	struct
	{
		u32 min_lod : 8;
		u32 max_lod : 8;
	};
	u32 hex;
};
union TexImage0
{
	struct
	{
		u32 width  : 10; // Actually w-1
		u32 height : 10; // Actually h-1
		u32 format : 4;
	};
	u32 hex;
};
union TexImage1
{
	struct
	{
		u32 tmem_even    : 15; // TMEM line index for even LODs
		u32 cache_width  : 3;
		u32 cache_height : 3;
		u32 image_type   : 1;  // 1 if this texture is managed manually (0 means we'll autofetch the texture data whenever it changes)
	};
	u32 hex;
};

union TexImage2
{
	struct
	{
		u32 tmem_odd     : 15; // tmem line index for odd LODs
		u32 cache_width  : 3;
		u32 cache_height : 3;
	};
	u32 hex;
};

union TexImage3
{
	struct
	{
		u32 image_base: 24;  //address in memory >> 5 (was 20 for GC)
	};
	u32 hex;
};
union TexTLUT
{
	struct
	{
		u32 tmem_offset : 10;
		u32 tlut_format : 2;
	};
	u32 hex;
};

union ZTex1
{
	struct
	{
		u32 bias : 24;
	};
	u32 hex;
};

union ZTex2
{
	struct
	{
		u32 type : 2; // TEV_Z_TYPE_X
		u32 op : 2; // GXZTexOp
	};
	u32 hex;
};

//  Z-texture types (formats)
#define TEV_ZTEX_TYPE_U8  0
#define TEV_ZTEX_TYPE_U16 1
#define TEV_ZTEX_TYPE_U24 2

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



// Geometry/other structs

union GenMode
{
	enum CullMode : u32
	{
		CULL_NONE = 0,
		CULL_BACK = 1, // cull back-facing primitives
		CULL_FRONT = 2, // cull front-facing primitives
		CULL_ALL = 3, // cull all primitives
	};

	BitField< 0,4,u32> numtexgens;
	BitField< 4,3,u32> numcolchans;
	// 1 bit unused?
	BitField< 8,1,u32> flat_shading; // unconfirmed
	BitField< 9,1,u32> multisampling;
	BitField<10,4,u32> numtevstages;
	BitField<14,2,CullMode> cullmode;
	BitField<16,3,u32> numindstages;
	BitField<19,1,u32> zfreeze;

	u32 hex;
};

union LPSize
{
	struct
	{
		u32 linesize   : 8; // in 1/6th pixels
		u32 pointsize  : 8; // in 1/6th pixels
		u32 lineoff    : 3;
		u32 pointoff   : 3;
		u32 lineaspect : 1; // interlacing: adjust for pixels having AR of 1/2
		u32 padding    : 1;
	};
	u32 hex;
};


union X12Y12
{
	struct
	{
		u32 y : 12;
		u32 x : 12;
	};
	u32 hex;
};
union X10Y10
{
	struct
	{
		u32 x : 10;
		u32 y : 10;
	};
	u32 hex;
};


// Framebuffer/pixel stuff (incl fog)

union BlendMode
{
	enum BlendFactor : u32
	{
		ZERO        = 0,
		ONE         = 1,
		SRCCLR      = 2,         // for dst factor
		INVSRCCLR   = 3,         // for dst factor
		DSTCLR      = SRCCLR,    // for src factor
		INVDSTCLR   = INVSRCCLR, // for src factor
		SRCALPHA    = 4,
		INVSRCALPHA = 5,
		DSTALPHA    = 6,
		INVDSTALPHA = 7
	};

	enum LogicOp : u32
	{
		CLEAR         =  0,
		AND           =  1,
		AND_REVERSE   =  2,
		COPY          =  3,
		AND_INVERTED  =  4,
		NOOP          =  5,
		XOR           =  6,
		OR            =  7,
		NOR           =  8,
		EQUIV         =  9,
		INVERT        = 10,
		OR_REVERSE    = 11,
		COPY_INVERTED = 12,
		OR_INVERTED   = 13,
		NAND          = 14,
		SET           = 15
	};

	BitField< 0,1,u32>         blendenable;
	BitField< 1,1,u32>         logicopenable;
	BitField< 2,1,u32>         dither;
	BitField< 3,1,u32>         colorupdate;
	BitField< 4,1,u32>         alphaupdate;
	BitField< 5,3,BlendFactor> dstfactor;
	BitField< 8,3,BlendFactor> srcfactor;
	BitField<11,1,u32>         subtract;
	BitField<12,4,LogicOp>     logicmode;

	u32 hex;
};


union FogParam0
{
	struct
	{
		u32 mantissa : 11;
		u32 exponent : 8;
		u32 sign : 1;
	};

	float GetA()
	{
		union { u32 i; float f; } dummy;
		dummy.i = ((u32)sign << 31) | ((u32)exponent << 23) | ((u32)mantissa << 12); // scale mantissa from 11 to 23 bits
		return dummy.f;
	}

	u32 hex;
};

union FogParam3
{
	struct
	{
		u32 c_mant : 11;
		u32 c_exp  : 8;
		u32 c_sign : 1;
		u32 proj   : 1; // 0 - perspective, 1 - orthographic
		u32 fsel   : 3; // 0 - off, 2 - linear, 4 - exp, 5 - exp2, 6 - backward exp, 7 - backward exp2
	};

	// amount to subtract from eyespacez after range adjustment
	float GetC()
	{
		union { u32 i; float f; } dummy;
		dummy.i = ((u32)c_sign << 31) | ((u32)c_exp << 23) | ((u32)c_mant << 12); // scale mantissa from 11 to 23 bits
		return dummy.f;
	}

	u32 hex;
};

union FogRangeKElement
{
	struct
	{
		u32 HI : 12;
		u32 LO : 12;
		u32 regid : 8;
	};

	// TODO: Which scaling coefficient should we use here? This is just a guess!
	float GetValue(int i) { return (i ? HI : LO) / 256.f; }
	u32 HEX;
};

struct FogRangeParams
{
	union RangeBase
	{
		struct
		{
			u32 Center  : 10; // viewport center + 342
			u32 Enabled : 1;
			u32 unused  : 13;
			u32 regid   : 8;
		};
		u32 hex;
	};
	RangeBase Base;
	FogRangeKElement K[5];
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
			u32 b  : 8;
			u32 g  : 8;
			u32 r  : 8;
		};
		u32 hex;
	};

	FogColor color;  //0:b 8:g 16:r - nice!
};

union ZMode
{
	enum CompareMode : u32
	{
		NEVER   = 0,
		LESS    = 1,
		EQUAL   = 2,
		LEQUAL  = 3,
		GREATER = 4,
		NEQUAL  = 5,
		GEQUAL  = 6,
		ALWAYS  = 7
	};

	BitField<0,1,u32>         testenable;
	BitField<1,3,CompareMode> func;
	BitField<4,1,u32>         updateenable;

	u32 hex;
};

union ConstantAlpha
{
	struct
	{
		u32 alpha : 8;
		u32 enable : 1;
	};
	u32 hex;
};

union FieldMode
{
	struct
	{
		u32 texLOD : 1; // adjust vert tex LOD computation to account for interlacing
	};
	u32 hex;
};

union FieldMask
{
	struct
	{
		// If bit is not set, do not write field to EFB
		u32 odd : 1;
		u32 even : 1;
	};
	u32 hex;
};

union PEControl
{
	enum PixelFormat : u32
	{
		RGB8_Z24    = 0,
		RGBA6_Z24   = 1,
		RGB565_Z16  = 2,
		Z24         = 3,
		Y8          = 4,
		U8          = 5,
		V8          = 6,
		YUV420      = 7,
		INVALID_FMT = 0xffffffff, // Used by Dolphin to represent a missing value.
	};

	enum DepthFormat : u32
	{
		ZLINEAR     = 0,
		ZNEAR       = 1,
		ZMID        = 2,
		ZFAR        = 3,

		// It seems these Z formats aren't supported/were removed ?
		ZINV_LINEAR = 4,
		ZINV_NEAR   = 5,
		ZINV_MID    = 6,
		ZINV_FAR    = 7
	};

	BitField< 0,3,PixelFormat> pixel_format;
	BitField< 3,3,DepthFormat> zformat;
	BitField< 6,1,u32>         early_ztest;

	u32 hex;
};


// Texture coordinate stuff

union TCInfo
{
	struct
	{
		u32 scale_minus_1  : 16;
		u32 range_bias     : 1;
		u32 cylindric_wrap : 1;
		// These bits only have effect in the s field of TCoordInfo
		u32 line_offset    : 1;
		u32 point_offset   : 1;
	};
	u32 hex;
};
struct TCoordInfo
{
	TCInfo s;
	TCInfo t;
};


union TevReg
{
	u64 hex;

	// Access to individual registers
	BitField< 0, 32,u64> low;
	BitField<32, 32,u64> high;

	// TODO: Check if Konst uses all 11 bits or just 8

	// Low register
	BitField< 0,11,s64> red;

	BitField<12,11,s64> alpha;
	BitField<23, 1,u64> type_ra;

	// High register
	BitField<32,11,s64> blue;

	BitField<44,11,s64> green;
	BitField<55, 1,u64> type_bg;
};

union TevKSel
{
	struct {
		u32 swap1 : 2;
		u32 swap2 : 2;
		u32 kcsel0 : 5;
		u32 kasel0 : 5;
		u32 kcsel1 : 5;
		u32 kasel1 : 5;
	};
	u32 hex;

	int getKC(int i) {return i?kcsel1:kcsel0;}
	int getKA(int i) {return i?kasel1:kasel0;}
};

union AlphaTest
{
	enum CompareMode : u32
	{
		NEVER   = 0,
		LESS    = 1,
		EQUAL   = 2,
		LEQUAL  = 3,
		GREATER = 4,
		NEQUAL  = 5,
		GEQUAL  = 6,
		ALWAYS  = 7
	};

	enum Op : u32
	{
		AND  = 0,
		OR   = 1,
		XOR  = 2,
		XNOR = 3
	};

	BitField< 0,8, u32>         ref0;
	BitField< 8,8, u32>         ref1;
	BitField<16,3, CompareMode> comp0;
	BitField<19,3, CompareMode> comp1;
	BitField<22,2, Op>          logic;

	u32 hex;

	enum TEST_RESULT
	{
		UNDETERMINED = 0,
		FAIL = 1,
		PASS = 2,
	};

	inline TEST_RESULT TestResult() const
	{
		switch (logic)
		{
		case AND:
			if (comp0 == ALWAYS && comp1 == ALWAYS)
				return PASS;
			if (comp0 == NEVER || comp1 == NEVER)
				return FAIL;
			break;

		case OR:
			if (comp0 == ALWAYS || comp1 == ALWAYS)
				return PASS;
			if (comp0 == NEVER && comp1 == NEVER)
				return FAIL;
			break;

		case XOR:
			if ((comp0 == ALWAYS && comp1 == NEVER) || (comp0 == NEVER && comp1 == ALWAYS))
				return PASS;
			if ((comp0 == ALWAYS && comp1 == ALWAYS) || (comp0 == NEVER && comp1 == NEVER))
				return FAIL;
			break;

		case XNOR:
			if ((comp0 == ALWAYS && comp1 == NEVER) || (comp0 == NEVER && comp1 == ALWAYS))
				return FAIL;
			if ((comp0 == ALWAYS && comp1 == ALWAYS) || (comp0 == NEVER && comp1 == NEVER))
				return PASS;
			break;
		}
		return UNDETERMINED;
	}
};

union UPE_Copy
{
	u32 Hex;

	BitField< 0,1,u32> clamp0;               // if set clamp top
	BitField< 1,1,u32> clamp1;               // if set clamp bottom
	BitField< 2,1,u32> yuv;                  // if set, color conversion from RGB to YUV
	BitField< 3,4,u32> target_pixel_format;  // realformat is (fmt/2)+((fmt&1)*8).... for some reason the msb is the lsb (pattern: cycling right shift)
	BitField< 7,2,u32> gamma;                // gamma correction.. 0 = 1.0 ; 1 = 1.7 ; 2 = 2.2 ; 3 is reserved
	BitField< 9,1,u32> half_scale;           // "mipmap" filter... 0 = no filter (scale 1:1) ; 1 = box filter (scale 2:1)
	BitField<10,1,u32> scale_invert;         // if set vertical scaling is on
	BitField<11,1,u32> clear;
	BitField<12,2,u32> frame_to_field;       // 0 progressive ; 1 is reserved ; 2 = interlaced (even lines) ; 3 = interlaced 1 (odd lines)
	BitField<14,1,u32> copy_to_xfb;
	BitField<15,1,u32> intensity_fmt;        // if set, is an intensity format (I4,I8,IA4,IA8)
	BitField<16,1,u32> auto_conv;            // if 0 automatic color conversion by texture format and pixel type

	u32 tp_realFormat()
	{
		return target_pixel_format / 2 + (target_pixel_format & 1) * 8;
	}
};

union BPU_PreloadTileInfo
{
	u32 hex;
	struct
	{
		u32 count : 15;
		u32 type : 2;
	};
};

struct BPS_TmemConfig
{
	u32 preload_addr;
	u32 preload_tmem_even;
	u32 preload_tmem_odd;
	BPU_PreloadTileInfo preload_tile_info;
	u32 tlut_src;
	u32 tlut_dest;
	u32 texinvalidate;
};

// All of BP memory


struct BPCmd
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
	PEControl zcontrol; //43 GXSetZCompLoc, GXPixModeSync
	FieldMask fieldmask; //44
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
	UPE_Copy triggerEFBCopy; //52
	u32 copyfilter[2]; //53,54
	u32 boundbox0;//55
	u32 boundbox1;//56
	u32 unknown7[2];//57,58
	X10Y10 scissorOffset; //59
	u32 unknown8[6]; //5a,5b,5c,5d, 5e,5f
	BPS_TmemConfig tmem_config; // 60-66
	u32 metric; //67
	FieldMode fieldmode;//68
	u32 unknown10[7];//69-6F
	u32 unknown11[16];//70-7F
	FourTexUnits tex[2]; //80-bf
	TevStageCombiner combiners[16]; //0xC0-0xDF
	TevReg tevregs[4];  //0xE0
	FogRangeParams fogRange;  // 0xE8
	FogParams fog; //0xEE,0xEF,0xF0,0xF1,0xF2
	AlphaTest alpha_test; //0xF3
	ZTex1 ztex1; //0xf4,0xf5
	ZTex2 ztex2;
	TevKSel tevksel[8];//0xf6,0xf7,f8,f9,fa,fb,fc,fd
	u32 bpMask; //0xFE
	u32 unknown18; //ff

	bool UseEarlyDepthTest() const { return zcontrol.early_ztest && zmode.testenable; }
	bool UseLateDepthTest() const { return !zcontrol.early_ztest && zmode.testenable; }
};

#pragma pack()

extern BPMemory bpmem;

void LoadBPReg(u32 value0);
void LoadBPRegPreprocess(u32 value0);

void GetBPRegInfo(const u8* data, std::string* name, std::string* desc);
