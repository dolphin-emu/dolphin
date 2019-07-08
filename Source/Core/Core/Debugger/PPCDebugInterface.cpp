// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Debugger/PPCDebugInterface.h"

#include <array>
#include <cstddef>
#include <string>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/GekkoDisassembler.h"

#include "Core/Core.h"
#include "Core/HW/DSP.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

void PPCPatches::Patch(std::size_t index)
{
  auto& patch = m_patches[index];
  if (patch.value.empty())
    return;

  const u32 address = patch.address;
  const std::size_t size = patch.value.size();
  if (!PowerPC::HostIsRAMAddress(address))
    return;

  for (u32 offset = 0; offset < size; ++offset)
  {
    const u8 value = PowerPC::HostRead_U8(address + offset);
    PowerPC::HostWrite_U8(patch.value[offset], address + offset);
    patch.value[offset] = value;

    if (((address + offset) % 4) == 3)
      PowerPC::ScheduleInvalidateCacheThreadSafe(Common::AlignDown(address + offset, 4));
  }
  if (((address + size) % 4) != 0)
  {
    PowerPC::ScheduleInvalidateCacheThreadSafe(
        Common::AlignDown(address + static_cast<u32>(size), 4));
  }
}

PPCDebugInterface::PPCDebugInterface() = default;
PPCDebugInterface::~PPCDebugInterface() = default;

std::size_t PPCDebugInterface::SetWatch(u32 address, std::string name)
{
  return m_watches.SetWatch(address, std::move(name));
}

const Common::Debug::Watch& PPCDebugInterface::GetWatch(std::size_t index) const
{
  return m_watches.GetWatch(index);
}

const std::vector<Common::Debug::Watch>& PPCDebugInterface::GetWatches() const
{
  return m_watches.GetWatches();
}

void PPCDebugInterface::UnsetWatch(u32 address)
{
  m_watches.UnsetWatch(address);
}

void PPCDebugInterface::UpdateWatch(std::size_t index, u32 address, std::string name)
{
  return m_watches.UpdateWatch(index, address, std::move(name));
}

void PPCDebugInterface::UpdateWatchAddress(std::size_t index, u32 address)
{
  return m_watches.UpdateWatchAddress(index, address);
}

void PPCDebugInterface::UpdateWatchName(std::size_t index, std::string name)
{
  return m_watches.UpdateWatchName(index, std::move(name));
}

void PPCDebugInterface::EnableWatch(std::size_t index)
{
  m_watches.EnableWatch(index);
}

void PPCDebugInterface::DisableWatch(std::size_t index)
{
  m_watches.DisableWatch(index);
}

bool PPCDebugInterface::HasEnabledWatch(u32 address) const
{
  return m_watches.HasEnabledWatch(address);
}

void PPCDebugInterface::RemoveWatch(std::size_t index)
{
  return m_watches.RemoveWatch(index);
}

void PPCDebugInterface::LoadWatchesFromStrings(const std::vector<std::string>& watches)
{
  m_watches.LoadFromStrings(watches);
}

std::vector<std::string> PPCDebugInterface::SaveWatchesToStrings() const
{
  return m_watches.SaveToStrings();
}

void PPCDebugInterface::ClearWatches()
{
  m_watches.Clear();
}

void PPCDebugInterface::SetPatch(u32 address, u32 value)
{
  m_patches.SetPatch(address, value);
}

void PPCDebugInterface::SetPatch(u32 address, std::vector<u8> value)
{
  m_patches.SetPatch(address, std::move(value));
}

const std::vector<Common::Debug::MemoryPatch>& PPCDebugInterface::GetPatches() const
{
  return m_patches.GetPatches();
}

void PPCDebugInterface::UnsetPatch(u32 address)
{
  m_patches.UnsetPatch(address);
}

void PPCDebugInterface::EnablePatch(std::size_t index)
{
  m_patches.EnablePatch(index);
}

void PPCDebugInterface::DisablePatch(std::size_t index)
{
  m_patches.DisablePatch(index);
}

bool PPCDebugInterface::HasEnabledPatch(u32 address) const
{
  return m_patches.HasEnabledPatch(address);
}

void PPCDebugInterface::RemovePatch(std::size_t index)
{
  m_patches.RemovePatch(index);
}

void PPCDebugInterface::ClearPatches()
{
  m_patches.ClearPatches();
}

