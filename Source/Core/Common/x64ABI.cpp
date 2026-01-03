// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/x64ABI.h"

#include <algorithm>
#include <array>
#include <optional>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"

using namespace Gen;

// Shared code between Win64 and Unix64

void XEmitter::ABI_CalculateFrameSize(BitSet32 mask, size_t rsp_alignment, size_t needed_frame_size,
                                      size_t* shadowp, size_t* subtractionp, size_t* xmm_offsetp)
{
  size_t shadow = 0;
#if defined(_WIN32)
  shadow = 0x20;
#endif

  int count = (mask & ABI_ALL_GPRS).Count();
  rsp_alignment -= count * 8;
  size_t subtraction = 0;
  int fpr_count = (mask & ABI_ALL_FPRS).Count();
  if (fpr_count)
  {
    // If we have any XMMs to save, we must align the stack here.
    subtraction = rsp_alignment & 0xf;
  }
  subtraction += 16 * fpr_count;
  size_t xmm_base_subtraction = subtraction;
  subtraction += needed_frame_size;
  subtraction += shadow;
  // Final alignment.
  rsp_alignment -= subtraction;
  subtraction += rsp_alignment & 0xf;

  *shadowp = shadow;
  *subtractionp = subtraction;
  *xmm_offsetp = subtraction - xmm_base_subtraction;
}

// This is using a hand-rolled algorithm. The goal is zero memory allocations, not necessarily
// the best JIT-time time complexity. (The number of moves is usually very small.)
void XEmitter::ParallelMoves(RegisterMove* begin, RegisterMove* end,
                             std::array<u8, 16>* source_reg_uses,
                             std::array<u8, 16>* destination_reg_uses)
{
  while (begin != end)
  {
    bool removed_moves_during_this_loop_iteration = false;

    RegisterMove* move = end;
    while (move != begin)
    {
      RegisterMove* prev_move = move;
      --move;
      if ((*source_reg_uses)[move->destination] == 0)
      {
        if (move->bits == move->extend_bits || (!move->signed_move && move->bits == 32))
          MOV(move->bits, R(move->destination), move->source);
        else if (move->signed_move)
          MOVSX(move->extend_bits, move->bits, move->destination, move->source);
        else
          MOVZX(32, move->bits, move->destination, move->source);

        (*destination_reg_uses)[move->destination]++;
        if (const std::optional<X64Reg> base_reg = move->source.GetBaseReg())
          (*source_reg_uses)[*base_reg]--;
        if (const std::optional<X64Reg> index_reg = move->source.GetIndexReg())
          (*source_reg_uses)[*index_reg]--;

        std::move(prev_move, end, move);
        --end;
        removed_moves_during_this_loop_iteration = true;
      }
    }

    if (!removed_moves_during_this_loop_iteration)
    {
      // We need to break a cycle using a temporary register.

      const BitSet32 temp_reg_candidates = ABI_ALL_CALLER_SAVED & ABI_ALL_GPRS;
      const auto temp_reg =
          std::find_if(temp_reg_candidates.begin(), temp_reg_candidates.end(), [&](int reg) {
            return (*source_reg_uses)[reg] == 0 && (*destination_reg_uses)[reg] == 0;
          });
      ASSERT_MSG(COMMON, temp_reg != temp_reg_candidates.end(), "Out of registers");

      const X64Reg source = begin->destination;
      const X64Reg destination = static_cast<X64Reg>(*temp_reg);

      const bool need_64_bit_move = std::any_of(begin, end, [source](const RegisterMove& m) {
        return (m.source.GetBaseReg() == source || m.source.GetIndexReg() == source) &&
               (m.source.scale != SCALE_NONE || m.bits == 64);
      });

      MOV(need_64_bit_move ? 64 : 32, R(destination), R(source));
      (*source_reg_uses)[destination] = (*source_reg_uses)[source];
      (*source_reg_uses)[source] = 0;

      std::for_each(begin, end, [source, destination](RegisterMove& m) {
        if (m.source.GetBaseReg() == source)
          m.source.offsetOrBaseReg = destination;
        if (m.source.GetIndexReg() == source)
          m.source.indexReg = destination;
      });
    }
  }
}

size_t XEmitter::ABI_PushRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                                 size_t needed_frame_size)
{
  mask[RSP] = false;  // Stack pointer is never pushed
  size_t shadow, subtraction, xmm_offset;
  ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction,
                         &xmm_offset);

  if (mask[RBP])
  {
    // Make a nice stack frame for any debuggers or profilers that might be looking at this
    PUSH(RBP);
    MOV(64, R(RBP), R(RSP));
  }
  for (int r : (mask & ABI_ALL_GPRS & ~BitSet32{RBP}))
    PUSH((X64Reg)r);

  if (subtraction)
    SUB(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

  for (int x : (mask & ABI_ALL_FPRS))
  {
    MOVAPD(MDisp(RSP, (int)xmm_offset), (X64Reg)(x - 16));
    xmm_offset += 16;
  }

  return shadow;
}

void XEmitter::ABI_PopRegistersAndAdjustStack(BitSet32 mask, size_t rsp_alignment,
                                              size_t needed_frame_size)
{
  mask[RSP] = false;  // Stack pointer is never pushed
  size_t shadow, subtraction, xmm_offset;
  ABI_CalculateFrameSize(mask, rsp_alignment, needed_frame_size, &shadow, &subtraction,
                         &xmm_offset);

  for (int x : (mask & ABI_ALL_FPRS))
  {
    MOVAPD((X64Reg)(x - 16), MDisp(RSP, (int)xmm_offset));
    xmm_offset += 16;
  }

  if (subtraction)
    ADD(64, R(RSP), subtraction >= 0x80 ? Imm32((u32)subtraction) : Imm8((u8)subtraction));

  for (int r = 15; r >= 0; r--)
  {
    if (r != RBP && mask[r])
      POP((X64Reg)r);
  }
  // RSP is pushed first and popped last to make debuggers/profilers happy
  if (mask[RBP])
    POP(RBP);
}

void XEmitter::MOVTwo(int bits, Gen::X64Reg dst1, Gen::X64Reg src1, s32 offset1, Gen::X64Reg dst2,
                      Gen::X64Reg src2)
{
  if (dst1 == src2 && dst2 == src1)
  {
    XCHG(bits, R(src1), R(src2));
    if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
  }
  else if (src2 != dst1)
  {
    if (dst1 != src1 && offset1)
      LEA(bits, dst1, MDisp(src1, offset1));
    else if (dst1 != src1)
      MOV(bits, R(dst1), R(src1));
    else if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
    if (dst2 != src2)
      MOV(bits, R(dst2), R(src2));
  }
  else
  {
    if (dst2 != src2)
      MOV(bits, R(dst2), R(src2));
    if (dst1 != src1 && offset1)
      LEA(bits, dst1, MDisp(src1, offset1));
    else if (dst1 != src1)
      MOV(bits, R(dst1), R(src1));
    else if (offset1)
      ADD(bits, R(dst1), Imm32(offset1));
  }
}
