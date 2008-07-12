#ifndef _BPSTRUCTS_H
#define _BPSTRUCTS_H

#pragma pack(4)

//////////////////////////////////////////////////////////////////////////
// Tev/combiner things
//////////////////////////////////////////////////////////////////////////

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

enum Compare
{
	COMPARE_NEVER,
	COMPARE_LESS,
	COMPARE_EQUAL,
	COMPARE_LEQUAL,
	COMPARE_GREATER,
	COMPARE_NEQUAL,
	COMPARE_GEQUAL,
	COMPARE_ALWAYS
};

//color chan above:
//  rasterized color selections
#define RAS1_CC_0	0x00000000 /* color channel 0 */
#define RAS1_CC_1	0x00000001 /* color channel 1 */
#define RAS1_CC_B	0x00000005 /* indirect texture bump alpha */
#define RAS1_CC_BN	0x00000006 /* ind tex bump alpha, normalized 0-255 */
#define RAS1_CC_Z	0x00000007 /* set color value to zero */

//  Z-texture types (formats)
#define TEV_Z_TYPE_U8	0x00000000
#define TEV_Z_TYPE_U16	0x00000001
#define TEV_Z_TYPE_U24	0x00000002


struct TevStageCombiner
{
	union ColorCombiner
	{
		struct  //abc=8bit,d=10bit
		{
			unsigned d : 4;
			unsigned c : 4;
			unsigned b : 4;
			unsigned a : 4;

			unsigned bias : 2;
			unsigned op : 1;
			unsigned clamp : 1;

			unsigned scale : 2;
			unsigned outreg : 2;  //1,2,3

		};
		u32 hex;
	};
	union AlphaCombiner
	{
		struct 
		{
			unsigned rswap : 2;
			unsigned tswap : 2;
			unsigned d : 3;
			unsigned c : 3;
			unsigned b : 3;
			unsigned a : 3;

			unsigned bias : 2;
			unsigned op : 1;
			unsigned clamp : 1;

			unsigned scale : 2;
			unsigned outreg : 2;  //1,2,3
		};
		u32 hex;
	};

	ColorCombiner colorC;
	AlphaCombiner alphaC;
};

union TwoTevStageOrders
{
	struct 
	{
		unsigned texmap0    : 3;
		unsigned texcoord0  : 3;
		unsigned enable0    : 1;
		unsigned colorchan0 : 3;

		unsigned pad0       : 2;

		unsigned texmap1    : 3;
		unsigned texcoord1  : 3;
		unsigned enable1    : 1;
		unsigned colorchan1 : 3;

		unsigned pad1       : 2;
	};
	u32 hex;
	int getTexMap(int i){return i?texmap1:texmap0;}
	int getTexCoord(int i){return i?texcoord1:texcoord0;}
	int getEnable(int i){return i?enable1:enable0;}
	int getColorChan(int i){return i?colorchan1:colorchan0;}
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
		signed lod_bias : 8;
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
		unsigned image_base: 24;  //address in memory >> 5
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
		unsigned type : 2;
		unsigned op : 2;
	};
	u32 hex;
};

//  Z-texture types (formats)
#define TEV_ZTEX_TYPE_U8	0x00000000
#define TEV_ZTEX_TYPE_U16	0x00000001
#define TEV_ZTEX_TYPE_U24	0x00000002

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
		unsigned linesize : 8;
		unsigned pointsize : 8;
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
	u32 hex;
};
union FogParam3
{
	struct
	{
		unsigned cShifted12 : 20;
		unsigned proj : 1;
		unsigned fsel : 3;
	};
	u32 hex;
};
struct FogParams
{
	u32 a;
	u32 b_magnitude;
	u32 b_exponent;
	FogParam3 c_proj_fsel;
	u32 color;  //0:b 8:g 16:r - nice!
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


//////////////////////////////////////////////////////////////////////////
// All of BP memory
//////////////////////////////////////////////////////////////////////////

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
		unsigned target_pixel_format	: 3;
		unsigned						: 2;
		unsigned gamma					: 2;
		unsigned						: 1;
		unsigned scale_something		: 1;
		unsigned clear					: 1;
		unsigned frame_to_field			: 2;
		unsigned copy_to_xfb			: 1;
		unsigned						: 17;
	};
};

#define EFBCOPY_EFBTOTEXTURE 0x10000
#define EFBCOPY_CLEAR 0x800
#define EFBCOPY_GENERATEMIPS 0x200 

struct BPMemory
{
	GenMode genMode;
	u32 unknown[15]; //0f = flushtexturestate
	u32 tevind[16];
	X12Y12 scissorTL; //20
	X12Y12 scissorBR; //21
	LPSize lineptwidth; //22 line and point width
	u32 unknown1[2]; //23-24
	u32 unknown2[3]; //25-27
	TwoTevStageOrders tevorders[8]; //28-2F
	TCoordInfo texcoords[8]; //0x30 s,t,s,t,s,t,s,t...
	ZMode zmode; //40
	BlendMode blendmode; //41
	ConstantAlpha dstalpha;  //42
	u32 unknown4;  //43	// GXSetZCompLoc, GXPixModeSync
	u32 fieldmask; //44
	u32 drawdone;  //45
	u32 unknown5;  //46
	u32 drawsync1; //47
	u32 drawsync2; //48
	X10Y10 copyTexSrcXY; //49
	X10Y10 copyTexSrcWH; //4a
	u32 copyTexDest; //4b// 4b == CopyAddress (GXDispCopy and GXTexCopy use it)
	u32 unknown6[2]; //4c, 4d   
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
	u32 unknown8[10]; //5a,5b,5c,5d, 5e,5f,60,61, 62,   63 (GXTexModeSync),
	u32 tlutXferSrc; //64
	u32 tlutXferDest; //65
	u32 texinvalidate;//66
	u32 unknown9; //67
	u32 unknown10[8];//68-6F
	u32 unknown11[16];//70-7F
	FourTexUnits tex[2]; //80-bf
	TevStageCombiner combiners[16]; //0xC0-0xDF
	TevReg tevregs[4];  //0xE0
	u32 fogRangeAdj;   //0xE8
	u32 unknown15[5];  //0xe9,0xea,0xeb,0xec,0xed
	FogParams fog; //0xEE,0xEF,0xF0,0xF1,0xF2
	AlphaFunc alphaFunc; //0xF3
	ZTex1 ztex1; //0xf4,0xf5
	ZTex2 ztex2;
	TevKSel tevksel[8];//0xf6,0xf7,f8,f9,fa,fb,fc,fd
	u32 bpMask; //0xFE
	u32 unknown18; //ff
};

void BPInit();
size_t BPSaveLoadState(char *ptr, BOOL save);
//bool BPWritten(int addr, int changes);
void LoadBPReg(u32 value0);

void ActivateTextures();

extern BPMemory bpmem;

#pragma pack()

#endif