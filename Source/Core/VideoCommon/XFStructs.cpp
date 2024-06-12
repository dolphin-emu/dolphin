// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/XFStructs.h"

#include <bit>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/XFStateManager.h"

static void XFMemWritten(XFStateManager& xf_state_manager, u32 transferSize, u32 baseAddress)
{
  g_vertex_manager->Flush();
  xf_state_manager.InvalidateXFRange(baseAddress, baseAddress + transferSize);
}

static void XFRegWritten(Core::System& system, XFStateManager& xf_state_manager, u32 address,
                         u32 value)
{
  if (address >= XFMEM_REGISTERS_START && address < XFMEM_REGISTERS_END)
  {
    switch (address)
    {
    case XFMEM_ERROR:
    case XFMEM_DIAG:
    case XFMEM_STATE0:  // internal state 0
    case XFMEM_STATE1:  // internal state 1
    case XFMEM_CLOCK:
    case XFMEM_SETGPMETRIC:
      // Not implemented
      break;

    case XFMEM_CLIPDISABLE:
    {
      ClipDisable setting{.hex = value};
      if (setting.disable_clipping_detection)
        DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::SETS_XF_CLIPDISABLE_BIT_0);
      if (setting.disable_trivial_rejection)
        DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::SETS_XF_CLIPDISABLE_BIT_1);
      if (setting.disable_copy_clipping_acceleration)
        DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::SETS_XF_CLIPDISABLE_BIT_2);
      break;
    }

    case XFMEM_VTXSPECS:  //__GXXfVtxSpecs, wrote 0004
      VertexLoaderManager::g_needs_cp_xf_consistency_check = true;
      break;

    case XFMEM_SETNUMCHAN:
      if (xfmem.numChan.numColorChans != (value & 3))
        g_vertex_manager->Flush();
      xf_state_manager.SetLightingConfigChanged();
      break;

    case XFMEM_SETCHAN0_AMBCOLOR:  // Channel Ambient Color
    case XFMEM_SETCHAN1_AMBCOLOR:
    {
      u8 chan = address - XFMEM_SETCHAN0_AMBCOLOR;
      if (xfmem.ambColor[chan] != value)
      {
        g_vertex_manager->Flush();
        xf_state_manager.SetMaterialColorChanged(chan);
      }
      break;
    }

    case XFMEM_SETCHAN0_MATCOLOR:  // Channel Material Color
    case XFMEM_SETCHAN1_MATCOLOR:
    {
      u8 chan = address - XFMEM_SETCHAN0_MATCOLOR;
      if (xfmem.matColor[chan] != value)
      {
        g_vertex_manager->Flush();
        xf_state_manager.SetMaterialColorChanged(chan + 2);
      }
      break;
    }

    case XFMEM_SETCHAN0_COLOR:  // Channel Color
    case XFMEM_SETCHAN1_COLOR:
    case XFMEM_SETCHAN0_ALPHA:  // Channel Alpha
    case XFMEM_SETCHAN1_ALPHA:
      if (((u32*)&xfmem)[address] != (value & 0x7fff))
        g_vertex_manager->Flush();
      xf_state_manager.SetLightingConfigChanged();
      break;

    case XFMEM_DUALTEX:
      if (xfmem.dualTexTrans.enabled != bool(value & 1))
        g_vertex_manager->Flush();
      xf_state_manager.SetTexMatrixInfoChanged(-1);
      break;

    case XFMEM_SETMATRIXINDA:
      xf_state_manager.SetTexMatrixChangedA(value);
      VertexLoaderManager::g_needs_cp_xf_consistency_check = true;
      break;
    case XFMEM_SETMATRIXINDB:
      xf_state_manager.SetTexMatrixChangedB(value);
      VertexLoaderManager::g_needs_cp_xf_consistency_check = true;
      break;

    case XFMEM_SETVIEWPORT:
    case XFMEM_SETVIEWPORT + 1:
    case XFMEM_SETVIEWPORT + 2:
    case XFMEM_SETVIEWPORT + 3:
    case XFMEM_SETVIEWPORT + 4:
    case XFMEM_SETVIEWPORT + 5:
      g_vertex_manager->Flush();
      xf_state_manager.SetViewportChanged();
      system.GetPixelShaderManager().SetViewportChanged();
      system.GetGeometryShaderManager().SetViewportChanged();
      break;

    case XFMEM_SETPROJECTION:
    case XFMEM_SETPROJECTION + 1:
    case XFMEM_SETPROJECTION + 2:
    case XFMEM_SETPROJECTION + 3:
    case XFMEM_SETPROJECTION + 4:
    case XFMEM_SETPROJECTION + 5:
    case XFMEM_SETPROJECTION + 6:
      g_vertex_manager->Flush();
      xf_state_manager.SetProjectionChanged();
      system.GetGeometryShaderManager().SetProjectionChanged();
      break;

    case XFMEM_SETNUMTEXGENS:  // GXSetNumTexGens
      if (xfmem.numTexGen.numTexGens != (value & 15))
        g_vertex_manager->Flush();
      break;

    case XFMEM_SETTEXMTXINFO:
    case XFMEM_SETTEXMTXINFO + 1:
    case XFMEM_SETTEXMTXINFO + 2:
    case XFMEM_SETTEXMTXINFO + 3:
    case XFMEM_SETTEXMTXINFO + 4:
    case XFMEM_SETTEXMTXINFO + 5:
    case XFMEM_SETTEXMTXINFO + 6:
    case XFMEM_SETTEXMTXINFO + 7:
      g_vertex_manager->Flush();
      xf_state_manager.SetTexMatrixInfoChanged(address - XFMEM_SETTEXMTXINFO);
      break;

    case XFMEM_SETPOSTMTXINFO:
    case XFMEM_SETPOSTMTXINFO + 1:
    case XFMEM_SETPOSTMTXINFO + 2:
    case XFMEM_SETPOSTMTXINFO + 3:
    case XFMEM_SETPOSTMTXINFO + 4:
    case XFMEM_SETPOSTMTXINFO + 5:
    case XFMEM_SETPOSTMTXINFO + 6:
    case XFMEM_SETPOSTMTXINFO + 7:
      g_vertex_manager->Flush();
      xf_state_manager.SetTexMatrixInfoChanged(address - XFMEM_SETPOSTMTXINFO);
      break;

    // --------------
    // Unknown Regs
    // --------------

    // Maybe these are for Normals?
    case 0x1048:  // xfmem.texcoords[0].nrmmtxinfo.hex = data; break; ??
    case 0x1049:
    case 0x104a:
    case 0x104b:
    case 0x104c:
    case 0x104d:
    case 0x104e:
    case 0x104f:
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNKNOWN_XF_COMMAND);
      DEBUG_LOG_FMT(VIDEO, "Possible Normal Mtx XF reg?: {:x}={:x}", address, value);
      break;

    case 0x1013:
    case 0x1014:
    case 0x1015:
    case 0x1016:
    case 0x1017:

    default:
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNKNOWN_XF_COMMAND);
      WARN_LOG_FMT(VIDEO, "Unknown XF Reg: {:x}={:x}", address, value);
      break;
    }
  }
}

