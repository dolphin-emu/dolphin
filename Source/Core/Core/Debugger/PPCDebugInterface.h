// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/Debug/MemoryPatches.h"
#include "Common/Debug/Watches.h"
#include "Common/DebugInterface.h"

class PPCPatches : public Common::Debug::MemoryPatches
{
private:
  void Patch(std::size_t index) override;
};

// wrapper between disasm control and Dolphin debugger

class PPCDebugInterface final : public Common::DebugInterface
{
public:
  PPCDebugInterface();
  ~PPCDebugInterface() override;

  // Watches
  std::size_t SetWatch(u32 address, std::string name = "") override;
  const Common::Debug::Watch& GetWatch(std::size_t index) const override;
  const std::vector<Common::Debug::Watch>& GetWatches() const override;
  void UnsetWatch(u32 address) override;
  void UpdateWatch(std::size_t index, u32 address, std::string name) override;
  void UpdateWatchAddress(std::size_t index, u32 address) override;
  void UpdateWatchName(std::size_t index, std::string name) override;
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

  std::string Disassemble(u32 address) const override;
  std::string GetRawMemoryString(int memory, u32 address) const override;
  bool IsAlive() const override;
  bool IsBreakpoint(u32 address) const override;
  void SetBreakpoint(u32 address) override;
  void ClearBreakpoint(u32 address) override;
  void ClearAllBreakpoints() override;
  void ToggleBreakpoint(u32 address) override;
  void ClearAllMemChecks() override;
  bool IsMemCheck(u32 address, size_t size = 1) const override;
  void ToggleMemCheck(u32 address, bool read = true, bool write = true, bool log = true) override;
  u32 ReadMemory(u32 address) const override;

  enum
  {
    EXTRAMEM_ARAM = 1,
  };

  u32 ReadExtraMemory(int memory, u32 address) const override;
  u32 ReadInstruction(u32 address) const override;
  u32 GetPC() const override;
  void SetPC(u32 address) override;
  void Step() override {}
  void RunToBreakpoint() override;
  u32 GetColor(u32 address) const override;
  std::string GetDescription(u32 address) const override;

  void Clear() override;

private:
  Common::Debug::Watches m_watches;
  PPCPatches m_patches;
};
