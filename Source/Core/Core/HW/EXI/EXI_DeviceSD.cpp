// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceSD.h"

#include <algorithm>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SDCardUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/System.h"

#ifdef _WIN32
#include <windows.h>

#include <winioctl.h>
#endif

namespace ExpansionInterface
{
namespace
{
// SPI-mode R1 flag bits
constexpr u8 R1_IN_IDLE_STATE = 0x01;
constexpr u8 R1_ILLEGAL_COMMAND = 0x04;
constexpr u8 R1_PARAMETER_ERROR = 0x40;

constexpr u8 START_BLOCK_TOKEN = 0xFE;
constexpr u8 MULTI_WRITE_TOKEN = 0xFC;
constexpr u8 MULTI_WRITE_STOP_TOKEN = 0xFD;
constexpr u8 DATA_RESPONSE_ACCEPTED = 0x05;
constexpr u8 DATA_RESPONSE_WRITE_ERROR = 0x0D;
constexpr u8 DATA_ERROR_OUT_OF_RANGE = 0x08;
constexpr u8 DATA_ERROR_CC_ERROR = 0x02;

constexpr u32 BLOCK_SIZE = 512;

// Cards at or above this capacity are reported as SDHC (CCS set, CSD v2, block addressing).
// Real SDHC starts past 2 GiB, but CSD v1 with 512-byte blocks tops out at 1 GiB, and every
// driver derives the addressing mode from the CCS bit rather than from the capacity.
constexpr u64 SDHC_THRESHOLD = 0x40000000;  // 1 GiB

u8 CRC7(const u8* data, size_t size)
{
  u8 crc = 0;
  for (size_t i = 0; i < size; ++i)
  {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 0x80) ? static_cast<u8>((crc << 1) ^ 0x12) : static_cast<u8>(crc << 1);
  }
  return static_cast<u8>(crc >> 1);
}

u16 CRC16(const u8* data, size_t size)
{
  u16 crc = 0;
  for (size_t i = 0; i < size; ++i)
  {
    crc ^= static_cast<u16>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? static_cast<u16>((crc << 1) ^ 0x1021) : static_cast<u16>(crc << 1);
  }
  return crc;
}
}  // namespace

namespace
{
bool IsRawVolumePath(const std::string& path)
{
  return path.starts_with("\\\\.\\");
}
}  // namespace

CEXISDCard::CEXISDCard(Core::System& system) : IEXIDevice(system)
{
  m_allow_writes = Config::Get(Config::MAIN_GC_SD_CARD_ALLOW_WRITES);
  OpenStorage(Config::GetGCSDCardImagePath());
  if (StorageIsOpen())
  {
    m_sdhc = m_size >= SDHC_THRESHOLD;
    INFO_LOG_FMT(EXPANSIONINTERFACE, "GC SD card: {} MiB, {}{}", m_size / 1024 / 1024,
                 m_sdhc ? "SDHC" : "SDSC", m_storage_read_only ? ", read-only" : "");
  }
}

CEXISDCard::~CEXISDCard()
{
  CloseStorage();
}

void CEXISDCard::OpenStorage(const std::string& path)
{
  if (IsRawVolumePath(path))
  {
#ifdef _WIN32
    // Serve sectors straight from the physical card (e.g. \\.\G:) — no image, no packing,
    // and the guest sees the card's real filesystem (exFAT included). Read-only: writing
    // raw sectors under a mounted Windows volume would corrupt it.
    HANDLE volume = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (volume == INVALID_HANDLE_VALUE)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE,
                    "Could not open raw volume {} (error {}). If access is denied, run Dolphin "
                    "as administrator - Windows gates raw volume reads.",
                    path, GetLastError());
      return;
    }
    GET_LENGTH_INFORMATION length{};
    DWORD returned = 0;
    if (!DeviceIoControl(volume, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
                         &returned, nullptr))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Could not query size of raw volume {} (error {})", path,
                    GetLastError());
      CloseHandle(volume);
      return;
    }
    m_volume = volume;
    m_size = static_cast<u64>(length.Length.QuadPart);
    m_storage_read_only = true;
    INFO_LOG_FMT(EXPANSIONINTERFACE, "GC SD card backed by raw volume {}", path);
#else
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Raw volume paths ({}) are only supported on Windows",
                  path);
#endif
    return;
  }

  if (!File::Exists(path))
  {
    INFO_LOG_FMT(EXPANSIONINTERFACE, "GC SD image not found, creating a 128 MB card at {}", path);
    if (!Common::SDCardCreate(128, path))
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Could not create GC SD image at {}", path);
  }

  if (m_image.Open(path, m_allow_writes ? "r+b" : "rb"))
  {
    m_size = m_image.GetSize();
    m_storage_read_only = !m_allow_writes;
  }
  else
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Could not open GC SD image at {} - no card present", path);
  }
}