void LoadXFReg(u16 base_address, u8 transfer_size, const u8* data)
{
  if (base_address > XFMEM_REGISTERS_END)
  {
    WARN_LOG_FMT(VIDEO, "XF load base address past end of address space: {:x} {} bytes",
                 base_address, transfer_size);
    return;
  }

  u32 end_address = base_address + transfer_size;  // exclusive

  // do not allow writes past registers
  if (end_address > XFMEM_REGISTERS_END)
  {
    WARN_LOG_FMT(VIDEO, "XF load ends past end of address space: {:x} {} bytes", base_address,
                 transfer_size);
    end_address = XFMEM_REGISTERS_END;
  }

  auto& system = Core::System::GetInstance();
  auto& xf_state_manager = system.GetXFStateManager();

  // write to XF mem
  if (base_address < XFMEM_REGISTERS_START)
  {
    const u32 xf_mem_base = base_address;
    u32 xf_mem_transfer_size = transfer_size;

    if (end_address > XFMEM_REGISTERS_START)
    {
      xf_mem_transfer_size = XFMEM_REGISTERS_START - base_address;
      base_address = XFMEM_REGISTERS_START;
    }

    XFMemWritten(xf_state_manager, xf_mem_transfer_size, xf_mem_base);
    for (u32 i = 0; i < xf_mem_transfer_size; i++)
    {
      ((u32*)&xfmem)[xf_mem_base + i] = Common::swap32(data);
      data += 4;
    }
  }

  // write to XF regs
  if (base_address >= XFMEM_REGISTERS_START)
  {
    for (u32 address = base_address; address < end_address; address++)
    {
      const u32 value = Common::swap32(data);

      XFRegWritten(system, xf_state_manager, address, value);
      ((u32*)&xfmem)[address] = value;

      data += 4;
    }
  }
}

