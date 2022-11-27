// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoState.h"

#include <cstring>

#include "Common/ChunkFile.h"
#include "Core/System.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TMEM.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/XFMemory.h"

void VideoCommon_DoState(PointerWrap& p)
{
  bool software = false;
  p.Do(software);

  if (p.IsReadMode() && software == true)
  {
    // change mode to abort load of incompatible save state.
    p.SetVerifyMode();
  }

  // BP Memory
  p.Do(bpmem);
  p.DoMarker("BP Memory");

  // CP Memory
  // We don't save g_preprocess_cp_state separately because the GPU should be
  // synced around state save/load.
  p.Do(g_main_cp_state);
  p.DoMarker("CP Memory");
  if (p.IsReadMode())
    CopyPreprocessCPStateFromMain();

  // XF Memory
  p.Do(xfmem);
  p.DoMarker("XF Memory");

  // Texture decoder
  p.DoArray(texMem);
  p.DoMarker("texMem");

  // TMEM
  TMEM::DoState(p);
  p.DoMarker("TMEM");

  // FIFO
  Fifo::DoState(p);
  p.DoMarker("Fifo");

  auto& system = Core::System::GetInstance();
  auto& command_processor = system.GetCommandProcessor();
  command_processor.DoState(p);
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

  g_framebuffer_manager->DoState(p);
  p.DoMarker("FramebufferManager");

  g_texture_cache->DoState(p);
  p.DoMarker("TextureCache");

  g_renderer->DoState(p);
  p.DoMarker("Renderer");

  // Refresh state.
  if (p.IsReadMode())
  {
    // Inform backend of new state from registers.
    BPReload();
    VertexLoaderManager::MarkAllDirty();
  }
}
