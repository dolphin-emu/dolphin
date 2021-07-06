// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  p.Do(g_main_cp_state.vtx_desc);
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

std::pair<std::string, std::string> GetCPRegInfo(u8 cmd, u32 value)
{
  switch (cmd & CP_COMMAND_MASK)
  {
  case MATINDEX_A:
    return std::make_pair("MATINDEX_A", fmt::to_string(TMatrixIndexA{.Hex = value}));
  case MATINDEX_B:
    return std::make_pair("MATINDEX_B", fmt::to_string(TMatrixIndexB{.Hex = value}));
  case VCD_LO:
    return std::make_pair("VCD_LO", fmt::to_string(TVtxDesc::Low{.Hex = value}));
  case VCD_HI:
    return std::make_pair("VCD_HI", fmt::to_string(TVtxDesc::High{.Hex = value}));
  case CP_VAT_REG_A:
    if (cmd - CP_VAT_REG_A >= CP_NUM_VAT_REG)
      return std::make_pair("CP_VAT_REG_A invalid", "");

    return std::make_pair(fmt::format("CP_VAT_REG_A - Format {}", cmd & CP_VAT_MASK),
                          fmt::to_string(UVAT_group0{.Hex = value}));
  case CP_VAT_REG_B:
    if (cmd - CP_VAT_REG_B >= CP_NUM_VAT_REG)
      return std::make_pair("CP_VAT_REG_B invalid", "");

    return std::make_pair(fmt::format("CP_VAT_REG_B - Format {}", cmd & CP_VAT_MASK),
                          fmt::to_string(UVAT_group1{.Hex = value}));
  case CP_VAT_REG_C:
    if (cmd - CP_VAT_REG_C >= CP_NUM_VAT_REG)
      return std::make_pair("CP_VAT_REG_C invalid", "");

    return std::make_pair(fmt::format("CP_VAT_REG_C - Format {}", cmd & CP_VAT_MASK),
                          fmt::to_string(UVAT_group2{.Hex = value}));
  case ARRAY_BASE:
    return std::make_pair(fmt::format("ARRAY_BASE Array {}", cmd & CP_ARRAY_MASK),
                          fmt::format("Base address {:08x}", value));
  case ARRAY_STRIDE:
    return std::make_pair(fmt::format("ARRAY_STRIDE Array {}", cmd - ARRAY_STRIDE),
                          fmt::format("Stride {:02x}", value & 0xff));
  default:
    return std::make_pair(fmt::format("Invalid CP register {:02x} = {:08x}", cmd, value), "");
  }
}