// TODO - verify that it is correct. Seems to work, though.
void LoadIndexedXF(CPArray array, u32 index, u16 address, u8 size)
{
  // load stuff from array to address in xf mem

  const u32 buf_size = size * sizeof(u32);
  u32* currData = reinterpret_cast<u32*>(&xfmem) + address;
  u32* newData;
  auto& system = Core::System::GetInstance();
  auto& fifo = system.GetFifo();
  if (fifo.UseDeterministicGPUThread())
  {
    newData = reinterpret_cast<u32*>(fifo.PopFifoAuxBuffer(buf_size));
  }
  else
  {
    auto& memory = system.GetMemory();
    newData = reinterpret_cast<u32*>(memory.GetPointerForRange(
        g_main_cp_state.array_bases[array] + g_main_cp_state.array_strides[array] * index,
        buf_size));
  }

  auto& xf_state_manager = system.GetXFStateManager();
  bool changed = false;
  for (u32 i = 0; i < size; ++i)
  {
    if (currData[i] != Common::swap32(newData[i]))
    {
      changed = true;
      XFMemWritten(xf_state_manager, size, address);
      break;
    }
  }
  if (changed)
  {
    for (u32 i = 0; i < size; ++i)
      currData[i] = Common::swap32(newData[i]);
  }
}

void PreprocessIndexedXF(CPArray array, u32 index, u16 address, u8 size)
{
  const size_t buf_size = size * sizeof(u32);

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  const u8* new_data = memory.GetPointerForRange(
      g_preprocess_cp_state.array_bases[array] + g_preprocess_cp_state.array_strides[array] * index,
      buf_size);

  system.GetFifo().PushFifoAuxBuffer(new_data, buf_size);
}

