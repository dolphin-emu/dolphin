// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE_Misc.h"

#include <chrono>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/PowerPC/PowerPC.h"

namespace HLE_Misc
{
// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
  NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
  ::PanicAlert("HLE: PanicAlert %08x", LR);
  NPC = LR;
}

void HBReload()
{
  // There isn't much we can do. Just stop cleanly.
  CPU::Break();
  Host_Message(WM_USER_STOP);
}

void GeckoCodeHandlerICacheFlush()
{
  // Work around the codehandler not properly invalidating the icache, but
  // only the first few frames.
  // (Project M uses a conditional to only apply patches after something has
  // been read into memory, or such, so we do the first 5 frames.  More
  // robust alternative would be to actually detect memory writes, but that
  // would be even uglier.)
  u32 gch_gameid = PowerPC::HostRead_U32(Gecko::INSTALLER_BASE_ADDRESS);
  if (gch_gameid - Gecko::MAGIC_GAMEID == 5)
  {
    return;
  }
  else if (gch_gameid - Gecko::MAGIC_GAMEID > 5)
  {
    gch_gameid = Gecko::MAGIC_GAMEID;
  }
  PowerPC::HostWrite_U32(gch_gameid + 1, Gecko::INSTALLER_BASE_ADDRESS);

  PowerPC::ppcState.iCache.Reset();
}

// Because Dolphin messes around with the CPU state instead of patching the game binary, we
// need a way to branch into the GCH from an arbitrary PC address. Branching is easy, returning
// back is the hard part. This HLE function acts as a trampoline that restores the original LR, SP,
// and PC before the magic, invisible BL instruction happened.
void GeckoReturnTrampoline()
{
  // Stack frame is built in GeckoCode.cpp, Gecko::RunCodeHandler.
  u32 SP = GPR(1);
  GPR(1) = PowerPC::HostRead_U32(SP + 8);
  NPC = PowerPC::HostRead_U32(SP + 12);
  LR = PowerPC::HostRead_U32(SP + 16);
  PowerPC::ExpandCR(PowerPC::HostRead_U32(SP + 20));
  for (int i = 0; i < 14; ++i)
  {
    riPS0(i) = PowerPC::HostRead_U64(SP + 24 + 2 * i * sizeof(u64));
    riPS1(i) = PowerPC::HostRead_U64(SP + 24 + (2 * i + 1) * sizeof(u64));
  }
}

// int SSL_read(SSL *ssl, void *buf, int num);
void OpenSSLRead()
{
  u32 ssl = GPR(3);
  u32 buf = GPR(4);
  u32 num = GPR(5);

  HLE::UnPatch("SSL_read");
  PowerPC::StepOut(std::chrono::seconds(5));
  HLE::PatchFunctions();

  auto& config = SConfig::GetInstance();
  if (!config.m_OpenSSLDumpPCAP)
    return;

  s32 ret = GPR(3);
  if (ret <= 0 || !PowerPC::HostIsRAMAddress(ssl) || !PowerPC::HostIsRAMAddress(buf) ||
      !PowerPC::HostIsRAMAddress(buf + num))
  {
    ERROR_LOG(OSHLE, "Failed to log SSL_read(%08x, %08x, %08x) = %d", ssl, buf, num, ret);
    return;
  }
  INFO_LOG(OSHLE, "SSL_read(%08x, %08x, %08x) = %d", ssl, buf, num, ret);

  std::vector<u8> data(num);
  Memory::CopyFromEmu(&data[0], buf, num);
  (void)ssl;
  config.m_openssl_logger->LogTCPRead(data.data(), data.size(), -1);
}

// int SSL_write(SSL *ssl, const void *buf, int num);
void OpenSSLWrite()
{
  u32 ssl = GPR(3);
  u32 buf = GPR(4);
  u32 num = GPR(5);

  HLE::UnPatch("SSL_write");
  PowerPC::StepOut(std::chrono::seconds(5));
  HLE::PatchFunctions();

  auto& config = SConfig::GetInstance();
  if (!config.m_OpenSSLDumpPCAP)
    return;

  s32 ret = GPR(3);
  if (ret <= 0 || !PowerPC::HostIsRAMAddress(ssl) || !PowerPC::HostIsRAMAddress(buf) ||
      !PowerPC::HostIsRAMAddress(buf + num))
  {
    ERROR_LOG(OSHLE, "Failed to log SSL_write(%08x, %08x, %08x) = %d", ssl, buf, num, ret);
    return;
  }
  INFO_LOG(OSHLE, "SSL_write(%08x, %08x, %08x) = %d", ssl, buf, num, ret);

  std::vector<u8> data(num);
  Memory::CopyFromEmu(&data[0], buf, num);
  (void)ssl;
  config.m_openssl_logger->LogTCPWrite(data.data(), data.size(), -1);
}

}  // namespace HLE_Misc
