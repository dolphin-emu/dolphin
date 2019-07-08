// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common::Debug
{
struct MemoryPatch;
struct Watch;
}  // namespace Common::Debug

namespace Common
{
class DebugInterface
{
protected:
  virtual ~DebugInterface() = default;

public:
  // Watches
  virtual std::size_t SetWatch(u32 address, std::string name = "") = 0;
  virtual const Debug::Watch& GetWatch(std::size_t index) const = 0;
  virtual const std::vector<Debug::Watch>& GetWatches() const = 0;
  virtual void UnsetWatch(u32 address) = 0;
  virtual void UpdateWatch(std::size_t index, u32 address, std::string name) = 0;
  virtual void UpdateWatchAddress(std::size_t index, u32 address) = 0;
  virtual void UpdateWatchName(std::size_t index, std::string name) = 0;
  virtual void EnableWatch(std::size_t index) = 0;
  virtual void DisableWatch(std::size_t index) = 0;
  virtual bool HasEnabledWatch(u32 address) const = 0;
  virtual void RemoveWatch(std::size_t index) = 0;
  virtual void LoadWatchesFromStrings(const std::vector<std::string>& watches) = 0;
  virtual std::vector<std::string> SaveWatchesToStrings() const = 0;
  virtual void ClearWatches() = 0;

  // Memory Patches
  virtual void SetPatch(u32 address, u32 value) = 0;
  virtual void SetPatch(u32 address, std::vector<u8> value) = 0;
  virtual const std::vector<Debug::MemoryPatch>& GetPatches() const = 0;
  virtual void UnsetPatch(u32 address) = 0;
  virtual void EnablePatch(std::size_t index) = 0;
  virtual void DisablePatch(std::size_t index) = 0;
  virtual bool HasEnabledPatch(u32 address) const = 0;
  virtual void RemovePatch(std::size_t index) = 0;
  virtual void ClearPatches() = 0;

  virtual std::string Disassemble(u32 /*address*/) const { return "NODEBUGGER"; }
  virtual std::string GetRawMemoryString(int /*memory*/, u32 /*address*/) const
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
  virtual u32 ReadMemory(u32 /*address*/) const { return 0; }
  virtual void WriteExtraMemory(int /*memory*/, u32 /*value*/, u32 /*address*/) {}
  virtual u32 ReadExtraMemory(int /*memory*/, u32 /*address*/) const { return 0; }
  virtual u32 ReadInstruction(u32 /*address*/) const { return 0; }
  virtual u32 GetPC() const { return 0; }
  virtual void SetPC(u32 /*address*/) {}
  virtual void Step() {}
  virtual void RunToBreakpoint() {}
  virtual u32 GetColor(u32 /*address*/) const { return 0xFFFFFFFF; }
  virtual std::string GetDescription(u32 /*address*/) const = 0;
  virtual void Clear() = 0;
};
}  // namespace Common
