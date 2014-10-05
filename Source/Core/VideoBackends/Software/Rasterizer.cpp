// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/CommonTypes.h"
#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/HwRasterizer.h"
#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVideoConfig.h"
#include "VideoBackends/Software/Tev.h"
#include "VideoBackends/Software/XFMemLoader.h"


#define BLOCK_SIZE 2

#define CLAMP(x, a, b) (x>b)?b:(x<a)?a:x

// returns approximation of log2(f) in s28.4
// results are close enough to use for LOD
static inline s32 FixedLog2(float f)
{
	u32 *x = (u32*)&f;
	s32 logInt = ((*x & 0x7F800000) >> 19) - 2032; // integer part
	s32 logFract = (*x & 0x007fffff) >> 19; // approximate fractional part

	return logInt + logFract;
}

namespace Rasterizer
{
static Slope ZSlope;
static Slope WSlope;
static Slope ColorSlopes[2][4];
static Slope TexSlopes[8][3];

static s32 vertex0X;
static s32 vertex0Y;
static float vertexOffsetX;
static float vertexOffsetY;

static s32 scissorLeft = 0;
static s32 scissorTop = 0;
static s32 scissorRight = 0;
static s32 scissorBottom = 0;

static Tev tev;
static RasterBlock rasterBlock;

void DoState(PointerWrap &p)
{
	ZSlope.DoState(p);
	WSlope.DoState(p);
	for (auto& color_slopes_1d : ColorSlopes)
		for (Slope& color_slope : color_slopes_1d)
			color_slope.DoState(p);
	for (auto& tex_slopes_1d : TexSlopes)
		for (Slope& tex_slope : tex_slopes_1d)
			tex_slope.DoState(p);
	p.Do(vertex0X);
	p.Do(vertex0Y);
	p.Do(vertexOffsetX);
	p.Do(vertexOffsetY);
	p.Do(scissorLeft);
	p.Do(scissorTop);
	p.Do(scissorRight);
	p.Do(scissorBottom);
	tev.DoState(p);
	p.Do(rasterBlock);
}

void Init()
{
	tev.Init();

	// Set initial z reference plane in the unlikely case that zfreeze is enabled when drawing the first primitive.
	// TODO: This is just a guess!
	ZSlope.dfdx = ZSlope.dfdy = 0.f;
	ZSlope.f0 = 1.f;
}

static inline int iround(float x)
{
	int t = (int)x;
	if ((x - t) >= 0.5)
		return t + 1;

	return t;
}

void SetScissor()
{
	int xoff = bpmem.scissorOffset.x * 2 - 342;
	int yoff = bpmem.scissorOffset.y * 2 - 342;

	scissorLeft = bpmem.scissorTL.x - xoff - 342;
	if (scissorLeft < 0)
		scissorLeft = 0;

	scissorTop = bpmem.scissorTL.y - yoff - 342;
	if (scissorTop < 0)
		scissorTop = 0;

	scissorRight = bpmem.scissorBR.x - xoff - 341;
	if (scissorRight > EFB_WIDTH)
		scissorRight = EFB_WIDTH;

	scissorBottom = bpmem.scissorBR.y - yoff - 341;
	if (scissorBottom > EFB_HEIGHT)
		scissorBottom = EFB_HEIGHT;
}

void SetTevReg(int reg, int comp, bool konst, s16 color)
{
	tev.SetRegColor(reg, comp, konst, color);
}

inline void Draw(s32 x, s32 y, s32 xi, s32 yi)
{
	INCSTAT(swstats.thisFrame.rasterizedPixels);

	float dx = vertexOffsetX + (float)(x - vertex0X);
	float dy = vertexOffsetY + (float)(y - vertex0Y);

	s32 z = (s32)ZSlope.GetValue(dx, dy);
	if (z < 0 || z > 0x00ffffff)
		return;

	if (bpmem.UseEarlyDepthTest() && g_SWVideoConfig.bZComploc)
	{
		// TODO: Test if perf regs are incremented even if test is disabled
		EfbInterface::IncPerfCounterQuadCount(PQ_ZCOMP_INPUT_ZCOMPLOC);
		if (bpmem.zmode.testenable)
		{
			// early z
			if (!EfbInterface::ZCompare(x, y, z))
				return;
		}
		EfbInterface::IncPerfCounterQuadCount(PQ_ZCOMP_OUTPUT_ZCOMPLOC);
	}

	RasterBlockPixel& pixel = rasterBlock.Pixel[xi][yi];

	tev.Position[0] = x;
	tev.Position[1] = y;
	tev.Position[2] = z;

	//  colors
	for (unsigned int i = 0; i < bpmem.genMode.numcolchans; i++)
	{
		for (int comp = 0; comp < 4; comp++)
		{
			u16 color = (u16)ColorSlopes[i][comp].GetValue(dx, dy);

			// clamp color value to 0
			u16 mask = ~(color >> 8);

			tev.Color[i][comp] = color & mask;
		}
	}

	// tex coords
	for (unsigned int i = 0; i < bpmem.genMode.numtexgens; i++)
	{
		// multiply by 128 because TEV stores UVs as s17.7
		tev.Uv[i].s = (s32)(pixel.Uv[i][0] * 128);
		tev.Uv[i].t = (s32)(pixel.Uv[i][1] * 128);
	}

	for (unsigned int i = 0; i < bpmem.genMode.numindstages; i++)
	{
		tev.IndirectLod[i] = rasterBlock.IndirectLod[i];
		tev.IndirectLinear[i] = rasterBlock.IndirectLinear[i];
	}

	for (unsigned int i = 0; i <= bpmem.genMode.numtevstages; i++)
	{
		tev.TextureLod[i] = rasterBlock.TextureLod[i];
		tev.TextureLinear[i] = rasterBlock.TextureLinear[i];
	}

	tev.Draw();
}

static void InitTriangle(float X1, float Y1, s32 xi, s32 yi)
{
	vertex0X = xi;
	vertex0Y = yi;

	// adjust a little less than 0.5
	const float adjust = 0.495f;

	vertexOffsetX = ((float)xi - X1) + adjust;
	vertexOffsetY = ((float)yi - Y1) + adjust;
}

static void InitSlope(Slope *slope, float f1, float f2, float f3, float DX31, float DX12, float DY12, float DY31)
{
	float DF31 = f3 - f1;
	float DF21 = f2 - f1;
	float a = DF31 * -DY12 - DF21 * DY31;
	float b = DX31 * DF21 + DX12 * DF31;
	float c = -DX12 * DY31 - DX31 * -DY12;
	slope->dfdx = -a / c;
	slope->dfdy = -b / c;
	slope->f0 = f1;
}

static inline void CalculateLOD(s32* lodp, bool* linear, u32 texmap, u32 texcoord)
{
	FourTexUnits& texUnit = bpmem.tex[(texmap >> 2) & 1];
	u8 subTexmap = texmap & 3;

	// LOD calculation requires data from the texture mode for bias, etc.
	// it does not seem to use the actual texture size
	TexMode0& tm0 = texUnit.texMode0[subTexmap];
	TexMode1& tm1 = texUnit.texMode1[subTexmap];

	float sDelta, tDelta;
	if (tm0.diag_lod)
	{
		float *uv0 = rasterBlock.Pixel[0][0].Uv[texcoord];
		float *uv1 = rasterBlock.Pixel[1][1].Uv[texcoord];

		sDelta = fabsf(uv0[0] - uv1[0]);
		tDelta = fabsf(uv0[1] - uv1[1]);
	}
	else
	{
		float *uv0 = rasterBlock.Pixel[0][0].Uv[texcoord];
		float *uv1 = rasterBlock.Pixel[1][0].Uv[texcoord];
		float *uv2 = rasterBlock.Pixel[0][1].Uv[texcoord];

		sDelta = std::max(fabsf(uv0[0] - uv1[0]), fabsf(uv0[0] - uv2[0]));
		tDelta = std::max(fabsf(uv0[1] - uv1[1]), fabsf(uv0[1] - uv2[1]));
	}

	// get LOD in s28.4
	s32 lod = FixedLog2(std::max(sDelta, tDelta));

	// bias is s2.5
	int bias = tm0.lod_bias;
	bias >>= 1;
	lod += bias;

	*linear = ((lod > 0 && (tm0.min_filter & 4)) || (lod <= 0 && tm0.mag_filter));

	// order of checks matters
	// should be:
	// if lod > max then max
	// else if lod < min then min
	lod = CLAMP(lod, (s32)tm1.min_lod, (s32)tm1.max_lod);
	*lodp = lod;
}

static void BuildBlock(s32 blockX, s32 blockY)
{
	for (s32 yi = 0; yi < BLOCK_SIZE; yi++)
	{
		for (s32 xi = 0; xi < BLOCK_SIZE; xi++)
		{
			RasterBlockPixel& pixel = rasterBlock.Pixel[xi][yi];

			float dx = vertexOffsetX + (float)(xi + blockX - vertex0X);
			float dy = vertexOffsetY + (float)(yi + blockY - vertex0Y);

			float invW = 1.0f / WSlope.GetValue(dx, dy);
			pixel.InvW = invW;

			// tex coords
			for (unsigned int i = 0; i < bpmem.genMode.numtexgens; i++)
			{
				float projection = invW;
				if (xfmem.texMtxInfo[i].projection)
				{
					float q = TexSlopes[i][2].GetValue(dx, dy) * invW;
					if (q != 0.0f)
						projection = invW / q;
				}

				pixel.Uv[i][0] = TexSlopes[i][0].GetValue(dx, dy) * projection;
				pixel.Uv[i][1] = TexSlopes[i][1].GetValue(dx, dy) * projection;
			}
		}
	}

	u32 indref = bpmem.tevindref.hex;
	for (unsigned int i = 0; i < bpmem.genMode.numindstages; i++)
	{
		u32 texmap = indref & 3;
		indref >>= 3;
		u32 texcoord = indref & 3;
		indref >>= 3;

		CalculateLOD(&rasterBlock.IndirectLod[i], &rasterBlock.IndirectLinear[i], texmap, texcoord);
	}

	for (unsigned int i = 0; i <= bpmem.genMode.numtevstages; i++)
	{
		int stageOdd = i&1;
		TwoTevStageOrders &order = bpmem.tevorders[i >> 1];
		if (order.getEnable(stageOdd))
		{
			u32 texmap = order.getTexMap(stageOdd);
			u32 texcoord = order.getTexCoord(stageOdd);

			CalculateLOD(&rasterBlock.TextureLod[i], &rasterBlock.TextureLinear[i], texmap, texcoord);
		}
	}
}

void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
{
	INCSTAT(swstats.thisFrame.numTrianglesDrawn);

	if (g_SWVideoConfig.bHwRasterizer)
	{
		HwRasterizer::DrawTriangleFrontFace(v0, v1, v2);
		return;
	}

	// adapted from http://devmaster.net/posts/6145/advanced-rasterization

	// 28.4 fixed-pou32 coordinates. rounded to nearest and adjusted to match hardware output
	// could also take floor and adjust -8
	const s32 Y1 = iround(16.0f * v0->screenPosition[1]) - 9;
	const s32 Y2 = iround(16.0f * v1->screenPosition[1]) - 9;
	const s32 Y3 = iround(16.0f * v2->screenPosition[1]) - 9;

	const s32 X1 = iround(16.0f * v0->screenPosition[0]) - 9;
	const s32 X2 = iround(16.0f * v1->screenPosition[0]) - 9;
	const s32 X3 = iround(16.0f * v2->screenPosition[0]) - 9;

	// Deltas
	const s32 DX12 = X1 - X2;
	const s32 DX23 = X2 - X3;
	const s32 DX31 = X3 - X1;

	const s32 DY12 = Y1 - Y2;
	const s32 DY23 = Y2 - Y3;
	const s32 DY31 = Y3 - Y1;

	// Fixed-pos32 deltas
	const s32 FDX12 = DX12 << 4;
	const s32 FDX23 = DX23 << 4;
	const s32 FDX31 = DX31 << 4;

	const s32 FDY12 = DY12 << 4;
	const s32 FDY23 = DY23 << 4;
	const s32 FDY31 = DY31 << 4;

	// Bounding rectangle
	s32 minx = (std::min(std::min(X1, X2), X3) + 0xF) >> 4;
	s32 maxx = (std::max(std::max(X1, X2), X3) + 0xF) >> 4;
	s32 miny = (std::min(std::min(Y1, Y2), Y3) + 0xF) >> 4;
	s32 maxy = (std::max(std::max(Y1, Y2), Y3) + 0xF) >> 4;

	// scissor
	minx = std::max(minx, scissorLeft);
	maxx = std::min(maxx, scissorRight);
	miny = std::max(miny, scissorTop);
	maxy = std::min(maxy, scissorBottom);

	if (minx >= maxx || miny >= maxy)
		return;

	// Setup slopes
	float fltx1 = v0->screenPosition.x;
	float flty1 = v0->screenPosition.y;
	float fltdx31 = v2->screenPosition.x - fltx1;
	float fltdx12 = fltx1 - v1->screenPosition.x;
	float fltdy12 = flty1 - v1->screenPosition.y;
	float fltdy31 = v2->screenPosition.y - flty1;

	InitTriangle(fltx1, flty1, (X1 + 0xF) >> 4, (Y1 + 0xF) >> 4);

	float w[3] = { 1.0f / v0->projectedPosition.w, 1.0f / v1->projectedPosition.w, 1.0f / v2->projectedPosition.w };
	InitSlope(&WSlope, w[0], w[1], w[2], fltdx31, fltdx12, fltdy12, fltdy31);

	// TODO: The zfreeze emulation is not quite correct, yet!
	// Many things might prevent us from reaching this line (culling, clipping, scissoring).
	// However, the zslope is always guaranteed to be calculated unless all vertices are trivially rejected during clipping!
	// We're currently sloppy at this since we abort early if any of the culling/clipping/scissoring tests fail.
	if (!bpmem.genMode.zfreeze || !g_SWVideoConfig.bZFreeze)
		InitSlope(&ZSlope, v0->screenPosition[2], v1->screenPosition[2], v2->screenPosition[2], fltdx31, fltdx12, fltdy12, fltdy31);

	for (unsigned int i = 0; i < bpmem.genMode.numcolchans; i++)
	{
		for (int comp = 0; comp < 4; comp++)
			InitSlope(&ColorSlopes[i][comp], v0->color[i][comp], v1->color[i][comp], v2->color[i][comp], fltdx31, fltdx12, fltdy12, fltdy31);
	}

	for (unsigned int i = 0; i < bpmem.genMode.numtexgens; i++)
	{
		for (int comp = 0; comp < 3; comp++)
			InitSlope(&TexSlopes[i][comp], v0->texCoords[i][comp] * w[0], v1->texCoords[i][comp] * w[1], v2->texCoords[i][comp] * w[2], fltdx31, fltdx12, fltdy12, fltdy31);
	}

	// Start in corner of 8x8 block
	minx &= ~(BLOCK_SIZE - 1);
	miny &= ~(BLOCK_SIZE - 1);

	// Half-edge constants
	s32 C1 = DY12 * X1 - DX12 * Y1;
	s32 C2 = DY23 * X2 - DX23 * Y2;
	s32 C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// Loop through blocks
	for (s32 y = miny; y < maxy; y += BLOCK_SIZE)
	{
		for (s32 x = minx; x < maxx; x += BLOCK_SIZE)
		{
			// Corners of block
			s32 x0 = x << 4;
			s32 x1 = (x + BLOCK_SIZE - 1) << 4;
			s32 y0 = y << 4;
			s32 y1 = (y + BLOCK_SIZE - 1) << 4;

			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

			// Skip block when outside an edge
			if (a == 0x0 || b == 0x0 || c == 0x0)
				continue;

			BuildBlock(x, y);

			// Accept whole block when totally covered
			if (a == 0xF && b == 0xF && c == 0xF)
			{
				for (s32 iy = 0; iy < BLOCK_SIZE; iy++)
				{
					for (s32 ix = 0; ix < BLOCK_SIZE; ix++)
					{
						Draw(x + ix, y + iy, ix, iy);
					}
				}
			}
			else // Partially covered block
			{
				s32 CY1 = C1 + DX12 * y0 - DY12 * x0;
				s32 CY2 = C2 + DX23 * y0 - DY23 * x0;
				s32 CY3 = C3 + DX31 * y0 - DY31 * x0;

				for (s32 iy = 0; iy < BLOCK_SIZE; iy++)
				{
					s32 CX1 = CY1;
					s32 CX2 = CY2;
					s32 CX3 = CY3;

					for (s32 ix = 0; ix < BLOCK_SIZE; ix++)
					{
						if (CX1 > 0 && CX2 > 0 && CX3 > 0)
						{
							Draw(x + ix, y + iy, ix, iy);
						}

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}
			}
		}
	}
}


}
