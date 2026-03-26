// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include <algorithm>
#include <numeric>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/AMMediaboard.h"
#include "Core/HW/MagCard/C1231BR.h"
#include "Core/HW/MagCard/C1231LR.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/Triforce/DeckReader.h"
#include "Core/HW/Triforce/FZeroAX.h"
#include "Core/HW/Triforce/ICCardReader.h"
#include "Core/HW/Triforce/JVSIO.h"
#include "Core/HW/Triforce/MarioKartGP.h"
#include "Core/HW/Triforce/Touchscreen.h"
#include "Core/Movie.h"
#include "Core/System.h"

namespace
{
enum class GCAMCommand : u8
{
  StatusSwitches = 0x10,
  SerialNumber = 0x11,
  Unknown_12 = 0x12,
  Unknown_14 = 0x14,
  FirmVersion = 0x15,
  FPGAVersion = 0x16,
  RegionSettings = 0x1F,

  Unknown_21 = 0x21,
  Unknown_22 = 0x22,
  Unknown_23 = 0x23,
  Unknown_24 = 0x24,

  SerialA = 0x31,
  SerialB = 0x32,

  JVSIOA = 0x40,
  JVSIOB = 0x41,

  Unknown_60 = 0x60,
};

// This value prevents F-Zero AX mag card breakage.
// It's now used for serial port reads in general.
// TODO: Verify how the hardware actually works.
constexpr u32 SERIAL_PORT_MAX_READ_SIZE = 0x1f;

}  // namespace

namespace SerialInterface
{

const constexpr u8 s_region_flags[] = "\x00\x00\x30\x00"
                                      //   "\x01\xfe\x00\x00"  // JAPAN
                                      "\x02\xfd\x00\x00"  // USA
                                      //"\x03\xfc\x00\x00"  // export
                                      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
// AM-Baseboard device on SI
CSIDevice_AMBaseboard::CSIDevice_AMBaseboard(Core::System& system, SIDevices device,
                                             int device_number)
    : ISIDevice(system, device, device_number)
{
  // Magnetic Card Reader
  m_mag_card_settings.card_path = File::GetUserPath(D_TRIUSER_IDX);
  m_mag_card_settings.card_name = fmt::format("tricard_{}.bin", SConfig::GetInstance().GetGameID());

  m_io_ports.AddIOAdapter(std::make_unique<Triforce::Common_IOAdapter>(m_system));

  switch (AMMediaboard::GetGameType())
  {
    using namespace AMMediaboard;

  case FZeroAX:
  {
    // This board enables DX mode on AX machines.
    // All this does is enable the chair motion.
    m_jvs_io_board = std::make_unique<Triforce::Sega_837_13844_JVSIOBoard>(&m_io_ports);
    auto slot_a = std::make_unique<Triforce::FZeroAXSteeringWheel>();
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::FZeroAXCommon_IOAdapter>(slot_a.get()));
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::FZeroAXDeluxe_IOAdapter>());
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::make_unique<MagCard::C1231BR>(&m_mag_card_settings);
    break;
  }
  case FZeroAXMonster:
  {
    auto slot_a = std::make_unique<Triforce::FZeroAXSteeringWheel>();
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::FZeroAXCommon_IOAdapter>(slot_a.get()));
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::FZeroAXMonster_IOAdapter>());
    m_serial_device_a = std::move(slot_a);
    break;
  }
  case MarioKartGP:
  case MarioKartGP2:
  {
    m_jvs_io_board = std::make_unique<Triforce::Namco_FCA_JVSIOBoard>(&m_io_ports);
    auto slot_a = std::make_unique<Triforce::MarioKartGPSteeringWheel>();
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::MarioKartGPCommon_IOAdapter>(slot_a.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::make_unique<MagCard::C1231LR>(&m_mag_card_settings);
    break;
  }
  case VirtuaStriker3:
  {
    m_io_ports.AddIOAdapter(std::make_unique<Triforce::VirtuaStriker3_IOAdapter>());
    break;
  }
  case VirtuaStriker4:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4Common_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);
    break;
  }
  case VirtuaStriker4_2006:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4Common_IOAdapter>(slot_a.get(), slot_b.get()));
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::VirtuaStriker4_2006_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);
    break;
  }
  case GekitouProYakyuu:
  {
    auto slot_a = std::make_unique<Triforce::ICCardReader>(0);
    auto slot_b = std::make_unique<Triforce::ICCardReader>(1);
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::GekitouProYakyuu_IOAdapter>(slot_a.get(), slot_b.get()));
    m_serial_device_a = std::move(slot_a);
    m_serial_device_b = std::move(slot_b);
    break;
  }
  case KeyOfAvalon:
  {
    auto deck_reader = std::make_unique<Triforce::DeckReader>();
    m_io_ports.AddIOAdapter(
        std::make_unique<Triforce::KeyOfAvalon_IOAdapter>(deck_reader->GetICCardReader()));
    m_serial_device_a = std::move(deck_reader);
    m_serial_device_b = std::make_unique<Triforce::Touchscreen>();
    break;
  }
  default:
    break;
  }

  // Fallback to a Sega board.
  if (m_jvs_io_board == nullptr)
    m_jvs_io_board = std::make_unique<Triforce::Sega_837_13551_JVSIOBoard>(&m_io_ports);
}

