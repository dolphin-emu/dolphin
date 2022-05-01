#include "Core/DefaultGeckoCodes.h"

void DefaultGeckoCodes::RunCodeInject(bool bNetplayEventCode, bool bIsRanked, bool bUseNightStadium)
{
  aWriteAddr = 0x802ED200;  // starting asm write addr

  Memory::Write_U8(0x1, aControllerRumble);  // enable rumble

  // handle asm writes for required code
  for (DefaultGeckoCode geckocode : sRequiredCodes)
    WriteAsm(geckocode);

  if (bNetplayEventCode || bIsRanked)
    InjectNetplayEventCode();

  if (bIsRanked)
    AddRankedCodes();

  if (bUseNightStadium)
    WriteAsm(sNightStadium);
}

void DefaultGeckoCodes::InjectNetplayEventCode()
{
  if (Memory::Read_U32(aBootToMainMenu) == 0x38600001) // Boot to Main Menu
    Memory::Write_U32(0x38600005, aBootToMainMenu);
  Memory::Write_U8(0x1, aSkipMemCardCheck);               // Skip Mem Card Check
  Memory::Write_U32(0x380400f6, aUnlimitedExtraInnings);  // Unlimited Extra Innings

  // Unlock Everything
  Memory::Write_U8(0x2, aUnlockEverything_1);
  for (int i = 0; i <= 0x5; i++)
    Memory::Write_U8(0x3, aUnlockEverything_2 + i);
  for (int i = 0; i <= 0x5; i++)
    Memory::Write_U8(0x1, aUnlockEverything_3 + i);
  for (int i = 0; i <= 0x29; i++)
    Memory::Write_U8(0x1, aUnlockEverything_4 + i);
  Memory::Write_U8(0x1, aUnlockEverything_5);
  for (int i = 0; i <= 0x35; i++)
    Memory::Write_U8(0x1, aUnlockEverything_6 + i);
  for (int i = 0; i <= 0x3; i++)
    Memory::Write_U8(0x1, aUnlockEverything_7 + i);
  Memory::Write_U8(0x1, aUnlockEverything_8);

  // Manual select
  Memory::Write_U8(0x0, aManualSelect_1);
  for (int i = 1; i <= 0x8; i++)
    Memory::Write_U8(i, aManualSelect_2 + i);

  // handle asm writes for netplay codes
  for (DefaultGeckoCode geckocode : sNetplayCodes)
    WriteAsm(geckocode);
}


// Adds codes specific to ranked, like the Pitch Clock
void DefaultGeckoCodes::AddRankedCodes()
{
  Memory::Write_U32(0x60000000, aPitchClock_1);
  Memory::Write_U32(0x60000000, aPitchClock_2);
  Memory::Write_U32(0x60000000, aPitchClock_3);

  WriteAsm(sPitchClock);
}


// calls this each time you want to write a code
void DefaultGeckoCodes::WriteAsm(DefaultGeckoCode CodeBlock)
{
  // METHODOLOGY:
  // use aWriteAddr as starting asm write addr
  // we compute a value, branchAmount, which tells how far we have to branch to get from the injection to aWriteAddr
  // do fancy bit wise math to formulate the hex value of the desired branch instruction
  // writes in first instruction in code block to aWriteAddr, increment aWriteAddr by 4, repeat for all code lines
  // once code block is finished, compute another branch instruction back to injection addr and write it in
  // repeat for all codes
  u32 branchToCode = 0x48000000;
  u32 baseAddr = aWriteAddr & 0x03ffffff;
  u32 codeAddr = CodeBlock.addr & 0x03ffffff;
  u32 branchAmount = (baseAddr - codeAddr) & 0x03ffffff;

  branchToCode += branchAmount;

  for (int i = 0; i < CodeBlock.codeLines.size(); i++)
  {
    Memory::Write_U32(CodeBlock.codeLines[i], aWriteAddr);
    aWriteAddr += 4;
  }

  u32 branchFromCode = 0x48000000;
  baseAddr = aWriteAddr & 0x03ffffff;
  codeAddr = CodeBlock.addr & 0x03ffffff;
  branchAmount = (codeAddr - baseAddr) & 0x03ffffff;

  branchFromCode += branchAmount + 4;
  Memory::Write_U32(branchFromCode, aWriteAddr);
  aWriteAddr += 4;

  if (CodeBlock.conditionalVal != 0 && Memory::Read_U32(CodeBlock.addr) != CodeBlock.conditionalVal)
    return;
  Memory::Write_U32(branchToCode, CodeBlock.addr);
}

// end
