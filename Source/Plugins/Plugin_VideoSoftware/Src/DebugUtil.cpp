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

#include "main.h"
#include "DebugUtil.h"
#include "BPMemLoader.h"
#include "TextureSampler.h"
#include "VideoConfig.h"
#include "EfbInterface.h"
#include "Statistics.h"
#include "HwRasterizer.h"
#include "StringUtil.h"
#include "CommandProcessor.h"
#include "../../../Core/VideoCommon/Src/ImageWrite.h"

namespace DebugUtil
{

u32 skipFrames = 0;
const int NumObjectBuffers = 32;
u8 ObjectBuffer[NumObjectBuffers][EFB_WIDTH*EFB_HEIGHT*4];
bool DrawnToBuffer[NumObjectBuffers];
const char* ObjectBufferName[NumObjectBuffers];

void Init()
{
    for (int i = 0; i < NumObjectBuffers; i++)
    {
        memset(ObjectBuffer[i], 0, sizeof(ObjectBuffer[i]));
        DrawnToBuffer[i] = false;
        ObjectBufferName[i] = 0;
    }
}

bool SaveTexture(const char* filename, u32 texmap, int width, int height)
{
    u8 *data = new u8[width * height * 4];
    
    GetTextureBGRA(data, texmap, width, height);

    bool result = SaveTGA(filename, width, height, data);

    delete []data;

    return result;
}

void SaveTexture(const char* filename, u32 texmap)
{
    FourTexUnits& texUnit = bpmem.tex[(texmap >> 2) & 1];
    u8 subTexmap = texmap & 3;

    TexImage0& ti0 = texUnit.texImage0[subTexmap];

    SaveTexture(filename, texmap, ti0.width, ti0.height);
}

void GetTextureBGRA(u8 *dst, u32 texmap, int width, int height)
{
    u8 sample[4];    

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            TextureSampler::Sample((float)x, (float)y, 0, texmap, sample);
            // rgba to bgra
            *(dst++) = sample[2];
            *(dst++) = sample[1];
            *(dst++) = sample[0];
            *(dst++) = sample[3];
        }
}

void DumpActiveTextures()
{
    for (unsigned int stageNum = 0; stageNum < bpmem.genMode.numindstages; stageNum++)
    {
        u32 texmap = bpmem.tevindref.getTexMap(stageNum);

        SaveTexture(StringFromFormat("%s/tar%i_ind%i_map%i.tga", FULL_DUMP_TEXTURES_DIR, stats.thisFrame.numDrawnObjects, stageNum, texmap).c_str(), texmap);     
    }

    for (unsigned int stageNum = 0; stageNum <= bpmem.genMode.numtevstages; stageNum++)
    {
        int stageNum2 = stageNum >> 1;
        int stageOdd = stageNum&1;
        TwoTevStageOrders &order = bpmem.tevorders[stageNum2];

        int texmap = order.getTexMap(stageOdd);

        SaveTexture(StringFromFormat("%s/tar%i_stage%i_map%i.tga", FULL_DUMP_TEXTURES_DIR, stats.thisFrame.numDrawnObjects, stageNum, texmap).c_str(), texmap);           
    }
}

void DumpEfb(const char* filename)
{
    u8 *data = new u8[EFB_WIDTH * EFB_HEIGHT * 4];
    u8 *writePtr = data;
    u8 sample[4];    

    for (int y = 0; y < EFB_HEIGHT; y++)
        for (int x = 0; x < EFB_WIDTH; x++) {
            EfbInterface::GetColor(x, y, sample);
            // rgba to bgra
            *(writePtr++) = sample[2];
            *(writePtr++) = sample[1];
            *(writePtr++) = sample[0];
            *(writePtr++) = sample[3];
        }

    bool result = SaveTGA(filename, EFB_WIDTH, EFB_HEIGHT, data);

    delete []data;
}

void DumpDepth(const char* filename)
{
    u8 *data = new u8[EFB_WIDTH * EFB_HEIGHT * 4];
    u8 *writePtr = data;

    for (int y = 0; y < EFB_HEIGHT; y++)
        for (int x = 0; x < EFB_WIDTH; x++) {
            u32 depth = EfbInterface::GetDepth(x, y);
            // depth to bgra
            *(writePtr++) = (depth >> 16) & 0xff;
            *(writePtr++) = (depth >> 8) & 0xff;            
            *(writePtr++) = depth & 0xff;
            *(writePtr++) = 255;
        }

    bool result = SaveTGA(filename, EFB_WIDTH, EFB_HEIGHT, data);

    delete []data;
}

void DrawObjectBuffer(s16 x, s16 y, u8 *color, int buffer, const char *name)
{
    u32 offset = (x + y * EFB_WIDTH) * 4;
    u8 *dst = &ObjectBuffer[buffer][offset];
    *(dst++) = color[2];
    *(dst++) = color[1];
    *(dst++) = color[0];
    *(dst++) = color[3];

    DrawnToBuffer[buffer] = true;
    ObjectBufferName[buffer] = name;
}

void OnObjectBegin()
{
    if (!g_bSkipCurrentFrame)
    {
        if (g_Config.bDumpTextures && stats.thisFrame.numDrawnObjects >= g_Config.drawStart && stats.thisFrame.numDrawnObjects < g_Config.drawEnd)
            DumpActiveTextures();

        if (g_Config.bHwRasterizer)
            HwRasterizer::BeginTriangles();
    }
}

void OnObjectEnd()
{
    if (!g_bSkipCurrentFrame)
    {
        if (g_Config.bDumpObjects && stats.thisFrame.numDrawnObjects >= g_Config.drawStart && stats.thisFrame.numDrawnObjects < g_Config.drawEnd)
            DumpEfb(StringFromFormat("%s/object%i.tga", FULL_FRAMES_DIR, stats.thisFrame.numDrawnObjects).c_str());

        if (g_Config.bHwRasterizer)
            HwRasterizer::EndTriangles();

        for (int i = 0; i < NumObjectBuffers; i++)
        {
            if (DrawnToBuffer[i])
            {
                DrawnToBuffer[i] = false;
                SaveTGA(StringFromFormat("%s/object%i_%s(%i).tga", FULL_FRAMES_DIR,
                    stats.thisFrame.numDrawnObjects, ObjectBufferName[i], i).c_str(), EFB_WIDTH, EFB_HEIGHT, ObjectBuffer[i]);
                memset(ObjectBuffer[i], 0, sizeof(ObjectBuffer[i]));
            }
        }

        stats.thisFrame.numDrawnObjects++;
    }
}

void OnFrameEnd()
{
    if (!g_bSkipCurrentFrame)
    {
        if (g_Config.bDumpFrames)
        {
            DumpEfb(StringFromFormat("%s/frame%i_color.tga", FULL_FRAMES_DIR, stats.frameCount).c_str());
            DumpDepth(StringFromFormat("%s/frame%i_depth.tga", FULL_FRAMES_DIR, stats.frameCount).c_str());
        }
    }
}

}