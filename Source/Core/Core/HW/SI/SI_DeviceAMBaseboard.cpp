// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include <algorithm>
#include <numeric>
#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

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

  // Note: [0x40, 0x5f] are JVSIO requests.

  Unknown_60 = 0x60,
};

// Multiple JVSIO command IDs presumably to correlate responses with multiple concurrent requests.
constexpr u8 GCAM_CMD_JVSIO = 0x40;
constexpr u8 GCAM_CMD_JVSIO_MASK = 0xe0;

// This value prevents F-Zero AX mag card breakage.
// It's now used for serial port reads in general.
// TODO: Verify how the hardware actually works.
constexpr std::size_t SERIAL_PORT_MAX_READ_SIZE = 0x1f;

constexpr std::string_view REGION_FLAGS = "\x00\x00\x30\x00"
                                          // "\x01\xfe\x00\x00"  // JAPAN
                                          "\x02\xfd\x00\x00"  // USA
                                          // "\x03\xfc\x00\x00"  // Export
                                          "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

}  // namespace

namespace SerialInterface
{

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

  const auto bb_command = EBufferCommands(buffer[0]);
  switch (bb_command)
  {
  case EBufferCommands::CMD_STATUS:  // Returns ID and dip switches
  {
    return CreateStatusResponse(SI_AM_BASEBOARD | 0x100, buffer);
  }
  case EBufferCommands::CMD_AM_BASEBOARD:
  {
    // FYI: Supplied `buffer` always points to 128 bytes.
    // TODO: Change the `RunBuffer` interface to make that more obvious.
    const int command_length = buffer[1];

    constexpr int header_size = 2;
    if (header_size + command_length > request_length)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Bad command_length: {} for request_length: {}",
                    command_length, request_length);
      break;
    }

    std::array<u8, RESPONSE_SIZE> response_buffer;

    RunCommandBuffer(std::span(buffer + header_size, command_length), response_buffer);

    std::ranges::copy(response_buffer, buffer);
    return RESPONSE_SIZE;
  }
  default:
    ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Unknown command: 0x{:08x}", u8(bb_command));
    break;
  }

  // No response.
  return -1;
}

