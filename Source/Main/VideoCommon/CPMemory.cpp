// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/CPMemory.h"
#include "Common/ChunkFile.h"

// CP state
CPState g_main_cp_state;
CPState g_preprocess_cp_state;

void DoCPState(PointerWrap& p)
{
  // We don't save g_preprocess_cp_state separately because the GPU should be
  // synced around state save/load.
  p.DoArray(g_main_cp_state.array_bases);
  p.DoArray(g_main_cp_state.array_strides);
  p.Do(g_main_cp_state.matrix_index_a);
  p.Do(g_main_cp_state.matrix_index_b);
  p.Do(g_main_cp_state.vtx_desc.Hex);
  p.DoArray(g_main_cp_state.vtx_attr);
  p.DoMarker("CP Memory");
  if (p.mode == PointerWrap::MODE_READ)
  {
    CopyPreprocessCPStateFromMain();
    g_main_cp_state.bases_dirty = true;
  }
}

void CopyPreprocessCPStateFromMain()
{
  memcpy(&g_preprocess_cp_state, &g_main_cp_state, sizeof(CPState));
}
