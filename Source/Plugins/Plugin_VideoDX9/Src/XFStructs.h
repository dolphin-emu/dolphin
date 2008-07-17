#ifndef _XFSTRUCTS_H
#define _XFSTRUCTS_H

#include "Common.h"
#include "Vec3.h"

#pragma pack(4)

//////////////////////////////////////////////////////////////////////////
// Lighting
//////////////////////////////////////////////////////////////////////////
#define XF_TEX_ST   0x00000000
#define XF_TEX_STQ  0x00000001

#define XF_TEX_AB11 0x00000000
#define XF_TEX_ABC1 0x00000001

#define XF_TEXGEN_REGULAR       0x00000000
#define XF_TEXGEN_EMBOSS_MAP    0x00000001
#define XF_TEXGEN_COLOR_STRGBC0 0x00000002
#define XF_TEXGEN_COLOR_STRGBC1 0x00000003

#define XF_GEOM_INROW       0x00000000
#define XF_NORMAL_INROW     0x00000001
#define XF_COLORS_INROW     0x00000002
#define XF_BINORMAL_T_INROW 0x00000003
#define XF_BINORMAL_B_INROW 0x00000004
#define XF_TEX0_INROW       0x00000005
#define XF_TEX1_INROW       0x00000006
#define XF_TEX2_INROW       0x00000007
#define XF_TEX3_INROW       0x00000008
#define XF_TEX4_INROW       0x00000009
#define XF_TEX5_INROW       0x0000000a
#define XF_TEX6_INROW       0x0000000b
#define XF_TEX7_INROW       0x0000000c

struct Light
{
	u32 useless[3]; 
	//Vec3 direction;
	u32 color;    //rgba
	float a0;  //attenuation
	float a1; 
	float a2; 
	float k0;  //k stuff
	float k1; 
	float k2; 
	union
	{
		struct {
			Vec3 dpos;
			Vec3 ddir;
		};
		struct {
			Vec3 sdir;
			Vec3 shalfangle;
		};
	};
};

#define GX_SRC_REG 0
#define GX_SRC_VTX 1

union LitChannel
{
	struct
	{
		unsigned matsource      : 1;
		unsigned enablelighting : 1;
		unsigned lightMask0_3   : 4;
		unsigned ambsource      : 1;
		unsigned diffusefunc    : 2; //0=none 1=sign 2=clamp
		unsigned attnfunc       : 2;    //1=spec 3=spot 2=none ???
		unsigned lightMask4_7   : 4;
	};
	u32 hex;
	unsigned int GetFullLightMask()
	{
		return lightMask0_3 | (lightMask4_7 << 4);
	}
};

struct ColorChannel
{
	u32 ambColor;
	u32 matColor;
	LitChannel color;
	LitChannel alpha;
};

struct MiscXF
{
	int numTexGens;
};

union TexMtxInfo
{
	struct 
	{
		unsigned unknown : 1;
		unsigned projection : 1;
		unsigned inputform : 2; //1 if three-component, 0 if two-component ?
		unsigned texgentype : 3; //0-POS 1-NRM 3-BINRM 4-TANGENT 5-TEX0 ...12-TEX7 13-COLOR
		unsigned sourcerow : 5;
		unsigned embosssourceshift : 3;
		unsigned embosslightshift : 3;
	};
	u32 hex;
};

union PostMtxInfo
{
	struct 
	{
		unsigned index : 8;
		unsigned normalize : 1;
	};
	u32 hex;
};

struct TexCoordInfo
{
	TexMtxInfo texmtxinfo;
	PostMtxInfo postmtxinfo;
};

struct Viewport
{
	float wd;
	float ht;
	float nearZ;
	float xOrig;
	float yOrig;
	float farZ;
};

#define XFMEM_SIZE               0x8000
#define XFMEM_POSMATRICES        0x000
#define XFMEM_POSMATRICES_END    0x100
#define XFMEM_NORMALMATRICES     0x400
#define XFMEM_NORMALMATRICES_END 0x460
#define XFMEM_POSTMATRICES       0x500
#define XFMEM_POSTMATRICES_END   0x600
#define XFMEM_LIGHTS             0x600
#define XFMEM_LIGHTS_END         0x680


extern TexCoordInfo texcoords[8];
extern ColorChannel colChans[2]; //C0A0 C1A1
extern MiscXF miscxf;

extern unsigned __int32 xfmem[XFMEM_SIZE];

extern float rawViewPort[6];
extern float rawProjection[7];

size_t XFSaveLoadState(char *ptr, BOOL save);
void XFUpdateVP();
void XFUpdatePJ();
void LoadXFReg(u32 transferSize, u32 address, u32 *pData);
void LoadIndexedXF(u32 val, int array);

#pragma pack()


#endif