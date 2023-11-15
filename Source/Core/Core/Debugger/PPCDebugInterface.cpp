// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/PPCDebugInterface.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <regex>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"

#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/Debugger/OSThread.h"
#include "Core/HW/DSP.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

void ApplyMemoryPatch(const Core::CPUThreadGuard& guard, Common::Debug::MemoryPatch& patch,
                      bool store_existing_value)
{
#ifdef USE_RETRO_ACHIEVEMENTS
  if (Config::Get(Config::RA_HARDCORE_ENABLED))
    return;
#endif  // USE_RETRO_ACHIEVEMENTS
  if (patch.value.empty())
    return;

  const u32 address = patch.address;
  const std::size_t size = patch.value.size();
  if (!PowerPC::MMU::HostIsRAMAddress(guard, address))
    return;

  auto& power_pc = guard.GetSystem().GetPowerPC();
  for (u32 offset = 0; offset < size; ++offset)
  {
    if (store_existing_value)
    {
      const u8 value = PowerPC::MMU::HostRead_U8(guard, address + offset);
      PowerPC::MMU::HostWrite_U8(guard, patch.value[offset], address + offset);
      patch.value[offset] = value;
    }
    else
    {
      PowerPC::MMU::HostWrite_U8(guard, patch.value[offset], address + offset);
    }

    if (((address + offset) % 4) == 3)
      power_pc.ScheduleInvalidateCacheThreadSafe(Common::AlignDown(address + offset, 4));
  }
  if (((address + size) % 4) != 0)
  {
    power_pc.ScheduleInvalidateCacheThreadSafe(
        Common::AlignDown(address + static_cast<u32>(size), 4));
  }
}

void PPCPatches::ApplyExistingPatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  auto& patch = m_patches[index];
  ApplyMemoryPatch(guard, patch, false);
}

void PPCPatches::Patch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  auto& patch = m_patches[index];
  if (patch.type == Common::Debug::MemoryPatch::ApplyType::Once)
    ApplyMemoryPatch(guard, patch);
  else
    PatchEngine::AddMemoryPatch(index);
}

void PPCPatches::UnPatch(std::size_t index)
{
  auto& patch = m_patches[index];
  if (patch.type == Common::Debug::MemoryPatch::ApplyType::Once)
    return;

  PatchEngine::RemoveMemoryPatch(index);
}

PPCDebugInterface::PPCDebugInterface(Core::System& system) : m_system(system)
{
}

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

void PPCDebugInterface::UpdateWatchLockedState(std::size_t index, bool locked)
{
  return m_watches.UpdateWatchLockedState(index, locked);
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

void PPCDebugInterface::SetPatch(const Core::CPUThreadGuard& guard, u32 address, u32 value)
{
  m_patches.SetPatch(guard, address, value);
}

void PPCDebugInterface::SetPatch(const Core::CPUThreadGuard& guard, u32 address,
                                 std::vector<u8> value)
{
  m_patches.SetPatch(guard, address, std::move(value));
}

void PPCDebugInterface::SetFramePatch(const Core::CPUThreadGuard& guard, u32 address, u32 value)
{
  m_patches.SetFramePatch(guard, address, value);
}

void PPCDebugInterface::SetFramePatch(const Core::CPUThreadGuard& guard, u32 address,
                                      std::vector<u8> value)
{
  m_patches.SetFramePatch(guard, address, std::move(value));
}

const std::vector<Common::Debug::MemoryPatch>& PPCDebugInterface::GetPatches() const
{
  return m_patches.GetPatches();
}

void PPCDebugInterface::UnsetPatch(const Core::CPUThreadGuard& guard, u32 address)
{
  m_patches.UnsetPatch(guard, address);
}

void PPCDebugInterface::EnablePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  m_patches.EnablePatch(guard, index);
}

void PPCDebugInterface::DisablePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  m_patches.DisablePatch(guard, index);
}

bool PPCDebugInterface::HasEnabledPatch(u32 address) const
{
  return m_patches.HasEnabledPatch(address);
}

void PPCDebugInterface::RemovePatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  m_patches.RemovePatch(guard, index);
}

void PPCDebugInterface::ClearPatches(const Core::CPUThreadGuard& guard)
{
  m_patches.ClearPatches(guard);
}

void PPCDebugInterface::ApplyExistingPatch(const Core::CPUThreadGuard& guard, std::size_t index)
{
  m_patches.ApplyExistingPatch(guard, index);
}

