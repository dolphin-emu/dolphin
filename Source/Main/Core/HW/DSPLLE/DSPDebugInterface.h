// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"

namespace DSP
{
namespace LLE
{
class DSPPatches : public Common::Debug::MemoryPatches
{
private:
  void Patch(std::size_t index) override;
};

class DSPDebugInterface final : public Common::DebugInterface
{
public:
  DSPDebugInterface() {}
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
  void RemovePatch(std::size_t index) override;
  bool HasEnabledPatch(u32 address) const override;
  void ClearPatches() override;

  std::string Disassemble(unsigned int address) override;
  std::string GetRawMemoryString(int memory, unsigned int address) override;
  int GetInstructionSize(int instruction) override { return 1; }
  bool IsAlive() override;
  bool IsBreakpoint(unsigned int address) override;
  void SetBreakpoint(unsigned int address) override;
  void ClearBreakpoint(unsigned int address) override;
  void ClearAllBreakpoints() override;
  void ToggleBreakpoint(unsigned int address) override;
  void ClearAllMemChecks() override;
  bool IsMemCheck(unsigned int address, size_t size) override;
  void ToggleMemCheck(unsigned int address, bool read = true, bool write = true,
                      bool log = true) override;
  unsigned int ReadMemory(unsigned int address) override;
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
  DSPPatches m_patches;
};
}  // namespace LLE
}  // namespace DSP
