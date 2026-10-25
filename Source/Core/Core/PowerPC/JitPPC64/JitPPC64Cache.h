// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>
#include <vector>

#include "Common/PPC64Emitter.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class JitBase;

class JitPPC64BlockCache : public JitBaseBlockCache
{
public:
  explicit JitPPC64BlockCache(JitBase& jit);

  [[nodiscard]] bool Init() override;

  void DestroyBlock(JitBlock& block) override;

  const std::vector<std::pair<u8*, u8*>>& GetRangesToFree() const;

  void ClearRangesToFree();

  void WriteLinkBlock(PPC64Gen::PPC64XEmitter& emit, const JitBlock::LinkData& source,
                      const JitBlock* dest = nullptr);
  // Whether the branch of a link at link can reach a block at target. It follows the check for
  // cycles, so its range is from there.
  static bool CanBranchToBlock(const u8* link, const u8* target);

  // An exit is a fixed number of instructions, so that it can be rewritten when blocks are linked
  // and unlinked:
  //
  //   Unlinked:                     Linked:
  //   nop                           ble cr0, 1f        # Out of cycles: go through the dispatcher
  //   nop                           b dest
  //   MOVI2R DISPATCHER_PC, exit    1: MOVI2R DISPATCHER_PC, exit
  //   b dispatcher                  b dispatcher
  //
  // MOVI2R of a 32-bit address takes up to three instructions, padded with nops. The dispatcher
  // goes to do_timing when CR0 says that the block ran out of cycles.
  static constexpr size_t BLOCK_LINK_SIZE = 6 * sizeof(u32);

  // A guest call, with the BLR optimization, is a host call, so that the host predicts the guest's
  // return, which comes back to the instruction after the bl, and the exit after this:
  //
  //   Unlinked:                     Linked:
  //   b 1f                          ble cr0, 1f
  //   nop                           bl dest
  //   b 2f                          b 2f
  //   1: MOVI2R DISPATCHER_PC, exit 1: MOVI2R DISPATCHER_PC, exit
  //   b dispatcher                  b dispatcher
  //   2:                            2:
  static constexpr size_t CALL_LINK_SIZE = 7 * sizeof(u32);
  static constexpr s32 CALL_LINK_RETURN = 2 * sizeof(u32);

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;

  std::vector<std::pair<u8*, u8*>> m_ranges_to_free_on_next_codegen;
};
