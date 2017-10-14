// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceIPL.h"

#include <cstring>
#include <string>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Sram.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"

#include "DiscIO/Enums.h"

namespace ExpansionInterface
{
// We should provide an option to choose from the above, or figure out the checksum (the algo in
// yagcd seems wrong)
// so that people can change default language.

static const char iplverPAL[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
                                     "(C) 1999 ArtX Inc.  All rights reserved."
                                     "PAL  Revision 1.0  ";

static const char iplverNTSC[0x100] = "(C) 1999-2001 Nintendo.  All rights reserved."
                                      "(C) 1999 ArtX Inc.  All rights reserved.";

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

CEXIIPL::CEXIIPL() : m_uPosition(0), m_uAddress(0), m_uRWOffset(0), m_FontsLoaded(false)
{
  // Create the IPL
  m_pIPL = static_cast<u8*>(Common::AllocateMemoryPages(ROM_SIZE));

  // Load whole ROM dump
  // Note: The Wii doesn't have a copy of the IPL, only fonts.
  if (!SConfig::GetInstance().bWii && LoadFileToIPL(SConfig::GetInstance().m_strBootROM, 0))
  {
    // Descramble the encrypted section (contains BS1 and BS2)
    Descrambler(m_pIPL + 0x100, 0x1afe00);
    INFO_LOG(BOOT, "Loaded bootrom: %s", m_pIPL);  // yay for null-terminated strings ;p
  }
  else
  {
    // If we are in Wii mode or if loading the GC IPL fails, we should still try to load fonts.

    // Copy header
    if (DiscIO::IsNTSC(SConfig::GetInstance().m_region))
      memcpy(m_pIPL, iplverNTSC, sizeof(iplverNTSC));
    else
      memcpy(m_pIPL, iplverPAL, sizeof(iplverPAL));

    // Load fonts
    LoadFontFile((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_SHIFT_JIS), 0x1aff00);
    LoadFontFile((File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + FONT_WINDOWS_1252), 0x1fcf00);
  }

  // Clear RTC
  memset(m_RTC, 0, sizeof(m_RTC));

  // We Overwrite language selection here since it's possible on the GC to change the language as
  // you please
  g_SRAM.lang = SConfig::GetInstance().SelectedLanguage;
  FixSRAMChecksums();

  Common::WriteProtectMemory(m_pIPL, ROM_SIZE);
  m_uAddress = 0;
}

CEXIIPL::~CEXIIPL()
{
  Common::FreeMemoryPages(m_pIPL, ROM_SIZE);
  m_pIPL = nullptr;

  // SRAM
  if (!g_SRAM_netplay_initialized)
  {
    File::IOFile file(SConfig::GetInstance().m_strSRAM, "wb");
    file.WriteArray(&g_SRAM, 1);
  }
  else
  {
    g_SRAM_netplay_initialized = false;
  }
}
void CEXIIPL::DoState(PointerWrap& p)
{
  p.Do(m_RTC);
  p.Do(m_uPosition);
  p.Do(m_uAddress);
  p.Do(m_uRWOffset);
  p.Do(m_buffer);
  p.Do(m_FontsLoaded);
}

bool CEXIIPL::LoadFileToIPL(const std::string& filename, u32 offset)
{
  File::IOFile stream(filename, "rb");
  if (!stream)
    return false;

  u64 filesize = stream.GetSize();

  if (!stream.ReadBytes(m_pIPL + offset, filesize))
    return false;

  m_FontsLoaded = true;
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

void CEXIIPL::LoadFontFile(const std::string& filename, u32 offset)
{
  // Official IPL fonts are copyrighted. Dolphin ships with a set of free font alternatives but
  // unfortunately the bundled fonts have different padding, causing issues with misplaced text
  // in some titles. This function check if the user has IPL dumps available and load the fonts
  // from those dumps instead of loading the bundled fonts

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
  u64 fontsize = (offset == 0x1aff00) ? 0x4a24d : 0x2575;

  INFO_LOG(BOOT, "Found IPL dump, loading %s font from %s",
           ((offset == 0x1aff00) ? "Shift JIS" : "Windows-1252"), (ipl_rom_path).c_str());

  stream.Seek(offset, 0);
  stream.ReadBytes(m_pIPL + offset, fontsize);

  m_FontsLoaded = true;
}

void CEXIIPL::SetCS(int _iCS)
{
  if (_iCS)
  {
    // cs transition to high
    m_uPosition = 0;
  }
}

void CEXIIPL::UpdateRTC()
{
  u32 epoch =
      (SConfig::GetInstance().bWii || SConfig::GetInstance().m_is_mios) ? WII_EPOCH : GC_EPOCH;
  u32 rtc = Common::swap32(GetEmulatedTime(epoch));
  std::memcpy(m_RTC, &rtc, sizeof(u32));
}

bool CEXIIPL::IsPresent() const
{
  return true;
}

void CEXIIPL::TransferByte(u8& _uByte)
{
  // The first 4 bytes must be the address
  // If we haven't read it, do it now
  if (m_uPosition <= 3)
  {
    m_uAddress <<= 8;
    m_uAddress |= _uByte;
    m_uRWOffset = 0;
    _uByte = 0xFF;

    // Check if the command is complete
    if (m_uPosition == 3)
    {
      // Get the time...
      UpdateRTC();

      // Log the command
      std::string device_name;

      switch (CommandRegion())
      {
      case REGION_RTC:
        device_name = "RTC";
        break;
      case REGION_SRAM:
        device_name = "SRAM";
        break;
      case REGION_UART:
        device_name = "UART";
        break;
      case REGION_EUART:
      case REGION_EUART_UNK:
        device_name = "EUART";
        break;
      case REGION_UART_UNK:
        device_name = "UART Other?";
        break;
      case REGION_BARNACLE:
        device_name = "UART Barnacle";
        break;
      case REGION_WRTC0:
      case REGION_WRTC1:
      case REGION_WRTC2:
        device_name = "Wii RTC flags - not implemented";
        break;
      default:
        if ((m_uAddress >> 6) < ROM_SIZE)
        {
          device_name = "ROM";
        }
        else
        {
          device_name = "illegal address";
          _dbg_assert_msg_(EXPANSIONINTERFACE, 0, "EXI IPL-DEV: %s %08x", device_name.c_str(),
                           m_uAddress);
        }
        break;
      }

      DEBUG_LOG(EXPANSIONINTERFACE, "%s %s %08x", device_name.c_str(),
                IsWriteCommand() ? "write" : "read", m_uAddress);
    }
  }
  else
  {
    // Actually read or write a byte
    switch (CommandRegion())
    {
    case REGION_RTC:
      if (IsWriteCommand())
        m_RTC[(m_uAddress & 0x03) + m_uRWOffset] = _uByte;
      else
        _uByte = m_RTC[(m_uAddress & 0x03) + m_uRWOffset];
      break;

    case REGION_SRAM:
      if (IsWriteCommand())
        g_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset] = _uByte;
      else
        _uByte = g_SRAM.p_SRAM[(m_uAddress & 0x3F) + m_uRWOffset];
      break;

    case REGION_UART:
    case REGION_EUART:
      if (IsWriteCommand())
      {
        if (_uByte != '\0')
          m_buffer += _uByte;

        if (_uByte == '\r')
        {
          NOTICE_LOG(OSREPORT, "%s", SHIFTJISToUTF8(m_buffer).c_str());
          m_buffer.clear();
        }
      }
      else
      {
        // "Queue Length"... return 0 cause we're instant
        _uByte = 0;
      }
      break;

    case REGION_EUART_UNK:
      // Writes 0xf2 then 0xf3 on EUART init. Just need to return non-zero
      // so we can leave the byte untouched.
      break;

    case REGION_UART_UNK:
      DEBUG_LOG(OSREPORT, "UART? %x", _uByte);
      _uByte = 0xff;
      break;

    case REGION_BARNACLE:
      DEBUG_LOG(OSREPORT, "UART Barnacle %x", _uByte);
      break;

    case REGION_WRTC0:
    case REGION_WRTC1:
    case REGION_WRTC2:
    // Wii only RTC flags... afaik just the Wii Menu initialize it
    default:
      if ((m_uAddress >> 6) < ROM_SIZE)
      {
        if (!IsWriteCommand())
        {
          u32 position = ((m_uAddress >> 6) & ROM_MASK) + m_uRWOffset;

          // Technically we should descramble here iff descrambling logic is enabled.
          // At the moment, we pre-decrypt the whole thing and
          // ignore the "enabled" bit - see CEXIIPL::CEXIIPL
          _uByte = m_pIPL[position];

          if ((position >= 0x001AFF00) && (position <= 0x001FF474) && !m_FontsLoaded)
          {
            if (position >= 0x001FCF00)
            {
              PanicAlertT("Error: Trying to access Windows-1252 fonts but they are not loaded. "
                          "Games may not show fonts correctly, or crash.");
            }
            else
            {
              PanicAlertT("Error: Trying to access Shift JIS fonts but they are not loaded. "
                          "Games may not show fonts correctly, or crash.");
            }
            m_FontsLoaded = true;  // Don't be a nag :p
          }
        }
      }
      else
      {
        NOTICE_LOG(OSREPORT, "EXI IPL-DEV: %s %x at %08x", IsWriteCommand() ? "write" : "read",
                   _uByte, m_uAddress);
      }
      break;
    }

    m_uRWOffset++;
  }

  m_uPosition++;
}

u32 CEXIIPL::GetEmulatedTime(u32 epoch)
{
  u64 ltime = 0;

  if (Movie::IsMovieActive())
  {
    ltime = Movie::GetRecordingStartTime();

    // let's keep time moving forward, regardless of what it starts at
    ltime += CoreTiming::GetTicks() / SystemTimers::GetTicksPerSecond();
  }
  else if (NetPlay::IsNetPlayRunning())
  {
    ltime = NetPlay_GetEmulatedTime();

    // let's keep time moving forward, regardless of what it starts at
    ltime += CoreTiming::GetTicks() / SystemTimers::GetTicksPerSecond();
  }
  else
  {
    _assert_(!Core::WantsDeterminism());
    ltime = Common::Timer::GetLocalTimeSinceJan1970() - SystemTimers::GetLocalTimeRTCOffset();
  }

  return static_cast<u32>(ltime) - epoch;
}
}  // namespace ExpansionInterface
