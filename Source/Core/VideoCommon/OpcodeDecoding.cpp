// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// DL facts:
//  Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)
//  Super Mario Galaxy has nearly all geometry and more than half of the state in DLs (great!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they
// are while interpreting them, and hope that the vertex format doesn't change, though, if you do
// it right when they are called. The reason is that the vertex format affects the sizes of the
// vertices.

#include "VideoCommon/OpcodeDecoding.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/XFMemory.h"

namespace OpcodeDecoder
{
namespace
{
bool s_is_fifo_error_seen = false;

u32 InterpretDisplayList(u32 address, u32 size)
{
  u8* start_address;

  if (Fifo::UseDeterministicGPUThread())
    start_address = static_cast<u8*>(Fifo::PopFifoAuxBuffer(size));
  else
    start_address = Memory::GetPointer(address);

  u32 cycles = 0;

  // Avoid the crash if Memory::GetPointer failed ..
  if (start_address != nullptr)
  {
    // temporarily swap dl and non-dl (small "hack" for the stats)
    g_stats.SwapDL();

    Run(DataReader(start_address, start_address + size), &cycles, true);
    INCSTAT(g_stats.this_frame.num_dlists_called);

    // un-swap
    g_stats.SwapDL();
  }

  return cycles;
}

void InterpretDisplayListPreprocess(u32 address, u32 size)
{
  u8* const start_address = Memory::GetPointer(address);

  Fifo::PushFifoAuxBuffer(start_address, size);

  if (start_address == nullptr)
    return;

  Run<true>(DataReader(start_address, start_address + size), nullptr, true);
}
}  // Anonymous namespace

bool g_record_fifo_data = false;

void Init()
{
  s_is_fifo_error_seen = false;
}

template <bool is_preprocess>
u8* Run(DataReader src, u32* cycles, bool in_display_list)
{
  u32 total_cycles = 0;
  u8* opcode_start = nullptr;

  const auto finish_up = [cycles, &opcode_start, &total_cycles] {
    if (cycles != nullptr)
    {
      *cycles = total_cycles;
    }
    return opcode_start;
  };

  while (true)
  {
    opcode_start = src.GetPointer();

    if (!src.size())
      return finish_up();

    const u8 cmd_byte = src.Read<u8>();
    switch (cmd_byte)
    {
    case GX_NOP:
      total_cycles += 6;  // Hm, this means that we scan over nop streams pretty slowly...
      break;

    case GX_UNKNOWN_RESET:
      total_cycles += 6;  // Datel software uses this command
      DEBUG_LOG(VIDEO, "GX Reset?: %08x", cmd_byte);
      break;

    case GX_LOAD_CP_REG:
    {
      if (src.size() < 1 + 4)
        return finish_up();

      total_cycles += 12;

      const u8 sub_cmd = src.Read<u8>();
      const u32 value = src.Read<u32>();
      LoadCPReg(sub_cmd, value, is_preprocess);
      if constexpr (!is_preprocess)
        INCSTAT(g_stats.this_frame.num_cp_loads);
    }
    break;

    case GX_LOAD_XF_REG:
    {
      if (src.size() < 4)
        return finish_up();

      const u32 cmd2 = src.Read<u32>();
      const u32 transfer_size = ((cmd2 >> 16) & 15) + 1;
      if (src.size() < transfer_size * sizeof(u32))
        return finish_up();

      total_cycles += 18 + 6 * transfer_size;

      if constexpr (!is_preprocess)
      {
        const u32 xf_address = cmd2 & 0xFFFF;
        LoadXFReg(transfer_size, xf_address, src);

        INCSTAT(g_stats.this_frame.num_xf_loads);
      }
      src.Skip<u32>(transfer_size);
    }
    break;

    case GX_LOAD_INDX_A:  // Used for position matrices
    case GX_LOAD_INDX_B:  // Used for normal matrices
    case GX_LOAD_INDX_C:  // Used for postmatrices
    case GX_LOAD_INDX_D:  // Used for lights
    {
      if (src.size() < 4)
        return finish_up();

      total_cycles += 6;

      // Map the command byte to its ref array.
      // GX_LOAD_INDX_A (32) -> 0xC
      // GX_LOAD_INDX_B (40) -> 0xD
      // GX_LOAD_INDX_C (48) -> 0xE
      // GX_LOAD_INDX_D (56) -> 0xF
      const int ref_array = (cmd_byte / 8) + 8;

      if constexpr (is_preprocess)
        PreprocessIndexedXF(src.Read<u32>(), ref_array);
      else
        LoadIndexedXF(src.Read<u32>(), ref_array);
    }
    break;

    case GX_CMD_CALL_DL:
    {
      if (src.size() < 8)
        return finish_up();

      const u32 address = src.Read<u32>();
      const u32 count = src.Read<u32>();

      if (in_display_list)
      {
        total_cycles += 6;
        INFO_LOG(VIDEO, "recursive display list detected");
      }
      else
      {
        if constexpr (is_preprocess)
          InterpretDisplayListPreprocess(address, count);
        else
          total_cycles += 6 + InterpretDisplayList(address, count);
      }
    }
    break;

    case GX_CMD_UNKNOWN_METRICS:  // zelda 4 swords calls it and checks the metrics registers after
                                  // that
      total_cycles += 6;
      DEBUG_LOG(VIDEO, "GX 0x44: %08x", cmd_byte);
      break;

    case GX_CMD_INVL_VC:  // Invalidate Vertex Cache
      total_cycles += 6;
      DEBUG_LOG(VIDEO, "Invalidate (vertex cache?)");
      break;

    case GX_LOAD_BP_REG:
      // In skipped_frame case: We have to let BP writes through because they set
      // tokens and stuff.  TODO: Call a much simplified LoadBPReg instead.
      {
        if (src.size() < 4)
          return finish_up();

        total_cycles += 12;

        const u32 bp_cmd = src.Read<u32>();
        if constexpr (is_preprocess)
        {
          LoadBPRegPreprocess(bp_cmd);
        }
        else
        {
          LoadBPReg(bp_cmd);
          INCSTAT(g_stats.this_frame.num_bp_loads);
        }
      }
      break;

    // draw primitives
    default:
      if ((cmd_byte & 0xC0) == 0x80)
      {
        // load vertices
        if (src.size() < 2)
          return finish_up();

        const u16 num_vertices = src.Read<u16>();
        const int bytes = VertexLoaderManager::RunVertices(
            cmd_byte & GX_VAT_MASK,  // Vertex loader index (0 - 7)
            (cmd_byte & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT, num_vertices, src, is_preprocess);

        if (bytes < 0)
          return finish_up();

        src.Skip(bytes);

        // 4 GPU ticks per vertex, 3 CPU ticks per GPU tick
        total_cycles += num_vertices * 4 * 3 + 6;
      }
      else
      {
        if (!s_is_fifo_error_seen)
          CommandProcessor::HandleUnknownOpcode(cmd_byte, opcode_start, is_preprocess);
        ERROR_LOG(VIDEO, "FIFO: Unknown Opcode(0x%02x @ %p, preprocessing = %s)", cmd_byte,
                  opcode_start, is_preprocess ? "yes" : "no");
        s_is_fifo_error_seen = true;
        total_cycles += 1;
      }
      break;
    }

    // Display lists get added directly into the FIFO stream
    if constexpr (!is_preprocess)
    {
      if (g_record_fifo_data && cmd_byte != GX_CMD_CALL_DL)
      {
        const u8* const opcode_end = src.GetPointer();
        FifoRecorder::GetInstance().WriteGPCommand(opcode_start, u32(opcode_end - opcode_start));
      }
    }
  }
}

template u8* Run<true>(DataReader src, u32* cycles, bool in_display_list);
template u8* Run<false>(DataReader src, u32* cycles, bool in_display_list);

}  // namespace OpcodeDecoder
