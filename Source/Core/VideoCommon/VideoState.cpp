// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoState.h"
#include "VideoCommon/XFMemory.h"

void VideoCommon_DoState(PointerWrap& p)
{
  bool software = false;
  p.Do(software);

  if (p.GetMode() == PointerWrap::MODE_READ && software == true)
  {
    // change mode to abort load of incompatible save state.
    p.SetMode(PointerWrap::MODE_VERIFY);
  }

  // BP Memory
  p.Do(bpmem);
  p.DoMarker("BP Memory");

  // CP Memory
  DoCPState(p);

  // XF Memory
  p.Do(xfmem);
  p.DoMarker("XF Memory");

  // Texture decoder
  p.DoArray(texMem);
  p.DoMarker("texMem");

  // FIFO
  Fifo::DoState(p);
  p.DoMarker("Fifo");

  CommandProcessor::DoState(p);
  p.DoMarker("CommandProcessor");

  PixelEngine::DoState(p);
  p.DoMarker("PixelEngine");

  // the old way of replaying current bpmem as writes to push side effects to pixel shader manager
  // doesn't really work.
  PixelShaderManager::DoState(p);
  p.DoMarker("PixelShaderManager");

  VertexShaderManager::DoState(p);
  p.DoMarker("VertexShaderManager");

  GeometryShaderManager::DoState(p);
  p.DoMarker("GeometryShaderManager");

  g_vertex_manager->DoState(p);
  p.DoMarker("VertexManager");

  BoundingBox::DoState(p);
  p.DoMarker("BoundingBox");

  g_framebuffer_manager->DoState(p);
  p.DoMarker("FramebufferManager");

  g_texture_cache->DoState(p);
  p.DoMarker("TextureCache");

  g_renderer->DoState(p);
  p.DoMarker("Renderer");

  // Refresh state.
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // Inform backend of new state from registers.
    BPReload();
  }
}