void CEXISDCard::CloseStorage()
{
#ifdef _WIN32
  if (m_volume)
  {
    CloseHandle(static_cast<HANDLE>(m_volume));
    m_volume = nullptr;
  }
#endif
  if (m_image.IsOpen())
    m_image.Close();
}

bool CEXISDCard::StorageIsOpen() const
{
#ifdef _WIN32
  if (m_volume)
    return true;
#endif
  return m_image.IsOpen();
}

bool CEXISDCard::StorageRead(u64 offset, u8* dst, u32 size)
{
#ifdef _WIN32
  if (m_volume)
  {
    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(offset);
    DWORD read = 0;
    return SetFilePointerEx(static_cast<HANDLE>(m_volume), position, nullptr, FILE_BEGIN) &&
           ReadFile(static_cast<HANDLE>(m_volume), dst, size, &read, nullptr) && read == size;
  }
#endif
  m_image.Seek(offset, File::SeekOrigin::Begin);
  if (!m_image.ReadBytes(dst, size))
  {
    m_image.ClearError();
    return false;
  }
  return true;
}

bool CEXISDCard::StorageWrite(u64 offset, const u8* src, u32 size)
{
  if (m_storage_read_only)
    return false;
#ifdef _WIN32
  if (m_volume)
    return false;
#endif
  m_image.Seek(offset, File::SeekOrigin::Begin);
  if (!m_image.WriteBytes(src, size))
  {
    m_image.ClearError();
    return false;
  }
  return true;
}

bool CEXISDCard::IsPresent() const
{
  return StorageIsOpen();
}

void CEXISDCard::SetCS(int cs)
{
  // libogc's sdgecko sends the command, deselects, then reselects to clock the response out
  // (__card_readresponse) - pending response bytes must survive CS changes, as they do on a
  // real card. Only a half-received command frame is abandoned when a new selection starts.
  if (cs)
    m_cmd_pos = 0;
}

void CEXISDCard::TransferByte(u8& byte)
{
  const u8 in = byte;

  if (m_response_pos < m_response.size())
  {
    byte = m_response[m_response_pos++];
    if (m_response_pos == m_response.size())
    {
      m_response.clear();
      m_response_pos = 0;
      if (m_multi_read)
        QueueNextMultiBlock();
    }
  }
  else
  {
    byte = 0xFF;
  }

  ProcessInputByte(in);
}

void CEXISDCard::ProcessInputByte(u8 in)
{
  if (m_write_state == WriteState::WaitToken)
  {
    if (in == START_BLOCK_TOKEN || (m_multi_write && in == MULTI_WRITE_TOKEN))
    {
      m_write_state = WriteState::Data;
      m_write_pos = 0;
    }
    else if (m_multi_write && in == MULTI_WRITE_STOP_TOKEN)
    {
      m_write_state = WriteState::None;
      m_multi_write = false;
      // One skipped byte, then busy, then ready.
      m_response.assign({0xFF, 0x00, 0xFF});
      m_response_pos = 0;
    }
    // Anything else (0xFF gaps) keeps waiting for a token.
    return;
  }

  if (m_write_state == WriteState::Data)
  {
    m_write_buffer[m_write_pos++] = in;
    if (m_write_pos < m_write_buffer.size())
      return;

    // Full block plus CRC received (the CRC is not verified, like most cards in SPI mode).
    u8 data_response = DATA_RESPONSE_ACCEPTED;
    if (!m_allow_writes || m_write_address + BLOCK_SIZE > m_size ||
        !StorageWrite(m_write_address, m_write_buffer.data(), BLOCK_SIZE))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "GC SD write rejected at {:#x}", m_write_address);
      data_response = DATA_RESPONSE_WRITE_ERROR;
    }
    m_write_address += BLOCK_SIZE;

    // Data response, one busy byte, ready.
    m_response.assign({data_response, 0x00, 0xFF});
    m_response_pos = 0;
    m_write_state = m_multi_write ? WriteState::WaitToken : WriteState::None;
    return;
  }

  if (m_cmd_pos == 0)
  {
    // A command frame starts with 0b01xxxxxx; everything else on the wire between frames
    // (0xFF idle clocks, the 0x00s an EXI immediate read shifts in) is ignored.
    if ((in & 0xC0) != 0x40)
      return;
  }

  m_cmd[m_cmd_pos++] = in;
  if (m_cmd_pos == m_cmd.size())
  {
    m_cmd_pos = 0;
    ExecuteCommand();
  }
}

u8 CEXISDCard::R1() const
{
  return m_idle_state ? R1_IN_IDLE_STATE : 0x00;
}