Common::Debug::Threads PPCDebugInterface::GetThreads(const Core::CPUThreadGuard& guard) const
{
  Common::Debug::Threads threads;

  constexpr u32 ACTIVE_QUEUE_HEAD_ADDR = 0x800000dc;
  if (!PowerPC::MMU::HostIsRAMAddress(guard, ACTIVE_QUEUE_HEAD_ADDR))
    return threads;
  const u32 active_queue_head = PowerPC::MMU::HostRead_U32(guard, ACTIVE_QUEUE_HEAD_ADDR);
  if (!PowerPC::MMU::HostIsRAMAddress(guard, active_queue_head))
    return threads;

  auto active_thread = std::make_unique<Core::Debug::OSThreadView>(guard, active_queue_head);
  if (!active_thread->IsValid(guard))
    return threads;

  std::vector<u32> visited_addrs{active_thread->GetAddress()};
  const auto insert_threads = [&guard, &threads, &visited_addrs](u32 addr, auto get_next_addr) {
    while (addr != 0 && PowerPC::MMU::HostIsRAMAddress(guard, addr))
    {
      if (std::find(visited_addrs.begin(), visited_addrs.end(), addr) != visited_addrs.end())
        break;
      visited_addrs.push_back(addr);
      auto thread = std::make_unique<Core::Debug::OSThreadView>(guard, addr);
      if (!thread->IsValid(guard))
        break;
      addr = get_next_addr(*thread);
      threads.emplace_back(std::move(thread));
    }
  };

  const u32 prev_addr = active_thread->Data().thread_link.prev;
  insert_threads(prev_addr, [](const auto& thread) { return thread.Data().thread_link.prev; });
  std::reverse(threads.begin(), threads.end());

  const u32 next_addr = active_thread->Data().thread_link.next;
  threads.emplace_back(std::move(active_thread));
  insert_threads(next_addr, [](const auto& thread) { return thread.Data().thread_link.next; });

  return threads;
}

