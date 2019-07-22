// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPLLE/DSPDebugInterface.h"

#include <array>
#include <cstddef>
#include <string>

#include <fmt/format.h>

#include "Common/MsgHandler.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/HW/DSPLLE/DSPSymbols.h"

namespace DSP::LLE
{
void DSPPatches::Patch(std::size_t index)
{
  PanicAlert("Patch functionality not supported in DSP module.");
}

DSPDebugInterface::DSPDebugInterface() = default;
DSPDebugInterface::~DSPDebugInterface() = default;

std::size_t DSPDebugInterface::SetWatch(u32 address, std::string name)
{
  return m_watches.SetWatch(address, std::move(name));
}

const Common::Debug::Watch& DSPDebugInterface::GetWatch(std::size_t index) const
{
  return m_watches.GetWatch(index);
}

const std::vector<Common::Debug::Watch>& DSPDebugInterface::GetWatches() const
{
  return m_watches.GetWatches();
}

void DSPDebugInterface::UnsetWatch(u32 address)
{
  m_watches.UnsetWatch(address);
}

void DSPDebugInterface::UpdateWatch(std::size_t index, u32 address, std::string name)
{
  return m_watches.UpdateWatch(index, address, std::move(name));
}

void DSPDebugInterface::UpdateWatchAddress(std::size_t index, u32 address)
{
  return m_watches.UpdateWatchAddress(index, address);
}

void DSPDebugInterface::UpdateWatchName(std::size_t index, std::string name)
{
  return m_watches.UpdateWatchName(index, std::move(name));
}

void DSPDebugInterface::EnableWatch(std::size_t index)
{
  m_watches.EnableWatch(index);
}

void DSPDebugInterface::DisableWatch(std::size_t index)
{
  m_watches.DisableWatch(index);
}

bool DSPDebugInterface::HasEnabledWatch(u32 address) const
{
  return m_watches.HasEnabledWatch(address);
}

void DSPDebugInterface::RemoveWatch(std::size_t index)
{
  return m_watches.RemoveWatch(index);
}

void DSPDebugInterface::LoadWatchesFromStrings(const std::vector<std::string>& watches)
{
  m_watches.LoadFromStrings(watches);
}

std::vector<std::string> DSPDebugInterface::SaveWatchesToStrings() const
{
  return m_watches.SaveToStrings();
}

void DSPDebugInterface::ClearWatches()
{
  m_watches.Clear();
}

void DSPDebugInterface::SetPatch(u32 address, u32 value)
{
  m_patches.SetPatch(address, value);
}

void DSPDebugInterface::SetPatch(u32 address, std::vector<u8> value)
{
  m_patches.SetPatch(address, std::move(value));
}

const std::vector<Common::Debug::MemoryPatch>& DSPDebugInterface::GetPatches() const
{
  return m_patches.GetPatches();
}

void DSPDebugInterface::UnsetPatch(u32 address)
{
  m_patches.UnsetPatch(address);
}

void DSPDebugInterface::EnablePatch(std::size_t index)
{
  m_patches.EnablePatch(index);
}

void DSPDebugInterface::DisablePatch(std::size_t index)
{
  m_patches.DisablePatch(index);
}

void DSPDebugInterface::RemovePatch(std::size_t index)
{
  m_patches.RemovePatch(index);
}

bool DSPDebugInterface::HasEnabledPatch(u32 address) const
{
  return m_patches.HasEnabledPatch(address);
}

void DSPDebugInterface::ClearPatches()
{
  m_patches.ClearPatches();
}

std::string DSPDebugInterface::Disassemble(u32 address) const
{
  // we'll treat addresses as line numbers.
  return Symbols::GetLineText(address);
}

std::string DSPDebugInterface::GetRawMemoryString(int memory, u32 address) const
{
  if (DSPCore_GetState() == State::Stopped)
    return "";

  switch (memory)
  {
  case 0:  // IMEM
    switch (address >> 12)
    {
    case 0:
    case 0x8:
      return fmt::format("{:04x}", dsp_imem_read(address));
    default:
      return "--IMEM--";
    }

  case 1:  // DMEM
    switch (address >> 12)
    {
    case 0:
    case 1:
      return fmt::format("{:04x} (DMEM)", dsp_dmem_read(address));
    case 0xf:
      return fmt::format("{:04x} (MMIO)", g_dsp.ifx_regs[address & 0xFF]);
    default:
      return "--DMEM--";
    }
  }

  return "";
}

u32 DSPDebugInterface::ReadMemory(u32 address) const
{
  return 0;
}

u32 DSPDebugInterface::ReadInstruction(u32 address) const
{
  return 0;
}

bool DSPDebugInterface::IsAlive() const
{
  return true;
}

bool DSPDebugInterface::IsBreakpoint(u32 address) const
{
  int real_addr = Symbols::Line2Addr(address);
  if (real_addr >= 0)
    return g_dsp_breakpoints.IsAddressBreakPoint(real_addr);

  return false;
}

void DSPDebugInterface::SetBreakpoint(u32 address)
{
  int real_addr = Symbols::Line2Addr(address);

  if (real_addr >= 0)
  {
    g_dsp_breakpoints.Add(real_addr);
  }
}

void DSPDebugInterface::ClearBreakpoint(u32 address)
{
  int real_addr = Symbols::Line2Addr(address);

  if (real_addr >= 0)
  {
    g_dsp_breakpoints.Remove(real_addr);
  }
}

void DSPDebugInterface::ClearAllBreakpoints()
{
  g_dsp_breakpoints.Clear();
}

void DSPDebugInterface::ToggleBreakpoint(u32 address)
{
  int real_addr = Symbols::Line2Addr(address);
  if (real_addr >= 0)
  {
    if (g_dsp_breakpoints.IsAddressBreakPoint(real_addr))
      g_dsp_breakpoints.Remove(real_addr);
    else
      g_dsp_breakpoints.Add(real_addr);
  }
}

bool DSPDebugInterface::IsMemCheck(u32 address, size_t size) const
{
  return false;
}

void DSPDebugInterface::ClearAllMemChecks()
{
  PanicAlert("MemCheck functionality not supported in DSP module.");
}

void DSPDebugInterface::ToggleMemCheck(u32 address, bool read, bool write, bool log)
{
  PanicAlert("MemCheck functionality not supported in DSP module.");
}

// =======================================================
// Separate the blocks with colors.
// -------------
u32 DSPDebugInterface::GetColor(u32 address) const
{
  // Scan backwards so we don't miss it. Hm, actually, let's not - it looks pretty good.
  int addr = -1;
  for (int i = 0; i < 1; i++)
  {
    addr = Symbols::Line2Addr(address - i);
    if (addr >= 0)
      break;
  }
  if (addr == -1)
    return 0xFFFFFF;

  Common::Symbol* symbol = Symbols::g_dsp_symbol_db.GetSymbolFromAddr(addr);
  if (!symbol)
    return 0xFFFFFF;
  if (symbol->type != Common::Symbol::Type::Function)
    return 0xEEEEFF;

  static constexpr std::array<u32, 6> colors{
      0xd0FFFF,  // light cyan
      0xFFd0d0,  // light red
      0xd8d8FF,  // light blue
      0xFFd0FF,  // light purple
      0xd0FFd0,  // light green
      0xFFFFd0,  // light yellow
  };
  return colors[symbol->index % colors.size()];
}
// =============

std::string DSPDebugInterface::GetDescription(u32 address) const
{
  return "";  // g_symbolDB.GetDescription(address);
}

u32 DSPDebugInterface::GetPC() const
{
  return Symbols::Addr2Line(DSP::g_dsp.pc);
}

void DSPDebugInterface::SetPC(u32 address)
{
  int new_pc = Symbols::Line2Addr(address);
  if (new_pc > 0)
    g_dsp.pc = new_pc;
}

void DSPDebugInterface::RunToBreakpoint()
{
}

void DSPDebugInterface::Clear()
{
  ClearPatches();
  ClearWatches();
}
}  // namespace DSP::LLE