void CEXISDCard::QueueR1()
{
  QueueR1(R1());
}

void CEXISDCard::QueueR1(u8 r1)
{
  // One dead byte (Ncr) before the response, as on a real card.
  m_response.assign({0xFF, r1});
  m_response_pos = 0;
}

void CEXISDCard::QueueDataBlock(const u8* data, size_t size)
{
  const u16 crc = CRC16(data, size);
  m_response.push_back(0xFF);
  m_response.push_back(START_BLOCK_TOKEN);
  m_response.insert(m_response.end(), data, data + size);
  m_response.push_back(static_cast<u8>(crc >> 8));
  m_response.push_back(static_cast<u8>(crc));
}

void CEXISDCard::QueueDataErrorToken(u8 token)
{
  m_response.push_back(0xFF);
  m_response.push_back(token);
}

bool CEXISDCard::QueueReadBlock(u64 byte_address)
{
  if (!StorageIsOpen() || byte_address + BLOCK_SIZE > m_size)
  {
    QueueDataErrorToken(DATA_ERROR_OUT_OF_RANGE);
    return false;
  }

  std::array<u8, BLOCK_SIZE> buffer;
  if (!StorageRead(byte_address, buffer.data(), static_cast<u32>(buffer.size())))
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "GC SD read failed at {:#x}", byte_address);
    QueueDataErrorToken(DATA_ERROR_CC_ERROR);
    return false;
  }

  QueueDataBlock(buffer.data(), buffer.size());
  return true;
}

void CEXISDCard::QueueNextMultiBlock()
{
  if (!QueueReadBlock(m_read_address))
    m_multi_read = false;
  else
    m_read_address += BLOCK_SIZE;
}

u64 CEXISDCard::ByteAddress(u32 arg) const
{
  return m_sdhc ? static_cast<u64>(arg) * BLOCK_SIZE : static_cast<u64>(arg);
}

void CEXISDCard::ExecuteCommand()
{
  const u8 command = m_cmd[0] & 0x3F;
  const u32 arg = (static_cast<u32>(m_cmd[1]) << 24) | (static_cast<u32>(m_cmd[2]) << 16) |
                  (static_cast<u32>(m_cmd[3]) << 8) | m_cmd[4];

  // INFO on purpose: release builds compile out DEBUG, and this trace is the only window
  // into the guest driver's init sequence when bring-up fails.
  INFO_LOG_FMT(EXPANSIONINTERFACE, "GC SD {}CMD{} arg={:08x}", m_app_cmd ? "A" : "", command,
               arg);

  if (m_app_cmd)
  {
    m_app_cmd = false;
    ExecuteAppCommand(command, arg);
    return;
  }

  switch (command)
  {
  case 0:  // GO_IDLE_STATE
    m_idle_state = true;
    m_multi_read = false;
    m_write_state = WriteState::None;
    m_multi_write = false;
    QueueR1();
    break;
  case 8:  // SEND_IF_COND (R7): voltage accepted + check pattern echo
    QueueR1();
    m_response.push_back(0x00);
    m_response.push_back(0x00);
    m_response.push_back(0x01);
    m_response.push_back(static_cast<u8>(arg & 0xFF));
    break;
  case 9:  // SEND_CSD
  {
    QueueR1(0x00);
    const auto csd = MakeCSD();
    QueueDataBlock(csd.data(), csd.size());
    break;
  }
  case 10:  // SEND_CID
  {
    QueueR1(0x00);
    const auto cid = MakeCID();
    QueueDataBlock(cid.data(), cid.size());
    break;
  }
  case 6:  // SWITCH_FUNC: report "no switchable functions"; hosts then stay at default speed
  {
    QueueR1(0x00);
    std::array<u8, 64> status{};
    QueueDataBlock(status.data(), status.size());
    break;
  }
  case 12:  // STOP_TRANSMISSION: stuff byte, R1, short busy
    m_multi_read = false;
    m_response.assign({0xFF, 0xFF, R1(), 0x00, 0xFF});
    m_response_pos = 0;
    break;
  case 13:  // SEND_STATUS (R2)
    QueueR1();
    m_response.push_back(0x00);
    break;
  case 16:  // SET_BLOCKLEN
    QueueR1(arg == BLOCK_SIZE ? R1() : static_cast<u8>(R1() | R1_PARAMETER_ERROR));
    break;
  case 17:  // READ_SINGLE_BLOCK
    QueueR1(0x00);
    QueueReadBlock(ByteAddress(arg));
    break;
  case 18:  // READ_MULTIPLE_BLOCK
    QueueR1(0x00);
    m_read_address = ByteAddress(arg);
    m_multi_read = true;
    QueueNextMultiBlock();
    break;
  case 24:  // WRITE_BLOCK
    QueueR1(0x00);
    m_write_address = ByteAddress(arg);
    m_multi_write = false;
    m_write_state = WriteState::WaitToken;
    break;
  case 25:  // WRITE_MULTIPLE_BLOCK
    QueueR1(0x00);
    m_write_address = ByteAddress(arg);
    m_multi_write = true;
    m_write_state = WriteState::WaitToken;
    break;
  case 55:  // APP_CMD
    m_app_cmd = true;
    QueueR1();
    break;
  case 58:  // READ_OCR (R3): powered up, 3.2-3.4V, CCS from card type
    QueueR1();
    m_response.push_back(m_sdhc ? 0xC0 : 0x80);
    m_response.push_back(0xFF);
    m_response.push_back(0x80);
    m_response.push_back(0x00);
    break;
  case 59:  // CRC_ON_OFF: CRCs are always generated here, so just acknowledge
    QueueR1();
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "GC SD unimplemented CMD{} arg={:08x}", command, arg);
    QueueR1(static_cast<u8>(R1() | R1_ILLEGAL_COMMAND));
    break;
  }
}

