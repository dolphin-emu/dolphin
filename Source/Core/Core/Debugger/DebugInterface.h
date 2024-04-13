// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Debug/Threads.h"

namespace Common::Debug
{
struct MemoryPatch;
struct Watch;
}  // namespace Common::Debug

namespace Core
{
class CPUThreadGuard;

class DebugInterface
{
protected:
  virtual ~DebugInterface() = default;

public:
  // Watches
  virtual std::size_t SetWatch(u32 address, std::string name = "") = 0;
  virtual const Common::Debug::Watch& GetWatch(std::size_t index) const = 0;
  virtual const std::vector<Common::Debug::Watch>& GetWatches() const = 0;
  virtual void UnsetWatch(u32 address) = 0;
  virtual void UpdateWatch(std::size_t index, u32 address, std::string name) = 0;
  virtual void UpdateWatchAddress(std::size_t index, u32 address) = 0;
  virtual void UpdateWatchName(std::size_t index, std::string name) = 0;
  virtual void UpdateWatchLockedState(std::size_t index, bool locked) = 0;
  virtual void EnableWatch(std::size_t index) = 0;
  virtual void DisableWatch(std::size_t index) = 0;
  virtual bool HasEnabledWatch(u32 address) const = 0;
  virtual void RemoveWatch(std::size_t index) = 0;
  virtual void LoadWatchesFromStrings(const std::vector<std::string>& watches) = 0;
  virtual std::vector<std::string> SaveWatchesToStrings() const = 0;
  virtual void ClearWatches() = 0;

  // Memory Patches
  virtual void SetPatch(const CPUThreadGuard& guard, u32 address, u32 value) = 0;
  virtual void SetPatch(const CPUThreadGuard& guard, u32 address, std::vector<u8> value) = 0;
  virtual void SetFramePatch(const CPUThreadGuard& guard, u32 address, u32 value) = 0;
  virtual void SetFramePatch(const CPUThreadGuard& guard, u32 address, std::vector<u8> value) = 0;
  virtual const std::vector<Common::Debug::MemoryPatch>& GetPatches() const = 0;
  virtual void UnsetPatch(const CPUThreadGuard& guard, u32 address) = 0;
  virtual void EnablePatch(const CPUThreadGuard& guard, std::size_t index) = 0;
  virtual void DisablePatch(const CPUThreadGuard& guard, std::size_t index) = 0;
  virtual bool HasEnabledPatch(u32 address) const = 0;
  virtual void RemovePatch(const CPUThreadGuard& guard, std::size_t index) = 0;
  virtual void ClearPatches(const CPUThreadGuard& guard) = 0;
  virtual void ApplyExistingPatch(const CPUThreadGuard& guard, std::size_t index) = 0;

  // Threads
  virtual Common::Debug::Threads GetThreads(const CPUThreadGuard& guard) const = 0;

  virtual std::string Disassemble(const CPUThreadGuard* /*guard*/, u32 /*address*/) const
  {
    return "NODEBUGGER";
  }
  virtual std::string GetRawMemoryString(const CPUThreadGuard& /*guard*/, int /*memory*/,
                                         u32 /*address*/) const
  {
    return "NODEBUGGER";
  }
  virtual bool IsAlive() const { return true; }
  virtual bool IsBreakpoint(u32 /*address*/) const { return false; }
  virtual void SetBreakpoint(u32 /*address*/) {}
  virtual void ClearBreakpoint(u32 /*address*/) {}
  virtual void ClearAllBreakpoints() {}
  virtual void ToggleBreakpoint(u32 /*address*/) {}
  virtual void ClearAllMemChecks() {}
  virtual bool IsMemCheck(u32 /*address*/, size_t /*size*/) const { return false; }
  virtual void ToggleMemCheck(u32 /*address*/, bool /*read*/, bool /*write*/, bool /*log*/) {}
  virtual u32 ReadMemory(const CPUThreadGuard& /*guard*/, u32 /*address*/) const { return 0; }
  virtual void WriteExtraMemory(const CPUThreadGuard& /*guard*/, int /*memory*/, u32 /*value*/,
                                u32 /*address*/)
  {
  }
  virtual u32 ReadExtraMemory(const CPUThreadGuard& /*guard*/, int /*memory*/,
                              u32 /*address*/) const
  {
    return 0;
  }
  virtual u32 ReadInstruction(const CPUThreadGuard& /*guard*/, u32 /*address*/) const { return 0; }
  virtual std::optional<u32>
  GetMemoryAddressFromInstruction(const std::string& /*instruction*/) const
  {
    return std::nullopt;
  }
  virtual u32 GetPC() const { return 0; }
  virtual void SetPC(u32 /*address*/) {}
  virtual void Step() {}
  virtual void RunToBreakpoint() {}
  virtual u32 GetColor(const CPUThreadGuard* /*guard*/, u32 /*address*/) const
  {
    return 0xFFFFFFFF;
  }
  virtual std::string_view GetDescription(u32 /*address*/) const = 0;
  virtual void Clear(const CPUThreadGuard& guard) = 0;
};
}  // namespace Core
