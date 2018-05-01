// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/DebugInterface.h"

class PPCPatches : public Common::Debug::MemoryPatches
{
private:
  void Patch(std::size_t index) override;
};

class PPCBreakPoints : public Common::Debug::BreakPoints
{
private:
  void UpdateHook(u32 address) override;
};

// wrapper between disasm control and Dolphin debugger

class PPCDebugInterface final : public DebugInterface
{
public:
  PPCDebugInterface() {}
  // Watches
  std::size_t SetWatch(u32 address, const std::string& name = "") override;
  const Common::Debug::Watch& GetWatch(std::size_t index) const override;
  const std::vector<Common::Debug::Watch>& GetWatches() const override;
  void UnsetWatch(u32 address) override;
  void UpdateWatch(std::size_t index, u32 address, const std::string& name) override;
  void UpdateWatchAddress(std::size_t index, u32 address) override;
  void UpdateWatchName(std::size_t index, const std::string& name) override;
  void EnableWatch(std::size_t index) override;
  void DisableWatch(std::size_t index) override;
  bool HasEnabledWatch(u32 address) const override;
  void RemoveWatch(std::size_t index) override;
  void LoadWatchesFromStrings(const std::vector<std::string>& watches) override;
  std::vector<std::string> SaveWatchesToStrings() const override;
  void ClearWatches() override;

  // Memory Patches
  void SetPatch(u32 address, u32 value) override;
  void SetPatch(u32 address, std::vector<u8> value) override;
  const std::vector<Common::Debug::MemoryPatch>& GetPatches() const override;
  void UnsetPatch(u32 address) override;
  void EnablePatch(std::size_t index) override;
  void DisablePatch(std::size_t index) override;
  bool HasEnabledPatch(u32 address) const override;
  void RemovePatch(std::size_t index) override;
  void ClearPatches() override;

  // Breakpoints
  void SetBreakpoint(u32 address, Common::Debug::BreakPoint::State state =
                                      Common::Debug::BreakPoint::State::Enabled) override;
  const Common::Debug::BreakPoint& GetBreakpoint(std::size_t index) const override;
  const std::vector<Common::Debug::BreakPoint>& GetBreakpoints() const override;
  void UnsetBreakpoint(u32 address) override;
  void ToggleBreakpoint(u32 address) override;
  void EnableBreakpoint(std::size_t index) override;
  void EnableBreakpointAt(u32 address) override;
  void DisableBreakpoint(std::size_t index) override;
  void DisableBreakpointAt(u32 address) override;
  bool HasBreakpoint(u32 address) const override;
  bool HasBreakpoint(u32 address, Common::Debug::BreakPoint::State state) const override;
  bool BreakpointBreak(u32 address) override;
  void RemoveBreakpoint(std::size_t index) override;
  void LoadBreakpointsFromStrings(const std::vector<std::string>& breakpoints) override;
  std::vector<std::string> SaveBreakpointsToStrings() const override;
  void ClearBreakpoints() override;
  void ClearTemporaryBreakpoints() override;

  std::string Disassemble(unsigned int address) override;
  std::string GetRawMemoryString(int memory, unsigned int address) override;
  int GetInstructionSize(int /*instruction*/) override { return 4; }
  bool IsAlive() override;
  void ClearAllMemChecks() override;
  bool IsMemCheck(unsigned int address, size_t size = 1) override;
  void ToggleMemCheck(unsigned int address, bool read = true, bool write = true,
                      bool log = true) override;
  unsigned int ReadMemory(unsigned int address) override;

  enum
  {
    EXTRAMEM_ARAM = 1,
  };

  unsigned int ReadExtraMemory(int memory, unsigned int address) override;
  unsigned int ReadInstruction(unsigned int address) override;
  unsigned int GetPC() override;
  void SetPC(unsigned int address) override;
  void Step() override {}
  void RunToBreakpoint() override;
  int GetColor(unsigned int address) override;
  std::string GetDescription(unsigned int address) override;

  void Clear() override;

private:
  Common::Debug::Watches m_watches;
  PPCBreakPoints m_breakpoints;
  PPCPatches m_patches;
};
