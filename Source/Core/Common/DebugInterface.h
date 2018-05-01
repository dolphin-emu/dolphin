// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Debug/BreakPoints.h"
#include "Common/Debug/MemoryPatches.h"
#include "Common/Debug/Watches.h"

class DebugInterface
{
protected:
  virtual ~DebugInterface() {}

public:
  // Watches
  virtual std::size_t SetWatch(u32 address, const std::string& name = "") = 0;
  virtual const Common::Debug::Watch& GetWatch(std::size_t index) const = 0;
  virtual const std::vector<Common::Debug::Watch>& GetWatches() const = 0;
  virtual void UnsetWatch(u32 address) = 0;
  virtual void UpdateWatch(std::size_t index, u32 address, const std::string& name) = 0;
  virtual void UpdateWatchAddress(std::size_t index, u32 address) = 0;
  virtual void UpdateWatchName(std::size_t index, const std::string& name) = 0;
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
  virtual const std::vector<Common::Debug::MemoryPatch>& GetPatches() const = 0;
  virtual void UnsetPatch(u32 address) = 0;
  virtual void EnablePatch(std::size_t index) = 0;
  virtual void DisablePatch(std::size_t index) = 0;
  virtual bool HasEnabledPatch(u32 address) const = 0;
  virtual void RemovePatch(std::size_t index) = 0;
  virtual void ClearPatches() = 0;

  // Breakpoints
  virtual void SetBreakpoint(u32 address, Common::Debug::BreakPoint::State state =
                                              Common::Debug::BreakPoint::State::Enabled) = 0;
  virtual const Common::Debug::BreakPoint& GetBreakpoint(std::size_t index) const = 0;
  virtual const std::vector<Common::Debug::BreakPoint>& GetBreakpoints() const = 0;
  virtual void UnsetBreakpoint(u32 address) = 0;
  virtual void ToggleBreakpoint(u32 address) = 0;
  virtual void EnableBreakpoint(std::size_t index) = 0;
  virtual void EnableBreakpointAt(u32 address) = 0;
  virtual void DisableBreakpoint(std::size_t index) = 0;
  virtual void DisableBreakpointAt(u32 address) = 0;
  virtual bool HasBreakpoint(u32 address) const = 0;
  virtual bool HasBreakpoint(u32 address, Common::Debug::BreakPoint::State state) const = 0;
  virtual bool BreakpointBreak(u32 address) = 0;
  virtual void RemoveBreakpoint(std::size_t index) = 0;
  virtual void LoadBreakpointsFromStrings(const std::vector<std::string>& breakpoints) = 0;
  virtual std::vector<std::string> SaveBreakpointsToStrings() const = 0;
  virtual void ClearBreakpoints() = 0;
  virtual void ClearTemporaryBreakpoints() = 0;

  virtual std::string Disassemble(unsigned int /*address*/) { return "NODEBUGGER"; }
  virtual std::string GetRawMemoryString(int /*memory*/, unsigned int /*address*/)
  {
    return "NODEBUGGER";
  }
  virtual int GetInstructionSize(int /*instruction*/) { return 1; }
  virtual bool IsAlive() { return true; }
  virtual void ClearAllMemChecks() {}
  virtual bool IsMemCheck(unsigned int /*address*/, size_t /*size*/) { return false; }
  virtual void ToggleMemCheck(unsigned int /*address*/, bool /*read*/, bool /*write*/, bool /*log*/)
  {
  }
  virtual unsigned int ReadMemory(unsigned int /*address*/) { return 0; }
  virtual void WriteExtraMemory(int /*memory*/, unsigned int /*value*/, unsigned int /*address*/) {}
  virtual unsigned int ReadExtraMemory(int /*memory*/, unsigned int /*address*/) { return 0; }
  virtual unsigned int ReadInstruction(unsigned int /*address*/) { return 0; }
  virtual unsigned int GetPC() { return 0; }
  virtual void SetPC(unsigned int /*address*/) {}
  virtual void Step() {}
  virtual void RunToBreakpoint() {}
  virtual int GetColor(unsigned int /*address*/) { return 0xFFFFFFFF; }
  virtual std::string GetDescription(unsigned int /*address*/) = 0;
  virtual void Clear() = 0;
};