std::string PPCDebugInterface::Disassemble(const Core::CPUThreadGuard* guard, u32 address) const
{
  if (guard)
  {
    if (!PowerPC::MMU::HostIsRAMAddress(*guard, address))
    {
      return "(No RAM here)";
    }

    const u32 op = PowerPC::MMU::HostRead_Instruction(*guard, address);
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

std::string PPCDebugInterface::GetRawMemoryString(const Core::CPUThreadGuard& guard, int memory,
                                                  u32 address) const
{
  if (IsAlive())
  {
    const bool is_aram = memory != 0;

    if (is_aram || PowerPC::MMU::HostIsRAMAddress(guard, address))
    {
      return fmt::format("{:08X}{}", ReadExtraMemory(guard, memory, address),
                         is_aram ? " (ARAM)" : "");
    }

    return is_aram ? "--ARAM--" : "--------";
  }

  return "<unknwn>";  // bad spelling - 8 chars
}

u32 PPCDebugInterface::ReadMemory(const Core::CPUThreadGuard& guard, u32 address) const
{
  return PowerPC::MMU::HostRead_U32(guard, address);
}

u32 PPCDebugInterface::ReadExtraMemory(const Core::CPUThreadGuard& guard, int memory,
                                       u32 address) const
{
  switch (memory)
  {
  case 0:
    return PowerPC::MMU::HostRead_U32(guard, address);
  case 1:
  {
    auto& dsp = Core::System::GetInstance().GetDSP();
    return (dsp.ReadARAM(address) << 24) | (dsp.ReadARAM(address + 1) << 16) |
           (dsp.ReadARAM(address + 2) << 8) | (dsp.ReadARAM(address + 3));
  }
  default:
    return 0;
  }
}

u32 PPCDebugInterface::ReadInstruction(const Core::CPUThreadGuard& guard, u32 address) const
{
  return PowerPC::MMU::HostRead_Instruction(guard, address);
}

bool PPCDebugInterface::IsAlive() const
{
  return Core::IsRunningAndStarted();
}

bool PPCDebugInterface::IsBreakpoint(u32 address) const
{
  return m_system.GetPowerPC().GetBreakPoints().IsAddressBreakPoint(address);
}

void PPCDebugInterface::SetBreakpoint(u32 address)
{
  m_system.GetPowerPC().GetBreakPoints().Add(address);
}

void PPCDebugInterface::ClearBreakpoint(u32 address)
{
  m_system.GetPowerPC().GetBreakPoints().Remove(address);
}

void PPCDebugInterface::ClearAllBreakpoints()
{
  m_system.GetPowerPC().GetBreakPoints().Clear();
}

void PPCDebugInterface::ToggleBreakpoint(u32 address)
{
  auto& breakpoints = m_system.GetPowerPC().GetBreakPoints();
  if (breakpoints.IsAddressBreakPoint(address))
    breakpoints.Remove(address);
  else
    breakpoints.Add(address);
}

void PPCDebugInterface::ClearAllMemChecks()
{
  m_system.GetPowerPC().GetMemChecks().Clear();
}

bool PPCDebugInterface::IsMemCheck(u32 address, size_t size) const
{
  return m_system.GetPowerPC().GetMemChecks().GetMemCheck(address, size) != nullptr;
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

    m_system.GetPowerPC().GetMemChecks().Add(std::move(MemCheck));
  }
  else
  {
    m_system.GetPowerPC().GetMemChecks().Remove(address);
  }
}

// =======================================================
// Separate the blocks with colors.
// -------------
u32 PPCDebugInterface::GetColor(const Core::CPUThreadGuard* guard, u32 address) const
{
  if (!guard || !IsAlive())
    return 0xFFFFFF;
  if (!PowerPC::MMU::HostIsRAMAddress(*guard, address))
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

std::optional<u32>
PPCDebugInterface::GetMemoryAddressFromInstruction(const std::string& instruction) const
{
  static const std::regex re(",[^r0-]*(-?)(?:0[xX])?([0-9a-fA-F]+|r\\d+)[^r^s]*.(p|toc|\\d+)");
  std::smatch match;

  // Instructions should be identified as a load or store before using this function. This error
  // check should never trigger.
  if (!std::regex_search(instruction, match, re))
    return std::nullopt;

  // match[1]: negative sign for offset or no match.
  // match[2]: 0xNNNN, 0, or rNN. Check next for 'r' to see if a gpr needs to be loaded.
  // match[3]: will either be p, toc, or NN. Always a gpr.
  const std::string_view offset_match{&*match[2].first, size_t(match[2].length())};
  const std::string_view register_match{&*match[3].first, size_t(match[3].length())};
  constexpr char is_reg = 'r';
  u32 offset = 0;

  if (is_reg == offset_match[0])
  {
    unsigned register_index;
    Common::FromChars(offset_match.substr(1), register_index, 10);
    offset = (register_index == 0 ? 0 : m_system.GetPPCState().gpr[register_index]);
  }
  else
  {
    Common::FromChars(offset_match, offset, 16);
  }

  // sp and rtoc need to be converted to 1 and 2.
  constexpr char is_sp = 'p';
  constexpr char is_rtoc = 't';
  u32 i = 0;

  if (is_sp == register_match[0])
    i = 1;
  else if (is_rtoc == register_match[0])
    i = 2;
  else
    Common::FromChars(register_match, i, 10);

  const u32 base_address = m_system.GetPPCState().gpr[i];

  if (std::string_view sign{&*match[1].first, size_t(match[1].length())}; !sign.empty())
    return base_address - offset;

  return base_address + offset;
}

u32 PPCDebugInterface::GetPC() const
{
  return m_system.GetPPCState().pc;
}

void PPCDebugInterface::SetPC(u32 address)
{
  m_system.GetPPCState().pc = address;
}

void PPCDebugInterface::RunToBreakpoint()
{
}

std::shared_ptr<Core::NetworkCaptureLogger> PPCDebugInterface::NetworkLogger()
{
  const bool has_ssl = Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ) ||
                       Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE);
  const bool is_pcap = Config::Get(Config::MAIN_NETWORK_DUMP_AS_PCAP);
  const auto current_capture_type = [&] {
    if (is_pcap)
      return Core::NetworkCaptureType::PCAP;
    if (has_ssl)
      return Core::NetworkCaptureType::Raw;
    return Core::NetworkCaptureType::None;
  }();

  if (m_network_logger && m_network_logger->GetCaptureType() == current_capture_type)
    return m_network_logger;

  switch (current_capture_type)
  {
  case Core::NetworkCaptureType::PCAP:
    m_network_logger = std::make_shared<Core::PCAPSSLCaptureLogger>();
    break;
  case Core::NetworkCaptureType::Raw:
    m_network_logger = std::make_shared<Core::BinarySSLCaptureLogger>();
    break;
  case Core::NetworkCaptureType::None:
    m_network_logger = std::make_shared<Core::DummyNetworkCaptureLogger>();
    break;
  }
  return m_network_logger;
}

void PPCDebugInterface::Clear(const Core::CPUThreadGuard& guard)
{
  ClearAllBreakpoints();
  ClearAllMemChecks();
  ClearPatches(guard);
  ClearWatches();
  m_network_logger.reset();
}
