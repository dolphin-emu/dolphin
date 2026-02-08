// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceIPL.h"

#include <cstring>
#include <string>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/Timer.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Sram.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/System.h"

#include "DiscIO/Enums.h"

namespace ExpansionInterface
{
// We should provide an option to choose from the above, or figure out the checksum (the algo in
// yagcd seems wrong) so that people can change default language.

static const char iplverPAL[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
                                     "(C) 1999 ArtX Inc.  All rights reserved."
                                     "PAL  Revision 1.0  ";

static const char iplverNTSC[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
                                      "(C) 1999 ArtX Inc.  All rights reserved.";

Common::Flags<RTCFlag> g_rtc_flags;

// bootrom descrambler reversed by segher
// Copyright 2008 Segher Boessenkool <segher@kernel.crashing.org>
void CEXIIPL::Descrambler(u8* data, u32 size)
{
  u8 acc = 0;
  u8 nacc = 0;

  u16 t = 0x2953;
  u16 u = 0xd9c2;
  u16 v = 0x3ff1;

  u8 x = 1;

  for (u32 it = 0; it < size;)
  {
    int t0 = t & 1;
    int t1 = (t >> 1) & 1;
    int u0 = u & 1;
    int u1 = (u >> 1) & 1;
    int v0 = v & 1;

    x ^= t1 ^ v0;
    x ^= (u0 | u1);
    x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

    if (t0 == u0)
    {
      v >>= 1;
      if (v0)
        v ^= 0xb3d0;
    }

    if (t0 == 0)
    {
      u >>= 1;
      if (u0)
        u ^= 0xfb10;
    }

    t >>= 1;
    if (t0)
      t ^= 0xa740;

    nacc++;
    acc = 2 * acc + x;
    if (nacc == 8)
    {
      data[it++] ^= acc;
      nacc = 0;
    }
  }
}

CEXIIPL::CEXIIPL(Core::System& system) : IEXIDevice(system)
{
  // Fill the ROM
  m_rom = std::make_unique<u8[]>(ROM_SIZE);

  // Load whole ROM dump
  // Note: The Wii doesn't have a copy of the IPL, only fonts.
  if (!system.IsWii() && Config::Get(Config::SESSION_LOAD_IPL_DUMP) &&
      LoadFileToIPL(SConfig::GetInstance().m_strBootROM, 0))
  {
    // Descramble the encrypted section (contains BS1 and BS2)
    Descrambler(&m_rom[0x100], 0x1afe00);

    const std::string_view name{reinterpret_cast<char*>(m_rom.get()),
                                strnlen(reinterpret_cast<char*>(m_rom.get()), 0x100)};
    INFO_LOG_FMT(BOOT, "Loaded bootrom: {}", name);
  }
  else
  {
    // If we are in Wii mode or if loading the GC IPL fails, we should still try to load fonts.

    // Copy header
    if (DiscIO::IsNTSC(SConfig::GetInstance().m_region))
      memcpy(&m_rom[0], iplverNTSC, sizeof(iplverNTSC));
    else
      memcpy(&m_rom[0], iplverPAL, sizeof(iplverPAL));

    // Load fonts
    LoadFontFile((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_SHIFT_JIS), 0x1aff00);
    LoadFontFile((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_WINDOWS_1252), 0x1fcf00);
  }

  auto& sram = system.GetSRAM();

  // Clear RTC
  sram.rtc = 0;

  // We Overwrite language selection here since it's possible on the GC to change the language as
  // you please
  sram.settings.language = Config::Get(Config::MAIN_GC_LANGUAGE);
  sram.settings.rtc_bias = 0;
  FixSRAMChecksums(&sram);
}

CEXIIPL::~CEXIIPL() = default;

void CEXIIPL::DoState(PointerWrap& p)
{
  auto& sram = m_system.GetSRAM();

  p.Do(sram);
  p.Do(g_rtc_flags);
  p.Do(m_command);
  p.Do(m_command_bytes_received);
  p.Do(m_cursor);
  p.Do(m_buffer);
  p.Do(m_fonts_loaded);
}

bool CEXIIPL::LoadFileToIPL(const std::string& filename, u32 offset)
{
  if (offset >= ROM_SIZE)
    return false;

  File::IOFile stream(filename, "rb");
  if (!stream)
    return false;

  const u64 filesize = stream.GetSize();
  if (!stream.ReadBytes(&m_rom[offset], std::min<u64>(filesize, ROM_SIZE - offset)))
    return false;

  m_fonts_loaded = true;
  return true;
}

std::string CEXIIPL::FindIPLDump(const std::string& path_prefix)
{
  std::string ipl_dump_path;

  if (File::Exists(path_prefix + DIR_SEP + USA_DIR + DIR_SEP + GC_IPL))
    ipl_dump_path = path_prefix + DIR_SEP + USA_DIR + DIR_SEP + GC_IPL;
  else if (File::Exists(path_prefix + DIR_SEP + EUR_DIR + DIR_SEP + GC_IPL))
    ipl_dump_path = path_prefix + DIR_SEP + EUR_DIR + DIR_SEP + GC_IPL;
  else if (File::Exists(path_prefix + DIR_SEP + JAP_DIR + DIR_SEP + GC_IPL))
    ipl_dump_path = path_prefix + DIR_SEP + JAP_DIR + DIR_SEP + GC_IPL;

  return ipl_dump_path;
}

bool CEXIIPL::HasIPLDump()
{
  std::string ipl_rom_path = FindIPLDump(File::GetUserPath(D_GCUSER_IDX));

  // If not found, check again in Sys folder
  if (ipl_rom_path.empty())
    ipl_rom_path = FindIPLDump(File::GetSysDirectory() + GC_SYS_DIR);

  return !ipl_rom_path.empty();
}

void CEXIIPL::LoadFontFile(const std::string& filename, u32 offset)
{
  // Official IPL fonts are copyrighted. Dolphin ships with a set of free font alternatives but
  // unfortunately the bundled fonts have different padding, causing issues with misplaced text
  // in some titles. This function check if the user has IPL dumps available and load the fonts
  // from those dumps instead of loading the bundled fonts

  if (!Config::Get(Config::SESSION_LOAD_IPL_DUMP))
  {
    // IPL loading disabled, load bundled font instead
    LoadFileToIPL(filename, offset);
    return;
  }

  // Check for IPL dumps in User folder
  std::string ipl_rom_path = FindIPLDump(File::GetUserPath(D_GCUSER_IDX));

  // If not found, check again in Sys folder
  if (ipl_rom_path.empty())
    ipl_rom_path = FindIPLDump(File::GetSysDirectory() + GC_SYS_DIR);

  // If the user has an IPL dump, load the font from it
  File::IOFile stream(ipl_rom_path, "rb");
  if (!stream)
  {
    // No IPL dump available, load bundled font instead
    LoadFileToIPL(filename, offset);
    return;
  }

  // Official Windows-1252 and Shift JIS fonts present on the IPL dumps are 0x2575 and 0x4a24d
  // bytes long respectively, so, determine the size of the font being loaded based on the offset
  const u64 fontsize = (offset == 0x1aff00) ? 0x4a24d : 0x2575;

  INFO_LOG_FMT(BOOT, "Found IPL dump, loading {} font from {}",
               (offset == 0x1aff00) ? "Shift JIS" : "Windows-1252", ipl_rom_path);

  if (!stream.Seek(offset, File::SeekOrigin::Begin) || !stream.ReadBytes(&m_rom[offset], fontsize))
  {
    WARN_LOG_FMT(BOOT, "Failed to read font from IPL dump.");
    return;
  }

  m_fonts_loaded = true;
}

void CEXIIPL::SetCS(int cs)
{
  if (cs)
  {
    m_command_bytes_received = 0;
    m_cursor = 0;
  }
}

void CEXIIPL::UpdateRTC()
{
  auto& sram = m_system.GetSRAM();
  sram.rtc = GetEmulatedTime(m_system, GC_EPOCH);
}

bool CEXIIPL::IsPresent() const
{
  return true;
}

void CEXIIPL::TransferByte(u8& data)
{
  // The first 4 bytes must be the command
  // If we haven't read it, do it now
  if (m_command_bytes_received < sizeof(m_command))
  {
    m_command.value <<= 8;
    m_command.value |= data;
    data = 0xff;
    m_command_bytes_received++;

    if (m_command_bytes_received == sizeof(m_command))
    {
      // Update RTC when a command is latched
      // This is technically not very accurate :(
      UpdateRTC();

      DEBUG_LOG_FMT(EXPANSIONINTERFACE, "IPL-DEV cmd {} {:08x} {:02x}",
                    m_command.is_write() ? "write" : "read", m_command.address(),
                    m_command.low_bits());
    }
  }
  else
  {
    // Actually read or write a byte
    const u32 address = m_command.address();

    DEBUG_LOG_FMT(EXPANSIONINTERFACE, "IPL-DEV data {} {:08x} {:02x}",
                  m_command.is_write() ? "write" : "read", address, data);

    auto UartFifoAccess = [&]() {
      if (m_command.is_write())
      {
        if (data != '\0')
          m_buffer += data;

        if (data == '\r')
        {
          NOTICE_LOG_FMT(OSREPORT, "{}", SHIFTJISToUTF8(m_buffer));
          m_buffer.clear();
        }
      }
      else
      {
        // "Queue Length"... return 0 cause we're instant
        data = 0;
      }
    };

    if (address < ROM_BASE + ROM_SIZE)
    {
      if (!m_command.is_write())
      {
        u32 dev_addr = address - ROM_BASE + m_cursor++;
        // TODO: Is this address wrapping correct? Needs a hardware test
        dev_addr %= ROM_SIZE;
        // Technically we should descramble here iff descrambling logic is enabled.
        // At the moment, we pre-decrypt the whole thing and
        // ignore the "enabled" bit - see CEXIIPL::CEXIIPL
        data = m_rom[dev_addr];

        if ((dev_addr >= 0x001AFF00) && (dev_addr <= 0x001FF474) && !m_fonts_loaded)
        {
          if (dev_addr >= 0x001FCF00)
          {
            PanicAlertFmtT("Error: Trying to access Windows-1252 fonts but they are not loaded. "
                           "Games may not show fonts correctly, or crash.");
          }
          else
          {
            PanicAlertFmtT("Error: Trying to access Shift JIS fonts but they are not loaded. "
                           "Games may not show fonts correctly, or crash.");
          }
          // Don't be a nag
          m_fonts_loaded = true;
        }
      }
    }
    else if (address >= SRAM_BASE && address < SRAM_BASE + SRAM_SIZE)
    {
      auto& sram = m_system.GetSRAM();
      u32 dev_addr = address - SRAM_BASE + m_cursor++;
      // TODO: Is this address wrapping correct? Needs a hardware test
      dev_addr %= SRAM_SIZE;
      if (m_command.is_write())
        sram[dev_addr] = data;
      else
        data = sram[dev_addr];
    }
    else if (address >= UART_BASE && address < UART_BASE + UART_SIZE)
    {
      switch (address - UART_BASE)
      {
      case 0:
        // Seems to be 16byte fifo
        UartFifoAccess();
        break;
      case 0xc:
        // Seen being written to after reading 4 bytes from barnacle
        break;
      case 0x4c:
        DEBUG_LOG_FMT(OSREPORT, "UART Barnacle {:x}", data);
        break;
      }
    }
    else if (address >= WII_RTC_BASE && address < WII_RTC_BASE + WII_RTC_SIZE &&
             address - WII_RTC_BASE == 0x20)
    {
      if (m_command.is_write())
        g_rtc_flags.m_hex = data;
      else
        data = g_rtc_flags.m_hex;
    }
    else if (address >= EUART_BASE && address < EUART_BASE + EUART_SIZE)
    {
      switch (address - EUART_BASE)
      {
      case 0:
        // Writes 0xf2 then 0xf3 on EUART init. Just need to return non-zero
        // so we can leave the byte untouched.
        break;
      case 4:
        UartFifoAccess();
        break;
      }
    }
    else
    {
      NOTICE_LOG_FMT(EXPANSIONINTERFACE, "IPL-DEV Accessing unknown device");
    }
  }
}

u32 CEXIIPL::GetEmulatedTime(Core::System& system, u32 epoch)
{
  u64 ltime = 0;

  auto& movie = system.GetMovie();
  if (movie.IsMovieActive())
  {
    ltime = movie.GetRecordingStartTime();

    // let's keep time moving forward, regardless of what it starts at
    ltime += system.GetCoreTiming().GetTicks() / system.GetSystemTimers().GetTicksPerSecond();
  }
  else if (NetPlay::IsNetPlayRunning())
  {
    ltime = NetPlay_GetEmulatedTime();

    // let's keep time moving forward, regardless of what it starts at
    ltime += system.GetCoreTiming().GetTicks() / system.GetSystemTimers().GetTicksPerSecond();
  }
  else
  {
    ASSERT(!Core::WantsDeterminism());
    ltime = Common::Timer::GetLocalTimeSinceJan1970() -
            system.GetSystemTimers().GetLocalTimeRTCOffset();
  }

  return static_cast<u32>(ltime) - epoch;
}
}  // namespace ExpansionInterface
