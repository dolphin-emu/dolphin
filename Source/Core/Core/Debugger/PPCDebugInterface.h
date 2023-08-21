// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "Common/Debug/MemoryPatches.h"
#include "Common/Debug/Watches.h"
#include "Common/DebugInterface.h"
#include "Core/NetworkCaptureLogger.h"

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

void ApplyMemoryPatch(const Core::CPUThreadGuard&, Common::Debug::MemoryPatch& patch,
                      bool store_existing_value = true);

class PPCPatches final : public Common::Debug::MemoryPatches
{
public:
  void ApplyExistingPatch(const Core::CPUThreadGuard& guard, std::size_t index) override;

private:
  void Patch(const Core::CPUThreadGuard& guard, std::size_t index) override;
  void UnPatch(std::size_t index) override;
};

// wrapper between disasm control and Dolphin debugger

class PPCDebugInterface final : public Common::DebugInterface
{
public:
  explicit PPCDebugInterface(Core::System& system);
  ~PPCDebugInterface() override;

  // Watches
  std::size_t SetWatch(u32 address, std::string name = "") override;
  const Common::Debug::Watch& GetWatch(std::size_t index) const override;
  const std::vector<Common::Debug::Watch>& GetWatches() const override;
  void UnsetWatch(u32 address) override;
  void UpdateWatch(std::size_t index, u32 address, std::string name) override;
  void UpdateWatchAddress(std::size_t index, u32 address) override;
  void UpdateWatchName(std::size_t index, std::string name) override;
  void UpdateWatchLockedState(std::size_t index, bool locked) override;
  void EnableWatch(std::size_t index) override;
  void DisableWatch(std::size_t index) override;
  bool HasEnabledWatch(u32 address) const override;
  void RemoveWatch(std::size_t index) override;
  void LoadWatchesFromStrings(const std::vector<std::string>& watches) override;
  std::vector<std::string> SaveWatchesToStrings() const override;
  void ClearWatches() override;

  // Memory Patches
  void SetPatch(const Core::CPUThreadGuard& guard, u32 address, u32 value) override;
  void SetPatch(const Core::CPUThreadGuard& guard, u32 address, std::vector<u8> value) override;
  void SetFramePatch(const Core::CPUThreadGuard& guard, u32 address, u32 value) override;
  void SetFramePatch(const Core::CPUThreadGuard& guard, u32 address,
                     std::vector<u8> value) override;
  const std::vector<Common::Debug::MemoryPatch>& GetPatches() const override;
  void UnsetPatch(const Core::CPUThreadGuard& guard, u32 address) override;
  void EnablePatch(const Core::CPUThreadGuard& guard, std::size_t index) override;
  void DisablePatch(const Core::CPUThreadGuard& guard, std::size_t index) override;
  bool HasEnabledPatch(u32 address) const override;
  void RemovePatch(const Core::CPUThreadGuard& guard, std::size_t index) override;
  void ClearPatches(const Core::CPUThreadGuard& guard) override;
  void ApplyExistingPatch(const Core::CPUThreadGuard& guard, std::size_t index) override;

  // Threads
  Common::Debug::Threads GetThreads(const Core::CPUThreadGuard& guard) const override;

  std::string Disassemble(const Core::CPUThreadGuard* guard, u32 address) const override;
  std::string GetRawMemoryString(const Core::CPUThreadGuard& guard, int memory,
                                 u32 address) const override;
  bool IsAlive() const override;
  bool IsBreakpoint(u32 address) const override;
  void SetBreakpoint(u32 address) override;
  void ClearBreakpoint(u32 address) override;
  void ClearAllBreakpoints() override;
  void ToggleBreakpoint(u32 address) override;
  void ClearAllMemChecks() override;
  bool IsMemCheck(u32 address, size_t size = 1) const override;
  void ToggleMemCheck(u32 address, bool read = true, bool write = true, bool log = true) override;
  u32 ReadMemory(const Core::CPUThreadGuard& guard, u32 address) const override;

  enum
  {
    EXTRAMEM_ARAM = 1,
  };

  u32 ReadExtraMemory(const Core::CPUThreadGuard& guard, int memory, u32 address) const override;
  u32 ReadInstruction(const Core::CPUThreadGuard& guard, u32 address) const override;
  std::optional<u32> GetMemoryAddressFromInstruction(const std::string& instruction) const override;
  u32 GetPC() const override;
  void SetPC(u32 address) override;
  void Step() override {}
  void RunToBreakpoint() override;
  u32 GetColor(const Core::CPUThreadGuard* guard, u32 address) const override;
  std::string GetDescription(u32 address) const override;

  std::shared_ptr<Core::NetworkCaptureLogger> NetworkLogger();

  void Clear(const Core::CPUThreadGuard& guard) override;

private:
  Common::Debug::Watches m_watches;
  PPCPatches m_patches;
  std::shared_ptr<Core::NetworkCaptureLogger> m_network_logger;
  Core::System& m_system;
};
