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

#include "Common.h"

#include "Rasterizer.h"
#include "HwRasterizer.h"
#include "EfbInterface.h"
#include "BPMemLoader.h"
#include "XFMemLoader.h"
#include "Tev.h"
#include "Statistics.h"
#include "VideoConfig.h"


#define BLOCK_SIZE 8


namespace Rasterizer
{

s32 scissorLeft = 0;
s32 scissorTop = 0;
s32 scissorRight = 0;
s32 scissorBottom = 0;

Tev tev;

void Init()
{
     tev.Init();
}

inline int iround(float x)
{
    int t;

    __asm
    {
        fld  x
        fistp t
    }

    return t;
}

void SetScissor()
{
    int xoff = bpmem.scissorOffset.x * 2 - 342;
    int yoff = bpmem.scissorOffset.y * 2 - 342;

	scissorLeft = bpmem.scissorTL.x - xoff - 342;
	if (scissorLeft < 0) scissorLeft = 0;

	scissorTop = bpmem.scissorTL.y - yoff - 342;
	if (scissorTop < 0) scissorTop = 0;
    
	scissorRight = bpmem.scissorBR.x - xoff - 341;
	if (scissorRight > EFB_WIDTH) scissorRight = EFB_WIDTH;

	scissorBottom = bpmem.scissorBR.y - yoff - 341;
	if (scissorBottom > EFB_HEIGHT) scissorBottom = EFB_HEIGHT;
}

void SetTevReg(int reg, int comp, bool konst, s16 color)
{
    tev.SetRegColor(reg, comp, konst, color);
}

inline void Draw(s32 x, s32 y)
{
    INCSTAT(stats.thisFrame.rasterizedPixels);

    float zFloat = 1.0f + ZSlope.GetValue(x, y);
    if(zFloat < 0|| zFloat > 1)
        return;

    u32 z = (u32)(zFloat * 0x00ffffff);

    if (bpmem.zcontrol.zcomploc && bpmem.zmode.testenable)
    {
        // early z
        if (!EfbInterface::ZCompare(x, y, z))
            return;
    }

    float invW = 1.0f / WSlope.GetValue(x, y);

    tev.Position[0] = x;
    tev.Position[1] = y;
    tev.Position[2] = z;

    for(unsigned int i = 0; i < bpmem.genMode.numcolchans; i++)
    {
        for(int comp = 0; comp < 4; comp++)
            tev.Color[i][comp] = (u8)ColorSlopes[i][comp].GetValue(x, y);
    }

    for(unsigned int i = 0; i < bpmem.genMode.numtexgens; i++)
    {
        if (xfregs.texMtxInfo[i].projection)
        {
            float q = TexSlopes[i][2].GetValue(x, y) * invW;
            float invQ = invW / q;
            tev.Uv[i][0] = TexSlopes[i][0].GetValue(x, y) * invQ * (bpmem.texcoords[i].s.scale_minus_1 + 1);
            tev.Uv[i][1] = TexSlopes[i][1].GetValue(x, y) * invQ * (bpmem.texcoords[i].t.scale_minus_1 + 1);
            tev.Lod[i] = 0;
        }
        else
        {
            tev.Uv[i][0] = TexSlopes[i][0].GetValue(x, y) * invW * (bpmem.texcoords[i].s.scale_minus_1 + 1);
            tev.Uv[i][1] = TexSlopes[i][1].GetValue(x, y) * invW * (bpmem.texcoords[i].t.scale_minus_1 + 1);
            tev.Lod[i] = 0;
        }
    }

    tev.Draw();
}

void InitSlope(Slope *slope, float f1, float f2, float f3, float DX31, float DX12, float DY12, float DY31, float X1, float Y1)
{
    float DF31 = f3 - f1;
    float DF21 = f2 - f1;
    float a = DF31 * -DY12 - DF21 * DY31;
    float b = DX31 * DF21 + DX12 * DF31;    
    float c = -DX12 * DY31 - DX31 * -DY12;
    slope->dfdx = -a / c;
    slope->dfdy = -b / c;
    slope->f0 = f1;
    slope->x0 = X1;
    slope->y0 = Y1;
}

void DrawTriangleFrontFace(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
{
    INCSTAT(stats.thisFrame.numTrianglesDrawn);

    if (g_Config.bHwRasterizer)
    {
        HwRasterizer::DrawTriangleFrontFace(v0, v1, v2);
        return;
    }

    // adapted from http://www.devmaster.net/forums/showthread.php?t=1884

    // 28.4 fixed-pou32 coordinates. rounded to nearest
    const s32 Y1 = iround(16.0f * v0->screenPosition[1]);
    const s32 Y2 = iround(16.0f * v1->screenPosition[1]);
    const s32 Y3 = iround(16.0f * v2->screenPosition[1]);

    const s32 X1 = iround(16.0f * v0->screenPosition[0]);
    const s32 X2 = iround(16.0f * v1->screenPosition[0]);
    const s32 X3 = iround(16.0f * v2->screenPosition[0]);

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
    s32 minx = (min(min(X1, X2), X3) + 0xF) >> 4;
    s32 maxx = (max(max(X1, X2), X3) + 0xF) >> 4;
    s32 miny = (min(min(Y1, Y2), Y3) + 0xF) >> 4;
    s32 maxy = (max(max(Y1, Y2), Y3) + 0xF) >> 4;

    // scissor
    minx = max(minx, scissorLeft);
    maxx = min(maxx, scissorRight);
    miny = max(miny, scissorTop);
    maxy = min(maxy, scissorBottom);

    if (minx >= maxx || miny >= maxy)
        return;

    // Setup slopes
    float fltx1 = v0->screenPosition[0];
    float flty1 = v0->screenPosition[1];
    float fltdx31 = v2->screenPosition[0] - fltx1;
    float fltdx12 = fltx1 - v1->screenPosition[0];
    float fltdy12 = flty1 - v1->screenPosition[1];
    float fltdy31 = v2->screenPosition[1] - flty1;

    float w[3] = { 1.0f / v0->projectedPosition[3], 1.0f / v1->projectedPosition[3], 1.0f / v2->projectedPosition[3] };
    InitSlope(&WSlope, w[0], w[1], w[2], fltdx31, fltdx12, fltdy12, fltdy31, fltx1, flty1);

    InitSlope(&ZSlope, v0->screenPosition[2], v1->screenPosition[2], v2->screenPosition[2], fltdx31, fltdx12, fltdy12, fltdy31, fltx1, flty1);

    for(unsigned int i = 0; i < bpmem.genMode.numcolchans; i++)
    {
        for(int comp = 0; comp < 4; comp++)
            InitSlope(&ColorSlopes[i][comp], v0->color[i][comp], v1->color[i][comp], v2->color[i][comp], fltdx31, fltdx12, fltdy12, fltdy31, fltx1, flty1);
    }

    for(unsigned int i = 0; i < bpmem.genMode.numtexgens; i++)
    {
        for(int comp = 0; comp < 3; comp++)
            InitSlope(&TexSlopes[i][comp], v0->texCoords[i][comp] * w[0], v1->texCoords[i][comp] * w[1], v2->texCoords[i][comp] * w[2], fltdx31, fltdx12, fltdy12, fltdy31, fltx1, flty1);
    }

    // Start in corner of 8x8 block
    minx &= ~(BLOCK_SIZE - 1);
    miny &= ~(BLOCK_SIZE - 1);
    
    // Half-edge constants
    s32 C1 = DY12 * X1 - DX12 * Y1;
    s32 C2 = DY23 * X2 - DX23 * Y2;
    s32 C3 = DY31 * X3 - DX31 * Y3;

    // Correct for fill convention
    if(DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
    if(DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
    if(DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

    // Loop through blocks
    for(s32 y = miny; y < maxy; y += BLOCK_SIZE)
    {
        for(s32 x = minx; x < maxx; x += BLOCK_SIZE)
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
            if(a == 0x0 || b == 0x0 || c == 0x0) continue;

            // Accept whole block when totally covered
            if(a == 0xF && b == 0xF && c == 0xF)
            {
                for(s32 iy = 0; iy < BLOCK_SIZE; iy++)
                {
                    for(s32 ix = x; ix < x + BLOCK_SIZE; ix++)
                    {                        
                        Draw(ix, iy + y);
                    }
                }
            }
            else // Partially covered block
            {
                s32 CY1 = C1 + DX12 * y0 - DY12 * x0;
                s32 CY2 = C2 + DX23 * y0 - DY23 * x0;
                s32 CY3 = C3 + DX31 * y0 - DY31 * x0;

                for(s32 iy = y; iy < y + BLOCK_SIZE; iy++)
                {
                    s32 CX1 = CY1;
                    s32 CX2 = CY2;
                    s32 CX3 = CY3;

                    for(s32 ix = x; ix < x + BLOCK_SIZE; ix++)
                    {
                        if(CX1 > 0 && CX2 > 0 && CX3 > 0)
                        {
                            Draw(ix, iy);
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