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
class DSPDebugInterface final : public DebugInterface
{
public:
  DSPDebugInterface() {}
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
  void Patch(unsigned int address, unsigned int value) override;
  int GetColor(unsigned int address) override;
  std::string GetDescription(unsigned int address) override;
};
}  // namespace LLE
}  // namespace DSP
