// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/DebugUtil.h"

#include <cstring>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Image.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/TextureSampler.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DebugUtil
{
static const int NUM_OBJECT_BUFFERS = 40;

static u32* ObjectBuffer[NUM_OBJECT_BUFFERS];
static u32 TempBuffer[NUM_OBJECT_BUFFERS];

static bool DrawnToBuffer[NUM_OBJECT_BUFFERS];
static const char* ObjectBufferName[NUM_OBJECT_BUFFERS];
static int BufferBase[NUM_OBJECT_BUFFERS];

void Init()
{
  for (int i = 0; i < NUM_OBJECT_BUFFERS; i++)
  {
    ObjectBuffer[i] = new u32[EFB_WIDTH * EFB_HEIGHT]();
    DrawnToBuffer[i] = false;
    ObjectBufferName[i] = nullptr;
    BufferBase[i] = 0;
  }
}

void Shutdown()
{
  for (auto& slot : ObjectBuffer)
  {
    delete[] slot;
  }
}

static void SaveTexture(const std::string& filename, u32 texmap, s32 mip)
{
  u32 width = bpmem.tex.GetUnit(texmap).texImage0.width + 1;
  u32 height = bpmem.tex.GetUnit(texmap).texImage0.height + 1;

  auto data = std::make_unique<u8[]>(width * height * 4);

  GetTextureRGBA(data.get(), texmap, mip, width, height);
  Common::SavePNG(filename, data.get(), Common::ImageByteFormat::RGBA, width, height, width * 4);
}

void GetTextureRGBA(u8* dst, u32 texmap, s32 mip, u32 width, u32 height)
{
  for (u32 y = 0; y < height; y++)
  {
    for (u32 x = 0; x < width; x++)
    {
      TextureSampler::SampleMip(x << 7, y << 7, mip, false, texmap, dst);
      dst += 4;
    }
  }
}

static s32 GetMaxTextureLod(u32 texmap)
{
  u8 maxLod = bpmem.tex.GetUnit(texmap).texMode1.max_lod;
  u8 mip = maxLod >> 4;
  u8 fract = maxLod & 0xf;

  if (fract)
    ++mip;

  return (s32)mip;
}

void DumpActiveTextures()
{
  for (unsigned int stageNum = 0; stageNum < bpmem.genMode.numindstages; stageNum++)
  {
    u32 texmap = bpmem.tevindref.getTexMap(stageNum);

    s32 maxLod = GetMaxTextureLod(texmap);
    for (s32 mip = 0; mip <= maxLod; ++mip)
    {
      SaveTexture(StringFromFormat("%star%i_ind%i_map%i_mip%i.png",
                                   File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
                                   g_stats.this_frame.num_drawn_objects, stageNum, texmap, mip),
                  texmap, mip);
    }
  }

  for (unsigned int stageNum = 0; stageNum <= bpmem.genMode.numtevstages; stageNum++)
  {
    int stageNum2 = stageNum >> 1;
    int stageOdd = stageNum & 1;
    TwoTevStageOrders& order = bpmem.tevorders[stageNum2];

    int texmap = order.getTexMap(stageOdd);

    s32 maxLod = GetMaxTextureLod(texmap);
    for (s32 mip = 0; mip <= maxLod; ++mip)
    {
      SaveTexture(StringFromFormat("%star%i_stage%i_map%i_mip%i.png",
                                   File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
                                   g_stats.this_frame.num_drawn_objects, stageNum, texmap, mip),
                  texmap, mip);
    }
  }
}

static void DumpEfb(const std::string& filename)
{
  auto data = std::make_unique<u8[]>(EFB_WIDTH * EFB_HEIGHT * 4);
  u8* writePtr = data.get();

  for (u32 y = 0; y < EFB_HEIGHT; y++)
  {
    for (u32 x = 0; x < EFB_WIDTH; x++)
    {
      // ABGR to RGBA
      const u32 sample = Common::swap32(EfbInterface::GetColor(x, y));

      std::memcpy(writePtr, &sample, sizeof(u32));
      writePtr += sizeof(u32);
    }
  }

  Common::SavePNG(filename, data.get(), Common::ImageByteFormat::RGBA, EFB_WIDTH, EFB_HEIGHT,
                  EFB_WIDTH * 4);
}

void DrawObjectBuffer(s16 x, s16 y, const u8* color, int bufferBase, int subBuffer,
                      const char* name)
{
  int buffer = bufferBase + subBuffer;

  u32 offset = (x + y * EFB_WIDTH) * 4;
  u8* dst = (u8*)&ObjectBuffer[buffer][offset];
  *(dst++) = color[2];
  *(dst++) = color[1];
  *(dst++) = color[0];
  *(dst++) = color[3];

  DrawnToBuffer[buffer] = true;
  ObjectBufferName[buffer] = name;
  BufferBase[buffer] = bufferBase;
}

void DrawTempBuffer(const u8* color, int buffer)
{
  u8* dst = (u8*)&TempBuffer[buffer];
  *(dst++) = color[2];
  *(dst++) = color[1];
  *(dst++) = color[0];
  *(dst++) = color[3];
}

void CopyTempBuffer(s16 x, s16 y, int bufferBase, int subBuffer, const char* name)
{
  int buffer = bufferBase + subBuffer;

  u32 offset = (x + y * EFB_WIDTH);
  ObjectBuffer[buffer][offset] = TempBuffer[buffer];

  DrawnToBuffer[buffer] = true;
  ObjectBufferName[buffer] = name;
  BufferBase[buffer] = bufferBase;
}

void OnObjectBegin()
{
  if (g_ActiveConfig.bDumpTextures &&
      g_stats.this_frame.num_drawn_objects >= g_ActiveConfig.drawStart &&
      g_stats.this_frame.num_drawn_objects < g_ActiveConfig.drawEnd)
  {
    DumpActiveTextures();
  }
}

void OnObjectEnd()
{
  if (g_ActiveConfig.bDumpObjects &&
      g_stats.this_frame.num_drawn_objects >= g_ActiveConfig.drawStart &&
      g_stats.this_frame.num_drawn_objects < g_ActiveConfig.drawEnd)
  {
    DumpEfb(StringFromFormat("%sobject%i.png", File::GetUserPath(D_DUMPOBJECTS_IDX).c_str(),
                             g_stats.this_frame.num_drawn_objects));
  }

  for (int i = 0; i < NUM_OBJECT_BUFFERS; i++)
  {
    if (DrawnToBuffer[i])
    {
      DrawnToBuffer[i] = false;
      std::string filename = StringFromFormat(
          "%sobject%i_%s(%i).png", File::GetUserPath(D_DUMPOBJECTS_IDX).c_str(),
          g_stats.this_frame.num_drawn_objects, ObjectBufferName[i], i - BufferBase[i]);

      Common::SavePNG(filename, reinterpret_cast<u8*>(ObjectBuffer[i]),
                      Common::ImageByteFormat::RGBA, EFB_WIDTH, EFB_HEIGHT, EFB_WIDTH * 4);
      memset(ObjectBuffer[i], 0, EFB_WIDTH * EFB_HEIGHT * sizeof(u32));
    }
  }

  g_stats.this_frame.num_drawn_objects++;
}
}  // namespace DebugUtil