std::pair<std::string, std::string> GetXFRegInfo(u32 address, u32 value)
{
// Macro to set the register name and make sure it was written correctly via compile time assertion
#define RegName(reg) ((void)(reg), #reg)
#define DescriptionlessReg(reg) std::make_pair(RegName(reg), "");

  switch (address)
  {
  case XFMEM_ERROR:
    return DescriptionlessReg(XFMEM_ERROR);
  case XFMEM_DIAG:
    return DescriptionlessReg(XFMEM_DIAG);
  case XFMEM_STATE0:  // internal state 0
    return std::make_pair(RegName(XFMEM_STATE0), "internal state 0");
  case XFMEM_STATE1:  // internal state 1
    return std::make_pair(RegName(XFMEM_STATE1), "internal state 1");
  case XFMEM_CLOCK:
    return DescriptionlessReg(XFMEM_CLOCK);
  case XFMEM_SETGPMETRIC:
    return DescriptionlessReg(XFMEM_SETGPMETRIC);

  case XFMEM_CLIPDISABLE:
    return std::make_pair(RegName(XFMEM_CLIPDISABLE), fmt::to_string(ClipDisable{.hex = value}));

  case XFMEM_VTXSPECS:
    return std::make_pair(RegName(XFMEM_VTXSPECS), fmt::to_string(INVTXSPEC{.hex = value}));

  case XFMEM_SETNUMCHAN:
    return std::make_pair(RegName(XFMEM_SETNUMCHAN),
                          fmt::format("Number of color channels: {}", value & 3));
    break;

  case XFMEM_SETCHAN0_AMBCOLOR:
    return std::make_pair(RegName(XFMEM_SETCHAN0_AMBCOLOR),
                          fmt::format("Channel 0 Ambient Color: {:08x}", value));
  case XFMEM_SETCHAN1_AMBCOLOR:
    return std::make_pair(RegName(XFMEM_SETCHAN1_AMBCOLOR),
                          fmt::format("Channel 1 Ambient Color: {:08x}", value));

  case XFMEM_SETCHAN0_MATCOLOR:
    return std::make_pair(RegName(XFMEM_SETCHAN0_MATCOLOR),
                          fmt::format("Channel 0 Material Color: {:08x}", value));
  case XFMEM_SETCHAN1_MATCOLOR:
    return std::make_pair(RegName(XFMEM_SETCHAN1_MATCOLOR),
                          fmt::format("Channel 1 Material Color: {:08x}", value));

  case XFMEM_SETCHAN0_COLOR:  // Channel Color
    return std::make_pair(RegName(XFMEM_SETCHAN0_COLOR),
                          fmt::format("Channel 0 Color config:\n{}", LitChannel{.hex = value}));
  case XFMEM_SETCHAN1_COLOR:
    return std::make_pair(RegName(XFMEM_SETCHAN1_COLOR),
                          fmt::format("Channel 1 Color config:\n{}", LitChannel{.hex = value}));
  case XFMEM_SETCHAN0_ALPHA:  // Channel Alpha
    return std::make_pair(RegName(XFMEM_SETCHAN0_ALPHA),
                          fmt::format("Channel 0 Alpha config:\n{}", LitChannel{.hex = value}));
  case XFMEM_SETCHAN1_ALPHA:
    return std::make_pair(RegName(XFMEM_SETCHAN1_ALPHA),
                          fmt::format("Channel 1 Alpha config:\n{}", LitChannel{.hex = value}));

  case XFMEM_DUALTEX:
    return std::make_pair(RegName(XFMEM_DUALTEX),
                          fmt::format("Dual Tex Trans {}", (value & 1) ? "enabled" : "disabled"));

  case XFMEM_SETMATRIXINDA:
    return std::make_pair(RegName(XFMEM_SETMATRIXINDA),
                          fmt::format("Matrix index A:\n{}", TMatrixIndexA{.Hex = value}));
  case XFMEM_SETMATRIXINDB:
    return std::make_pair(RegName(XFMEM_SETMATRIXINDB),
                          fmt::format("Matrix index B:\n{}", TMatrixIndexB{.Hex = value}));

  case XFMEM_SETVIEWPORT:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 0),
                          fmt::format("Viewport width: {}", std::bit_cast<float>(value)));
  case XFMEM_SETVIEWPORT + 1:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 1),
                          fmt::format("Viewport height: {}", std::bit_cast<float>(value)));
  case XFMEM_SETVIEWPORT + 2:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 2),
                          fmt::format("Viewport z range: {}", std::bit_cast<float>(value)));
  case XFMEM_SETVIEWPORT + 3:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 3),
                          fmt::format("Viewport x origin: {}", std::bit_cast<float>(value)));
  case XFMEM_SETVIEWPORT + 4:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 4),
                          fmt::format("Viewport y origin: {}", std::bit_cast<float>(value)));
  case XFMEM_SETVIEWPORT + 5:
    return std::make_pair(RegName(XFMEM_SETVIEWPORT + 5),
                          fmt::format("Viewport far z: {}", std::bit_cast<float>(value)));
    break;

  case XFMEM_SETPROJECTION:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 0),
                          fmt::format("Projection[0]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 1:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 1),
                          fmt::format("Projection[1]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 2:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 2),
                          fmt::format("Projection[2]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 3:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 3),
                          fmt::format("Projection[3]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 4:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 4),
                          fmt::format("Projection[4]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 5:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 5),
                          fmt::format("Projection[5]: {}", std::bit_cast<float>(value)));
  case XFMEM_SETPROJECTION + 6:
    return std::make_pair(RegName(XFMEM_SETPROJECTION + 6),
                          fmt::to_string(static_cast<ProjectionType>(value)));

  case XFMEM_SETNUMTEXGENS:
    return std::make_pair(RegName(XFMEM_SETNUMTEXGENS),
                          fmt::format("Number of tex gens: {}", value & 15));

  case XFMEM_SETTEXMTXINFO:
  case XFMEM_SETTEXMTXINFO + 1:
  case XFMEM_SETTEXMTXINFO + 2:
  case XFMEM_SETTEXMTXINFO + 3:
  case XFMEM_SETTEXMTXINFO + 4:
  case XFMEM_SETTEXMTXINFO + 5:
  case XFMEM_SETTEXMTXINFO + 6:
  case XFMEM_SETTEXMTXINFO + 7:
    return std::make_pair(
        fmt::format("XFMEM_SETTEXMTXINFO Matrix {}", address - XFMEM_SETTEXMTXINFO),
        fmt::to_string(TexMtxInfo{.hex = value}));

  case XFMEM_SETPOSTMTXINFO:
  case XFMEM_SETPOSTMTXINFO + 1:
  case XFMEM_SETPOSTMTXINFO + 2:
  case XFMEM_SETPOSTMTXINFO + 3:
  case XFMEM_SETPOSTMTXINFO + 4:
  case XFMEM_SETPOSTMTXINFO + 5:
  case XFMEM_SETPOSTMTXINFO + 6:
  case XFMEM_SETPOSTMTXINFO + 7:
    return std::make_pair(
        fmt::format("XFMEM_SETPOSTMTXINFO Matrix {}", address - XFMEM_SETPOSTMTXINFO),
        fmt::to_string(PostMtxInfo{.hex = value}));

  // --------------
  // Unknown Regs
  // --------------

  // Maybe these are for Normals?
  case 0x1048:  // xfmem.texcoords[0].nrmmtxinfo.hex = data; break; ??
  case 0x1049:
  case 0x104a:
  case 0x104b:
  case 0x104c:
  case 0x104d:
  case 0x104e:
  case 0x104f:
    return std::make_pair(
        fmt::format("Possible Normal Mtx XF reg?: {:x}={:x}", address, value),
        "Maybe these are for Normals? xfmem.texcoords[0].nrmmtxinfo.hex = data; break; ??");
    break;

  case 0x1013:
  case 0x1014:
  case 0x1015:
  case 0x1016:
  case 0x1017:

  default:
    return std::make_pair(fmt::format("Unknown XF Reg: {:x}={:x}", address, value), "");
  }
#undef RegName
#undef DescriptionlessReg
}

std::string GetXFMemName(u32 address)
{
  if (address >= XFMEM_POSMATRICES && address < XFMEM_POSMATRICES_END)
  {
    const u32 row = (address - XFMEM_POSMATRICES) / 4;
    const u32 col = (address - XFMEM_POSMATRICES) % 4;
    return fmt::format("Position matrix row {:2d} col {:2d}", row, col);
  }
  else if (address >= XFMEM_NORMALMATRICES && address < XFMEM_NORMALMATRICES_END)
  {
    const u32 row = (address - XFMEM_NORMALMATRICES) / 3;
    const u32 col = (address - XFMEM_NORMALMATRICES) % 3;
    return fmt::format("Normal matrix row {:2d} col {:2d}", row, col);
  }
  else if (address >= XFMEM_POSTMATRICES && address < XFMEM_POSTMATRICES_END)
  {
    const u32 row = (address - XFMEM_POSTMATRICES) / 4;
    const u32 col = (address - XFMEM_POSTMATRICES) % 4;
    return fmt::format("Post matrix row {:2d} col {:2d}", row, col);
  }
  else if (address >= XFMEM_LIGHTS && address < XFMEM_LIGHTS_END)
  {
    const u32 light = (address - XFMEM_LIGHTS) / 16;
    const u32 offset = (address - XFMEM_LIGHTS) % 16;
    switch (offset)
    {
    default:
      return fmt::format("Light {} unused param {}", light, offset);
    case 3:
      return fmt::format("Light {} color", light);
    case 4:
    case 5:
    case 6:
      return fmt::format("Light {} cosine attenuation {}", light, offset - 4);
    case 7:
    case 8:
    case 9:
      return fmt::format("Light {} distance attenuation {}", light, offset - 7);
    case 10:
    case 11:
    case 12:
      // Yagcd says light pos or "inf ldir", while dolphin has a union for dpos and sdir with only
      // dpos being used nowadays. As far as I can tell only the DX9 engine once at
      // Source/Plugins/Plugin_VideoDX9/Src/TransformEngine.cpp used sdir directly...
      return fmt::format("Light {0} {1} position or inf ldir {1}", light, "xyz"[offset - 10]);
    case 13:
    case 14:
    case 15:
      // Yagcd says light dir or "1/2 angle", dolphin has union for ddir or shalfangle.
      // It would make sense if d stood for direction and s for specular, but it's ddir and
      // shalfangle that have the comment "specular lights only", both at the same offset,
      // while dpos and sdir have none...
      return fmt::format("Light {0} {1} direction or half angle {1}", light, "xyz"[offset - 13]);
    }
  }
  else
  {
    return fmt::format("Unknown memory {:04x}", address);
  }
}

std::string GetXFMemDescription(u32 address, u32 value)
{
  if ((address >= XFMEM_POSMATRICES && address < XFMEM_POSMATRICES_END) ||
      (address >= XFMEM_NORMALMATRICES && address < XFMEM_NORMALMATRICES_END) ||
      (address >= XFMEM_POSTMATRICES && address < XFMEM_POSTMATRICES_END))
  {
    // The matrices all use floats
    return fmt::format("{} = {}", GetXFMemName(address), std::bit_cast<float>(value));
  }
  else if (address >= XFMEM_LIGHTS && address < XFMEM_LIGHTS_END)
  {
    // Each light is 16 words; for this function we don't care which light it is
    const u32 offset = (address - XFMEM_LIGHTS) % 16;
    if (offset <= 3)
    {
      // The unused parameters (0, 1, 2) and the color (3) should be hex-formatted
      return fmt::format("{} = {:08x}", GetXFMemName(address), value);
    }
    else
    {
      // Everything else is a float
      return fmt::format("{} = {}", GetXFMemName(address), std::bit_cast<float>(value));
    }
  }
  else
  {
    // Unknown address
    return fmt::format("{} = {:08x}", GetXFMemName(address), value);
  }
}

std::pair<std::string, std::string> GetXFTransferInfo(u16 base_address, u8 transfer_size,
                                                      const u8* data)
{
  if (base_address > XFMEM_REGISTERS_END)
  {
    return std::make_pair("Invalid XF Transfer", "Base address past end of address space");
  }
  else if (transfer_size == 1 && base_address >= XFMEM_REGISTERS_START)
  {
    // Write directly to a single register
    const u32 value = Common::swap32(data);
    return GetXFRegInfo(base_address, value);
  }

  // More complicated cases
  fmt::memory_buffer name, desc;
  u32 end_address = base_address + transfer_size;  // exclusive

  // do not allow writes past registers
  if (end_address > XFMEM_REGISTERS_END)
  {
    fmt::format_to(std::back_inserter(name), "Invalid XF Transfer ");
    fmt::format_to(std::back_inserter(desc), "Transfer ends past end of address space\n\n");
    end_address = XFMEM_REGISTERS_END;
  }

  // write to XF mem
  if (base_address < XFMEM_REGISTERS_START)
  {
    const u32 xf_mem_base = base_address;
    u32 xf_mem_transfer_size = transfer_size;

    if (end_address > XFMEM_REGISTERS_START)
    {
      xf_mem_transfer_size = XFMEM_REGISTERS_START - base_address;
      base_address = XFMEM_REGISTERS_START;
    }

    fmt::format_to(std::back_inserter(name), "Write {} XF mem words at {:04x}",
                   xf_mem_transfer_size, xf_mem_base);

    for (u32 i = 0; i < xf_mem_transfer_size; i++)
    {
      const auto mem_desc = GetXFMemDescription(xf_mem_base + i, Common::swap32(data));
      fmt::format_to(std::back_inserter(desc), "{}{}", i != 0 ? "\n" : "", mem_desc);
      data += 4;
    }

    if (end_address > XFMEM_REGISTERS_START)
      fmt::format_to(std::back_inserter(name), "; ");
  }

  // write to XF regs
  if (base_address >= XFMEM_REGISTERS_START)
  {
    fmt::format_to(std::back_inserter(name), "Write {} XF regs at {:04x}",
                   end_address - base_address, base_address);

    for (u32 address = base_address; address < end_address; address++)
    {
      const u32 value = Common::swap32(data);

      const auto [regname, regdesc] = GetXFRegInfo(address, value);
      fmt::format_to(std::back_inserter(desc), "{}\n{}\n", regname, regdesc);

      data += 4;
    }
  }

  return std::make_pair(fmt::to_string(name), fmt::to_string(desc));
}

std::pair<std::string, std::string> GetXFIndexedLoadInfo(CPArray array, u32 index, u16 address,
                                                         u8 size)
{
  const auto desc = fmt::format("Load {} bytes to XF address {:03x} from CP array {} row {}", size,
                                address, array, index);
  fmt::memory_buffer written;
  for (u32 i = 0; i < size; i++)
  {
    fmt::format_to(std::back_inserter(written), "{}\n", GetXFMemName(address + i));
  }

  return std::make_pair(desc, fmt::to_string(written));
}