void CSIDevice_AMBaseboard::RunCommandBuffer(std::span<const u8> input,
                                             std::span<u8, RESPONSE_SIZE> output)
{
  // Throttle just before user input to lower input latency.
  auto& core_timing = m_system.GetCoreTiming();
  core_timing.Throttle(core_timing.GetTicks());

  // Update user input.
  m_io_ports.Update();

  // Need room for the checksum.
  constexpr auto usable_out_size = output.size() - 1;

  constexpr u32 header_size = 2;
  std::size_t out_pos = header_size;

  std::size_t in_pos = 0;
  u8 gcam_command{};

  const auto validate_input = [&](std::size_t byte_count) {
    if (byte_count + in_pos > input.size())
    {
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Buffer underrun (Command: {:02x})", gcam_command);
      in_pos = input.size();
      return false;
    }

    return true;
  };

  const auto get_fixed_input = [&](std::size_t byte_count) -> std::optional<std::span<const u8>> {
    if (!validate_input(byte_count))
      return std::nullopt;

    return input.subspan(std::exchange(in_pos, in_pos + byte_count), byte_count);
  };

  // Reads first byte for the payload length.
  const auto get_variable_input = [&] -> std::optional<std::span<const u8>> {
    if (!validate_input(1))
      return std::nullopt;

    const u8 in_length = input[in_pos++];
    return get_fixed_input(in_length);
  };

  // Also writes a header for the current `gcam_command`.
  const auto prepare_response = [&](std::size_t byte_count) -> std::optional<std::span<u8>> {
    if (out_pos + 2 + byte_count > usable_out_size)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_AMBB, "Buffer overrun (Command: {:02x})", gcam_command);
      in_pos = input.size();
      return std::nullopt;
    }

    // 2-byte response header.
    output[out_pos++] = gcam_command;
    output[out_pos++] = u8(byte_count);

    return output.subspan(std::exchange(out_pos, out_pos + byte_count), byte_count);
  };

  // Process a JVSIO request from the previous frame.
  if (!m_pending_requests.empty())
  {
    const auto& request = m_pending_requests.front();
    m_jvs_io_board->WriteRxBytes(std::span(request).subspan(1));

    m_jvs_io_board->Update();

    const auto out_length = m_jvs_io_board->GetTxByteCount();

    gcam_command = u8(request[0]);
    const auto out_span = prepare_response(out_length);
    if (out_span)
      m_jvs_io_board->TakeTxBytes(*out_span);

    m_pending_requests.pop_front();
  }

  while (in_pos != input.size())
  {
    gcam_command = input[in_pos++];
    DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "Command {:02x}", gcam_command);
    switch (GCAMCommand(gcam_command))
    {
    case GCAMCommand::StatusSwitches:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "StatusSwitches: {:02x}", fmt::join(*payload, " "));

      const auto status_switches = m_io_ports.GetStatusSwitches();

      const auto out_span = prepare_response(status_switches.size());
      if (!out_span)
        break;

      std::ranges::copy(status_switches, out_span->data());
      break;
    }
    case GCAMCommand::SerialNumber:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      INFO_LOG_FMT(SERIALINTERFACE_AMBB, "SerialNumber: {:02x}", fmt::join(*payload, " "));

      constexpr std::string_view serial_number = "AADE-01B98394904";

      const auto out_span = prepare_response(serial_number.size());
      if (!out_span)
        break;

      std::ranges::copy(serial_number, out_span->data());
      break;
    }
    case GCAMCommand::FirmVersion:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      INFO_LOG_FMT(SERIALINTERFACE_AMBB, "FirmVersion: {:02x}", fmt::join(*payload, " "));

      const auto out_span = prepare_response(2);
      if (!out_span)
        break;

      // 00.26
      (*out_span)[0] = 0x00;
      (*out_span)[1] = 0x26;
      break;
    }
    case GCAMCommand::FPGAVersion:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      INFO_LOG_FMT(SERIALINTERFACE_AMBB, "FPGAVersion: {:02x}", fmt::join(*payload, " "));

      const auto out_span = prepare_response(2);
      if (!out_span)
        break;

      // 07.06
      (*out_span)[0] = 0x07;
      (*out_span)[1] = 0x06;
      break;
    }
    case GCAMCommand::RegionSettings:
    {
      // Note: Always 5 bytes.
      const auto payload = get_fixed_input(5);
      if (!payload)
        break;

      // Used by SegaBoot for region checks (dev mode skips this check)
      // In some games this also controls the displayed language
      INFO_LOG_FMT(SERIALINTERFACE_AMBB, "RegionSettings: {:02x}", fmt::join(*payload, " "));

      const auto out_span = prepare_response(REGION_FLAGS.size());
      if (!out_span)
        break;

      std::ranges::copy(REGION_FLAGS, out_span->data());
      break;
    }
    case GCAMCommand::Unknown_21:
    {
      // Note: Always 4 bytes.
      const auto payload = get_fixed_input(4);
      if (!payload)
        break;

      WARN_LOG_FMT(SERIALINTERFACE_AMBB, "Unknown_21: {:02x}", fmt::join(*payload, " "));

      // Note: No reply.
      break;
    }
    case GCAMCommand::SerialA:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      if (m_serial_device_a != nullptr)
      {
        m_serial_device_a->WriteRxBytes(*payload);
      }

      // Note: Reply handled later.
      break;
    }
    case GCAMCommand::SerialB:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      if (m_serial_device_b != nullptr)
      {
        m_serial_device_b->WriteRxBytes(*payload);
      }

      // Note: Reply handled later.
      break;
    }
    default:
    {
      const auto payload = get_variable_input();
      if (!payload)
        break;

      if ((gcam_command & GCAM_CMD_JVSIO_MASK) == GCAM_CMD_JVSIO)
      {
        // We save JVSIO requests for processing next frame.
        // Processing now (and saving the response for later) would induce an input delay.
        auto& request = m_pending_requests.emplace_back(payload->size() + 1);
        request[0] = gcam_command;
        std::ranges::copy(*payload, request.data() + 1);
        break;
      }

      WARN_LOG_FMT(SERIALINTERFACE_AMBB, "Unknown_{:02x}: {:02x}", gcam_command,
                   fmt::join(*payload, " "));
      // Note: No reply.
      break;
    }
    }
  }

  // Update attached serial devices and read data into our response buffer.
  // This is done regardless of the game having just sent a SerialA/B write.

  const auto process_serial_device = [&](Triforce::SerialDevice* serial_device, GCAMCommand cmd) {
    if (serial_device == nullptr)
      return;

    serial_device->Update();

    const auto out_length = std::min(serial_device->GetTxByteCount(), SERIAL_PORT_MAX_READ_SIZE);
    if (out_length == 0)
      return;

    gcam_command = u8(cmd);
    const auto out_span = prepare_response(out_length);
    if (!out_span)
      return;

    serial_device->TakeTxBytes(*out_span);
  };

  process_serial_device(m_serial_device_a.get(), GCAMCommand::SerialA);
  process_serial_device(m_serial_device_b.get(), GCAMCommand::SerialB);

  output[0] = 0x01;
  output[1] = u8(out_pos - header_size);  // Length

  // Zero-fill the rest of the buffer.
  std::fill(output.data() + out_pos, output.data() + usable_out_size, 0x00);

  // Compute the checksum (bitwise-xor of additive sum).
  const u8 checksum = ~std::accumulate(output.data(), output.data() + out_pos, u8{});
  output.back() = checksum;

  DEBUG_LOG_FMT(SERIALINTERFACE_AMBB, "RunCommandBuffer response:\n{}", HexDump(output));
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
  p.Do(m_pending_requests);

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