void CEXISDCard::ExecuteAppCommand(u8 command, u32 arg)
{
  switch (command)
  {
  case 41:  // SD_SEND_OP_COND: leave idle immediately
    m_idle_state = false;
    QueueR1();
    break;
  case 13:  // SD_STATUS (R2 + 64 data bytes)
  {
    QueueR1();
    m_response.push_back(0x00);
    std::array<u8, 64> status{};
    QueueDataBlock(status.data(), status.size());
    break;
  }
  case 51:  // SEND_SCR: SD spec v2, 1-bit and 4-bit bus widths
  {
    QueueR1(0x00);
    const std::array<u8, 8> scr = {0x02, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    QueueDataBlock(scr.data(), scr.size());
    break;
  }
  case 42:  // SET_CLR_CARD_DETECT
    QueueR1();
    break;
  default:
    WARN_LOG_FMT(EXPANSIONINTERFACE, "GC SD unimplemented ACMD{} arg={:08x}", command, arg);
    QueueR1(static_cast<u8>(R1() | R1_ILLEGAL_COMMAND));
    break;
  }
}

std::array<u8, 16> CEXISDCard::MakeCSD() const
{
  std::array<u8, 16> csd{};
  if (m_sdhc)
  {
    // CSD v2: capacity in 512 KiB units
    const u32 c_size = static_cast<u32>(std::max<u64>(m_size / (512 * 1024), 1) - 1);
    csd = {0x40, 0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, static_cast<u8>((c_size >> 16) & 0x3F),
           static_cast<u8>(c_size >> 8), static_cast<u8>(c_size), 0x7F, 0x80, 0x0A, 0x40,
           0x00, 0x00};
  }
  else
  {
    // CSD v1 with READ_BL_LEN 9 / C_SIZE_MULT 7: capacity = (C_SIZE + 1) * 256 KiB
    const u32 c_size =
        static_cast<u32>(std::clamp<u64>(m_size / (256 * 1024), 1, 4096) - 1);
    csd = {0x00, 0x26, 0x00, 0x32, 0x5B, 0x59, static_cast<u8>(0x80 | ((c_size >> 10) & 0x3)),
           static_cast<u8>(c_size >> 2), static_cast<u8>(((c_size & 0x3) << 6) | 0x3F), 0xFF,
           0xFF, 0x80, 0x0A, 0x40, 0x00, 0x00};
  }
  csd[15] = static_cast<u8>((CRC7(csd.data(), 15) << 1) | 1);
  return csd;
}

std::array<u8, 16> CEXISDCard::MakeCID() const
{
  // Manufacturer/product strings are arbitrary; nothing derives behavior from them.
  std::array<u8, 16> cid = {0x03, 'S',  'D',  'D',  'O',  'L',  'P',  'H',
                            'N',  0x10, 0x00, 0x00, 0xD0, 0x17, 0x01, 0x00};
  cid[15] = static_cast<u8>((CRC7(cid.data(), 15) << 1) | 1);
  return cid;
}

void CEXISDCard::DoState(PointerWrap& p)
{
  p.Do(m_size);
  p.Do(m_sdhc);
  p.Do(m_idle_state);
  p.Do(m_app_cmd);
  p.Do(m_cmd);
  p.Do(m_cmd_pos);
  p.Do(m_response);
  p.Do(m_response_pos);
  p.Do(m_multi_read);
  p.Do(m_read_address);
  p.Do(m_write_state);
  p.Do(m_multi_write);
  p.Do(m_write_address);
  p.Do(m_write_buffer);
  p.Do(m_write_pos);
}
}  // namespace ExpansionInterface