std::string PPCDebugInterface::Disassemble(u32 address) const
{
  // PowerPC::HostRead_U32 seemed to crash on shutdown
  if (!IsAlive())
    return "";

  if (Core::GetState() == Core::State::Paused)
  {
    if (!PowerPC::HostIsRAMAddress(address))
    {
      return "(No RAM here)";
    }

    const u32 op = PowerPC::HostRead_Instruction(address);
    std::string disasm = Common::GekkoDisassembler::Disassemble(op, address);
    const UGeckoInstruction inst{op};

    if (inst.OPCD == 1)
    {
      disasm += " (hle)";
    }

    return disasm;
  }
  else
  {
    return "<unknown>";
  }
}

std::string PPCDebugInterface::GetRawMemoryString(int memory, u32 address) const
{
  if (IsAlive())
  {
    const bool is_aram = memory != 0;

    if (is_aram || PowerPC::HostIsRAMAddress(address))
      return fmt::format("{:08X}{}", ReadExtraMemory(memory, address), is_aram ? " (ARAM)" : "");

    return is_aram ? "--ARAM--" : "--------";
  }

  return "<unknwn>";  // bad spelling - 8 chars
}

u32 PPCDebugInterface::ReadMemory(u32 address) const
{
  return PowerPC::HostRead_U32(address);
}

u32 PPCDebugInterface::ReadExtraMemory(int memory, u32 address) const
{
  switch (memory)
  {
  case 0:
    return PowerPC::HostRead_U32(address);
  case 1:
    return (DSP::ReadARAM(address) << 24) | (DSP::ReadARAM(address + 1) << 16) |
           (DSP::ReadARAM(address + 2) << 8) | (DSP::ReadARAM(address + 3));
  default:
    return 0;
  }
}

u32 PPCDebugInterface::ReadInstruction(u32 address) const
{
  return PowerPC::HostRead_Instruction(address);
}

bool PPCDebugInterface::IsAlive() const
{
  return Core::IsRunningAndStarted();
}

bool PPCDebugInterface::IsBreakpoint(u32 address) const
{
  return PowerPC::breakpoints.IsAddressBreakPoint(address);
}

void PPCDebugInterface::SetBreakpoint(u32 address)
{
  PowerPC::breakpoints.Add(address);
}

void PPCDebugInterface::ClearBreakpoint(u32 address)
{
  PowerPC::breakpoints.Remove(address);
}

void PPCDebugInterface::ClearAllBreakpoints()
{
  PowerPC::breakpoints.Clear();
}

void PPCDebugInterface::ToggleBreakpoint(u32 address)
{
  if (PowerPC::breakpoints.IsAddressBreakPoint(address))
    PowerPC::breakpoints.Remove(address);
  else
    PowerPC::breakpoints.Add(address);
}

void PPCDebugInterface::ClearAllMemChecks()
{
  PowerPC::memchecks.Clear();
}

bool PPCDebugInterface::IsMemCheck(u32 address, size_t size) const
{
  return PowerPC::memchecks.GetMemCheck(address, size) != nullptr;
}

void PPCDebugInterface::ToggleMemCheck(u32 address, bool read, bool write, bool log)
{
  if (!IsMemCheck(address))
  {
    // Add Memory Check
    TMemCheck MemCheck;
    MemCheck.start_address = address;
    MemCheck.end_address = address;
    MemCheck.is_break_on_read = read;
    MemCheck.is_break_on_write = write;

    MemCheck.log_on_hit = log;
    MemCheck.break_on_hit = true;

    PowerPC::memchecks.Add(MemCheck);
  }
  else
  {
    PowerPC::memchecks.Remove(address);
  }
}

// =======================================================
// Separate the blocks with colors.
// -------------
u32 PPCDebugInterface::GetColor(u32 address) const
{
  if (!IsAlive())
    return 0xFFFFFF;
  if (!PowerPC::HostIsRAMAddress(address))
    return 0xeeeeee;

  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(address);
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

std::string PPCDebugInterface::GetDescription(u32 address) const
{
  return g_symbolDB.GetDescription(address);
}

u32 PPCDebugInterface::GetPC() const
{
  return PowerPC::ppcState.pc;
}

void PPCDebugInterface::SetPC(u32 address)
{
  PowerPC::ppcState.pc = address;
}

void PPCDebugInterface::RunToBreakpoint()
{
}

void PPCDebugInterface::Clear()
{
  ClearAllBreakpoints();
  ClearAllMemChecks();
  ClearPatches();
  ClearWatches();
}
