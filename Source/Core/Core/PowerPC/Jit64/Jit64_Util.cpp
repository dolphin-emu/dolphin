// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GPFifo.h"
#include "Core/PowerPC/Jit64/Jit.h"

using namespace Gen;

// This set of functions encapsulates the entire craziness we need to deal
// with when dealing with loads and stores. When running in MMU mode, we need
// to ensure that all modifications made to input registers (other than the
// write to a register as part of a load) are rolled back in the case of
// exception. In some cases we use a scratch register for this, in other cases
// we create a register transaction that will restore the register value.

void Jit64::SafeWrite(GPRRegister& reg_value, GPRRegister& reg_addr, GPRRegister& offset,
                      int accessSize, bool swap, bool update)
{
  _assert_msg_(DYNAREC, accessSize == 8 || accessSize == 16 || accessSize == 32 || accessSize == 64,
               "Invalid write size: %d", accessSize);

  if (update && !reg_addr.HasPPCRegister())
  {
    // This form of instruction is illegal
    WriteInvalidInstruction();
    return;
  }

  bool valueAddressShareRegister = reg_value.IsAliasOf(reg_addr);
  bool valueOffsetShareRegister = reg_value.IsAliasOf(offset);

  // We want to preserve any non-scratch registers passed into this
  // function. If a scratch register is passed, we assume it may be safely
  // clobbered.
  if (WriteClobbersRegValue(accessSize, swap) && reg_value.HasPPCRegister())
  {
    // TODO: test for free register, otherwise swap back
    auto scratch = reg_value.BorrowCopy();
    SafeWrite(scratch, reg_addr, offset, accessSize, swap, update);
    return;
  }

  // Constant writes have a much faster path
  if (reg_addr.IsImm() && offset.IsImm())
  {
    u32 const_addr = reg_addr.Imm32() + offset.Imm32();
    if (UnsafeWriteToConstAddress(accessSize, reg_value, const_addr, swap))
    {
      if (update)
        reg_addr.SetImm32(const_addr);
      return;
    }

    // Nope, we'll fall back to the standard write...
  }

  // Note that while SafeWriteRegToReg takes a register and an OpArg, it really wants things to be
  // registers or immediates.
  auto val =
      (reg_value.IsImm() || reg_value.IsRegBound()) ? reg_value : reg_value.Bind(BindMode::Read);

  // Optimize swap flag
  if (swap)
  {
    // No need to swap
    if (accessSize == 8)
    {
      swap = false;
    }
    else
    {
      // We can swap these ahead of time
      if (val.IsImm())
      {
        swap = false;
        switch (accessSize)
        {
        case 16:
          val = regs.gpr.Imm32(Common::swap16(val.Imm32()));
          break;
        case 32:
          val = regs.gpr.Imm32(Common::swap32(val.Imm32()));
          break;
        case 64:
          // We don't support 64-bit immediates (TODO?)
          UnexpectedInstructionForm();
          break;
        }
      }
    }
  }
  u32 flags = swap ? 0 : SAFE_LOADSTORE_NO_SWAP;

  // We need to bind a register, so may as well just add the offset here
  if (reg_addr.IsImm())
  {
    if (update)
    {
      // Update reg_addr inside a transaction
      auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
      MOV_sum(32, xa, xa, offset);
      SafeWriteRegToReg(val, xa, accessSize, 0, CallerSavedRegistersInUse(), flags);
    }
    else
    {
      auto scratch = regs.gpr.Borrow();
      MOV_sum(32, scratch, reg_addr, offset);
      SafeWriteRegToReg(val, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
    }
    return;
  }

  auto addr = reg_addr.Bind(BindMode::Read);

  // Easy: zero offset, so updates don't matter
  if (offset.IsZero())
  {
    SafeWriteRegToReg(valueAddressShareRegister ? addr : val, addr, accessSize, 0,
                      CallerSavedRegistersInUse(), flags);
    return;
  }

  // If we're updating we can clobber address (transactionally)
  if (update)
  {
    if (valueAddressShareRegister)
    {
      // Keep value in a scratch register so we can update the address
      // without worrying about it
      auto scratch = reg_value.BorrowCopy();
      auto xa = addr.Bind(BindMode::ReadWriteTransactional);
      MOV_sum(32, xa, xa, offset);
      SafeWriteRegToReg((X64Reg)scratch, xa, accessSize, 0, CallerSavedRegistersInUse(), flags);
    }
    else
    {
      // Update reg_addr inside a transaction
      auto xa = addr.Bind(BindMode::ReadWriteTransactional);
      MOV_sum(32, xa, xa, offset);
      SafeWriteRegToReg(val, xa, accessSize, 0, CallerSavedRegistersInUse(), flags);
    }
    return;
  }

  if (valueAddressShareRegister)
  {
    // Not transactional, since we only modify value by writing to it
    auto xv = reg_value.Bind(BindMode::ReadWrite);
    auto scratch = regs.gpr.Borrow();
    MOV_sum(32, scratch, xv, offset);
    SafeWriteRegToReg((X64Reg)xv, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
    return;
  }

  if (valueOffsetShareRegister)
  {
    // Not transactional, since we only modify value by writing to it
    auto xv = reg_value.Bind(BindMode::ReadWrite);
    auto scratch = regs.gpr.Borrow();
    MOV_sum(32, scratch, addr, xv);
    SafeWriteRegToReg((X64Reg)xv, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
    return;
  }

  // No update so we need to sum things in a scratch register. We can't just pass the offset into
  // SafeWriteRegToReg because that will clobber the address register we pass in
  auto scratch = regs.gpr.Borrow();
  MOV_sum(32, scratch, addr, offset);
  SafeWriteRegToReg(val, scratch, accessSize, 0, CallerSavedRegistersInUse(), flags);
}

bool Jit64::UnsafeWriteToConstAddress(int accessSize, GPRRegister& value, u32 address, bool swap)
{
  // This needs to be a scratch register if it would be clobbered by SwapAndStore (ie: if we don't
  // have MOVBE)
  _assert_msg_(DYNAREC, !WriteClobbersRegValue(accessSize, swap) || !value.HasPPCRegister(),
               "Asked to clobber a PPC register");

  // If we already know the address through constant folding, we can do a
  // few interesting things:

  // If this is the WPAR address, we can generate optimized FIFO routines
  if (jit->jo.optimizeGatherPipe && PowerPC::IsOptimizableGatherPipeWrite(address))
  {
    value.LoadIfNotImmediate();
    UnsafeWriteGatherPipe(accessSize, value, swap);
    return true;
  }

  // If we know for sure this is RAM, we can just write to it
  if (PowerPC::IsOptimizableRAMAddress(address))
  {
    value.LoadIfNotImmediate();
    UnsafeWriteToConstRamAddress(accessSize, value, address, swap);
    return true;
  }

  return false;
}

void Jit64::UnsafeWriteToMemory(int accessSize, GPRRegister& value, OpArg target, bool swap)
{
  OpArg arg = value;
  if (arg.IsImm())
  {
    arg = arg.AsImm(accessSize);
    if (swap)
      arg = arg.SwapImm();
    MOV(accessSize, target, arg);
  }
  else
  {
    if (swap)
      SwapAndStore(accessSize, target, arg.GetSimpleReg());
    else
      MOV(accessSize, target, arg);
  }
}

void Jit64::UnsafeWriteGatherPipe(int accessSize, GPRRegister& value, bool swap)
{
  u32 gather_pipe = (u32)(u64)GPFifo::m_gatherPipe;
  _assert_msg_(DYNA_REC, gather_pipe <= 0x7FFFFFFF, "Gather pipe not in low 2GB of memory!");

  // Append this write to the pipe
  auto scratch = regs.gpr.Borrow();
  MOV(32, R(scratch), M(&GPFifo::m_gatherPipeCount));
  UnsafeWriteToMemory(accessSize, value, MDisp(scratch, gather_pipe), swap);
  ADD(32, R(scratch), Imm8(accessSize >> 3));
  MOV(32, M(&GPFifo::m_gatherPipeCount), R(scratch));
  jit->js.fifoBytesThisBlock += accessSize >> 3;
}

void Jit64::UnsafeWriteToConstRamAddress(int accessSize, GPRRegister& value, u32 address, bool swap)
{
  // We can avoid a scratch register if the address is in the lower half
  if (address > 0x80000000)
  {
    // Technically the borrow doesn't last long enough, but nothing will
    // re-allocate it before we're done
    auto addr = regs.gpr.Borrow();
    MOV(32, addr, Imm32(address));
    UnsafeWriteToMemory(accessSize, value, MRegSum(RMEM, addr), swap);
  }
  else
  {
    UnsafeWriteToMemory(accessSize, value, MDisp(RMEM, address), swap);
  }
}

void Jit64::SafeLoad(GPRNative& reg_value, GPRRegister& reg_addr, GPRRegister& offset,
                     int accessSize, bool signExtend, bool swap, bool update)
{
  bool valueAddressShareRegister = reg_value.IsAliasOf(reg_addr);
  bool valueOffsetShareRegister = reg_value.IsAliasOf(offset);

  if (update && !reg_addr.HasPPCRegister())
  {
    // This form of instruction is illegal
    WriteInvalidInstruction();
    return;
  }

  if (!swap && accessSize == 8)
  {
    // Swapping automatically is currently cheaper
    swap = true;
  }

  // Constant writes have a much faster path
  if (reg_addr.IsImm() && offset.IsImm())
  {
    u32 const_addr = reg_addr.Imm32() + offset.Imm32();
    if (UnsafeReadFromConstAddress(accessSize, reg_value, const_addr, signExtend, swap))
    {
      if (update)
        reg_addr.SetImm32(const_addr);
      return;
    }

    // Nope, we'll fall back to the standard read...
  }

  if (!swap)
  {
    // TODO: add a flag to disable swap in SafeLoadToReg
    SafeLoad(reg_value, reg_addr, offset, accessSize, signExtend, true, update);
    BSWAP(accessSize, reg_value);
    return;
  }

  // Easy: zero offset, so updates don't matter
  if (offset.IsZero())
  {
    // Neither of these is transactional, since we won't update address
    // unless the load succeeds
    if (valueAddressShareRegister)
    {
      auto xv = reg_value.Bind(BindMode::ReadWrite);
      SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    }
    else
    {
      auto addr =
          (reg_addr.IsImm() || reg_addr.IsRegBound()) ? reg_addr : reg_addr.Bind(BindMode::Read);
      SafeLoadToReg(reg_value, addr, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    }
    return;
  }

  // If we're updating we can clobber address (transactionally)
  if (update)
  {
    if (valueAddressShareRegister)
    {
      // If update is set, the value and address cannot share a register
      WriteInvalidInstruction();
      return;
    }
    else if (valueOffsetShareRegister)
    {
      auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
      auto xo = offset.Bind(BindMode::ReadWrite);
      MOV_sum(32, xa, xa, xo);
      SafeLoadToReg(xo, xa, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    }
    else
    {
      auto xa = reg_addr.Bind(BindMode::ReadWriteTransactional);
      MOV_sum(32, xa, xa, offset);
      SafeLoadToReg(reg_value, xa, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    }
    return;
  }

  // If we know the address we can pass it as an immediate
  if (reg_addr.IsImm() && offset.IsImm())
  {
    auto addr = Gen::Imm32(reg_addr.Imm32() + offset.Imm32());
    SafeLoadToReg(reg_value, addr, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    return;
  }

  // We know that address is getting clobbered
  if (valueAddressShareRegister)
  {
    auto xv = reg_value.Bind(BindMode::ReadWriteTransactional);
    MOV_sum(32, xv, xv, offset);
    SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    return;
  }

  // We know that offset is getting clobbered
  if (valueOffsetShareRegister)
  {
    auto xv = reg_value.Bind(BindMode::ReadWriteTransactional);
    MOV_sum(32, xv, reg_addr, xv);
    SafeLoadToReg(xv, xv, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
    return;
  }

  if (offset.IsImm())
  {
    // Passing an offset requires us to provide a scratch register
    auto offsetScratch = regs.gpr.BorrowAnyBut(BitSet32{RSCRATCH});
    auto addr =
        reg_addr.IsImm() || reg_addr.IsRegBound() ? reg_addr : reg_addr.Bind(BindMode::Read);
    SafeLoadToReg(reg_value, addr, accessSize, offset.Imm32(), CallerSavedRegistersInUse(),
                  signExtend, 0, offsetScratch);
    return;
  }

  // Non-immediate offset so we need to sum things in a scratch register.
  auto scratch = regs.gpr.BorrowAnyBut(BitSet32{RSCRATCH});
  MOV_sum(32, scratch, reg_addr, offset);
  SafeLoadToReg(reg_value, scratch, accessSize, 0, CallerSavedRegistersInUse(), signExtend, 0);
}

bool Jit64::UnsafeReadFromConstAddress(int accessSize, GPRNative& value, u32 address,
                                       bool signExtend, bool swap)
{
  // If we know for sure this is RAM, we can just write to it
  if (PowerPC::IsOptimizableRAMAddress(address))
  {
    UnsafeReadFromConstRamAddress(accessSize, value, address, signExtend, swap);
    return true;
  }

  return false;
}

void Jit64::UnsafeReadFromConstRamAddress(int accessSize, GPRNative& value, u32 address,
                                          bool signExtend, bool swap)
{
  // We can avoid a scratch register if the address is in the lower half
  if (address > 0x80000000)
  {
    auto addr = regs.gpr.Borrow();
    MOV(32, addr, Imm32(address));
    UnsafeReadFromMemory(accessSize, value, MRegSum(RMEM, addr), signExtend, swap);
  }
  else
  {
    UnsafeReadFromMemory(accessSize, value, MDisp(RMEM, address), signExtend, swap);
  }
}

void Jit64::UnsafeReadFromMemory(int accessSize, GPRNative& value, Gen::OpArg target,
                                 bool signExtend, bool swap)
{
  if (swap)
  {
    LoadAndSwap(accessSize, value, target, signExtend);
  }
  else
  {
    LoadAndExtend(accessSize, value, target, signExtend);
  }
}