CSIDevice_AMBaseboard::~CSIDevice_AMBaseboard() = default;

int CSIDevice_AMBaseboard::RunBuffer(u8* buffer, int request_length)
{
  // Debug logging
  ISIDevice::RunBuffer(buffer, request_length);

  // Update user input.
  m_io_ports.Update();

  const auto bb_command = EBufferCommands(buffer[0]);
  switch (bb_command)
  {
  case EBufferCommands::CMD_STATUS:  // Returns ID and dip switches
  {
    return CreateStatusResponse(SI_AM_BASEBOARD | 0x100, buffer);
  }
  break;
  case EBufferCommands::CMD_AM_BASEBOARD:
  {
    // FYI: Supplied `buffer` always points to 128 bytes.
    // TODO: Change the `RunBuffer` interface to make that more obvious.
    const int command_length = buffer[1];

    constexpr int command_header_size = 2;
    if (command_header_size + command_length > request_length)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Bad command_length: {} for request_length: {}",
                    command_length, request_length);
      return -1;
    }

    auto& data_out = m_response_buffers[m_current_response_buffer_index];

    // Need room for the checksum.
    const auto usable_out_size = data_out.size() - 1;

    constexpr int response_header_size = 2;
    u32 data_offset = response_header_size;

    u8* data_in = buffer + command_header_size;
    u8* const data_in_end = data_in + command_length;

    // Helper to check that iterating over data n times is safe,
    // i.e. *data++ at most lead to data.end()
    auto validate_data_in_out = [&](u32 n_in, u32 n_out, std::string_view command) -> bool {
      if (data_in + n_in > data_in_end)
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: data_in overflow in {}", command);
      else if (u64{data_offset} + n_out > usable_out_size)
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: data_out overflow in {}", command);
      else
        return true;
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                    "Overflow details:\n"
                    " - data_in(begin={}, current={}, end={}, n_in={})\n"
                    " - data_out(offset={}, size={}, n_out={})",
                    fmt::ptr(buffer + 2), fmt::ptr(data_in), fmt::ptr(data_in_end), n_in,
                    data_offset, data_out.size(), n_out);
      data_in = data_in_end;
      return false;
    };

    while (data_in < data_in_end)
    {
      const u32 gcam_command = *data_in++;
      switch (GCAMCommand(gcam_command))
      {
      case GCAMCommand::StatusSwitches:
      {
        const auto status_switches = m_io_ports.GetStatusSwitches();
        const auto out_length = u8(status_switches.size());

        if (!validate_data_in_out(1, 2 + out_length, "StatusSwitches"))
          break;

        const u8 status = *data_in++;
        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x10, {:02x} (READ STATUS&SWITCHES)",
                      status);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = out_length;

        std::ranges::copy(status_switches, data_out.data() + data_offset);

        data_offset += out_length;
        break;
      }
      case GCAMCommand::SerialNumber:
      {
        if (!validate_data_in_out(1, 18, "SerialNumber"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x11, {:02x} (READ SERIAL NR)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 16;
        memcpy(data_out.data() + data_offset, "AADE-01B98394904", 16);

        data_offset += 16;
        break;
      }
      case GCAMCommand::Unknown_12:
        if (!validate_data_in_out(2, 2, "Unknown_12"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x12, {:02x} {:02x}", data_in[0],
                       data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::Unknown_14:
        if (!validate_data_in_out(2, 2, "Unknown_14"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x14, {:02x} {:02x}", data_in[0],
                       data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::FirmVersion:
        if (!validate_data_in_out(1, 4, "FirmVersion"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x15, {:02x} (READ FIRM VERSION)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x02;
        // 00.26
        data_out[data_offset++] = 0x00;
        data_out[data_offset++] = 0x26;
        break;
      case GCAMCommand::FPGAVersion:
        if (!validate_data_in_out(1, 4, "FPGAVersion"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x16, {:02x} (READ FPGA VERSION)",
                       *data_in);
        data_in++;

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x02;
        // 07.06
        data_out[data_offset++] = 0x07;
        data_out[data_offset++] = 0x06;
        break;
      case GCAMCommand::RegionSettings:
      {
        if (!validate_data_in_out(5, 0x16, "RegionSettings"))
          break;

        // Used by SegaBoot for region checks (dev mode skips this check)
        // In some games this also controls the displayed language
        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB,
                       "GC-AM: Command 0x1F, {:02x} {:02x} {:02x} {:02x} {:02x} (REGION)",
                       data_in[0], data_in[1], data_in[2], data_in[3], data_in[4]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x14;

        for (int i = 0; i < 0x14; ++i)
          data_out[data_offset++] = s_region_flags[i];

        data_in += 5;
      }
      break;
      // No reply
      // Note: Always sends three bytes even though size is set to two
      case GCAMCommand::Unknown_21:
      {
        if (!validate_data_in_out(4, 0, "Unknown_21"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x21, {:02x}, {:02x}, {:02x}, {:02x}",
                      data_in[0], data_in[1], data_in[2], data_in[3]);
        data_in += 4;
      }
      break;
      // No reply
      // Note: Always sends six bytes
      case GCAMCommand::Unknown_22:
      {
        if (!validate_data_in_out(7, 0, "Unknown_22"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB,
                      "GC-AM: Command 0x22, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}, {:02x}",
                      data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5],
                      data_in[6]);

        const u32 in_size = data_in[0] + 1;
        if (!validate_data_in_out(in_size, 0, "Unknown_22"))
          break;
        data_in += in_size;
      }
      break;
      case GCAMCommand::Unknown_23:
        if (!validate_data_in_out(2, 2, "Unknown_23"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x23, {:02x} {:02x}", data_in[0],
                      data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::Unknown_24:
        if (!validate_data_in_out(2, 2, "Unknown_24"))
          break;

        DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x24, {:02x} {:02x}", data_in[0],
                      data_in[1]);

        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = 0x00;

        data_in += 2;
        break;
      case GCAMCommand::SerialA:
      {
        if (!validate_data_in_out(1, 0, "SerialA"))
          break;
        const u32 in_length = *data_in++;

        if (!validate_data_in_out(in_length, 0, "SerialA"))
          break;

        if (m_serial_device_a != nullptr)
        {
          m_serial_device_a->WriteRxBytes({data_in, in_length});
        }

        data_in += in_length;
        break;
      }
      case GCAMCommand::SerialB:
      {
        if (!validate_data_in_out(1, 0, "SerialB"))
          break;
        const u32 in_length = *data_in++;

        if (!validate_data_in_out(in_length, 0, "SerialB"))
          break;

        if (m_serial_device_b != nullptr)
        {
          m_serial_device_b->WriteRxBytes({data_in, in_length});
        }

        data_in += in_length;
        break;
      }
      case GCAMCommand::JVSIOA:
      case GCAMCommand::JVSIOB:
      {
        if (!validate_data_in_out(1, 0, "JVSIO"))
          break;
        const u32 in_length = *data_in++;

        if (!validate_data_in_out(in_length, 0, "JVSIO"))
          break;

        m_jvs_io_board->WriteRxBytes({data_in, in_length});
        data_in += in_length;

        m_jvs_io_board->Update();

        const auto out_length = u32(m_jvs_io_board->GetTxByteCount());

        if (out_length == 0)
          break;

        // Also accounting for the 2-byte header.
        if (!validate_data_in_out(0, out_length + 2, "SerialDevice"))
          break;

        // Write the 2-byte header.
        data_out[data_offset++] = gcam_command;
        data_out[data_offset++] = u8(out_length);

        const auto out_span = std::span{data_out}.subspan(data_offset, out_length);

        m_jvs_io_board->TakeTxBytes(out_span);

        data_offset += out_length;
        break;
      }
      case GCAMCommand::Unknown_60:
      {
        if (!validate_data_in_out(3, 0, "Unknown_60"))
          break;

        NOTICE_LOG_FMT(SERIALINTERFACE_AMBB, "GC-AM: Command 0x60, {:02x} {:02x} {:02x}",
                       data_in[0], data_in[1], data_in[2]);

        const u32 in_size = data_in[0] + 1;
        if (!validate_data_in_out(in_size, 0, "Unknown_60"))
          break;
        data_in += in_size;
      }
      break;
      default:
        if (!validate_data_in_out(5, 0, fmt::format("Unknown_{}", gcam_command)))
          break;
        ERROR_LOG_FMT(SERIALINTERFACE_AMBB,
                      "GC-AM: Command {:02x} (unknown) {:02x} {:02x} {:02x} {:02x} {:02x}",
                      gcam_command, data_in[0], data_in[1], data_in[2], data_in[3], data_in[4]);
        break;
      }
    }

    // Update attached serial devices and read data into our response buffer.
    // This is done regardless of the game having just sent a SerialA/B write.

    const auto process_serial_device = [&](Triforce::SerialDevice* serial_device, GCAMCommand cmd) {
      if (serial_device == nullptr)
        return;

      serial_device->Update();

      const auto out_length =
          std::min(u32(serial_device->GetTxByteCount()), SERIAL_PORT_MAX_READ_SIZE);

      if (out_length == 0)
        return;

      // Also accounting for the 2-byte header.
      if (!validate_data_in_out(0, out_length + 2, "SerialDevice"))
        return;

      // Write the 2-byte header.
      data_out[data_offset++] = u8(cmd);
      data_out[data_offset++] = u8(out_length);

      const auto out_span = std::span{data_out}.subspan(data_offset, out_length);

      serial_device->TakeTxBytes(out_span);

      data_offset += out_length;
    };

    process_serial_device(m_serial_device_a.get(), GCAMCommand::SerialA);
    process_serial_device(m_serial_device_b.get(), GCAMCommand::SerialB);

    data_out[0] = 0x01;                                // Status code ?
    data_out[1] = data_offset - response_header_size;  // Length

    // Zero-fill the rest of the buffer.
    std::fill(data_out.data() + data_offset, data_out.data() + usable_out_size, 0x00);

    // Compute the checksum (bitwise-xor of additive sum).
    const u8 checksum = ~std::accumulate(data_out.data(), data_out.data() + data_offset, u8{});
    data_out.back() = checksum;

    DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "Command response: {}",
                  HexDump(data_out.data(), data_out.size()));

    // Switch to next buffer and return it.
    if (++m_current_response_buffer_index == m_response_buffers.size())
      m_current_response_buffer_index = 0;

    auto& response_buffer = m_response_buffers[m_current_response_buffer_index];

    const bool buffer_has_data = response_buffer.front() != 0x00;
    if (!buffer_has_data)
      return 0;

    std::ranges::copy(response_buffer, buffer);
    return int(response_buffer.size());
  }
  default:
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command (0x{:08x})", (u32)bb_command);
    PanicAlertFmt("SI: Unknown command");
    break;
  }
  }

  // No response.
  return -1;
}

DataResponse CSIDevice_AMBaseboard::GetData(u32&, u32&)
{
  return DataResponse::NoData;
}

void CSIDevice_AMBaseboard::SendCommand(u32 command, u8 poll)
{
  WARN_LOG_FMT(SERIALINTERFACE_AMBB, "Unhandled SendCommand(0x{:08x}, 0x{:02x})", command, poll);
}

void CSIDevice_AMBaseboard::DoState(PointerWrap& p)
{
  p.Do(m_response_buffers);
  p.Do(m_current_response_buffer_index);

  m_io_ports.DoState(p);

  m_jvs_io_board->DoState(p);

  // Serial A
  if (m_serial_device_a != nullptr)
    m_serial_device_a->DoState(p);

  // Serial B
  if (m_serial_device_b != nullptr)
    m_serial_device_b->DoState(p);
}

}  // namespace SerialInterface
