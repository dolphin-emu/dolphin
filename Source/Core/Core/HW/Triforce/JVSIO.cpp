// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/JVSIO.h"

#include <numeric>
#include <optional>
#include <vector>

#include <fmt/ranges.h>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"

namespace
{
constexpr u8 JVSIO_SYNC = 0xe0;
constexpr u8 JVSIO_MARK = 0xd0;

constexpr u8 JVSIO_BROADCAST_ADDRESS = 0xff;
constexpr u8 JVSIO_HOST_ADDRESS = 0x00;

enum class JVSIOFeature : u8
{
  SwitchInput = 0x01,          // players, buttons, 0
  CoinInput = 0x02,            // slots, 0, 0
  AnalogInput = 0x03,          // channels, bits, 0
  RotaryInput = 0x04,          // channels, 0, 0
  KeycodeInput = 0x05,         // 0, 0, 0
  ScreenPositionInput = 0x06,  // X-bits, Y-bits, channels
  MiscSwitchInput = 0x07,      // SW-MSB, SW-LSB, 0

  CardSystem = 0x10,            // slots, 0, 0
  MedalHopper = 0x11,           // channels, 0, 0
  GeneralPurposeOutput = 0x12,  // slots, 0, 0
  AnalogOutput = 0x13,          // channels, 0, 0
  CharacterOutput = 0x14,       // width, height, type
  Backup = 0x15,                // 0, 0, 0
};

struct JVSIOFeatureSpec
{
  JVSIOFeature feature{};
  u8 param_a{};
  u8 param_b{};
  u8 param_c{};
};

enum class JVSIOCoinConditionCode : u8
{
  Normal = 0x00,
  CoinJam = 0x01,
  CounterDisconnected = 0x02,
  Busy = 0x03,
};

}  // namespace

namespace Triforce
{

enum class JVSIOStatusCode : u8
{
  Okay = 1,
  UnknownCommand = 2,
  ChecksumError = 3,
  ResponseOverflow = 4,
};

enum class JVSIOReportCode : u8
{
  Okay = 1,
  ParameterSizeError = 2,
  ParameterDataError = 3,
  Busy = 4,
};

enum class JVSIOCommand : u8
{
  IOIdentify = 0x10,
  CommandRevision = 0x11,
  JVSRevision = 0x12,
  CommVersion = 0x13,
  FeatureCheck = 0x14,
  MainID = 0x15,

  SwitchInput = 0x20,
  CoinInput = 0x21,
  AnalogInput = 0x22,
  RotaryInput = 0x23,
  KeycodeInput = 0x24,
  ScreenPositionInput = 0x25,
  MiscSwitchInput = 0x26,

  RemainingPayout = 0x2e,
  DataRetransmit = 0x2f,
  CoinCounterDec = 0x30,
  PayoutCounterInc = 0x31,
  GenericOutput = 0x32,  // Multiple bytes.
  AnalogOutput = 0x33,
  CharacterOutput = 0x34,
  CoinCounterInc = 0x35,
  PayoutCounterDec = 0x36,
  GenericOutputByte = 0x37,  // Single byte.
  GenericOutputBit = 0x38,   // Single bit.

  NamcoCommand = 0x70,

  Reset = 0xf0,
  SetAddress = 0xf1,
  CommMethodChange = 0xf2,
};

class JVSIORequestReader
{
public:
  explicit JVSIORequestReader(std::span<const u8> data)
      : m_data{data.data()}, m_data_end{data.data() + data.size()}
  {
  }

  // Returns a span of all remaining bytes.
  constexpr std::span<const u8> PeekBytes() const { return {m_data, m_data_end}; }

  // Returns the remaining readable byte count.
  constexpr std::size_t RemainingByteCount() const { return std::size_t(m_data_end - m_data); }

  constexpr u8 ReadByte()
  {
    DEBUG_ASSERT(RemainingByteCount() != 0);
    return *(m_data++);
  }

  constexpr void SkipBytes(std::size_t count)
  {
    DEBUG_ASSERT(RemainingByteCount() >= count);
    m_data += count;
  }

private:
  const u8* m_data;
  const u8* m_data_end;
};

class JVSIOResponseWriter
{
public:
  explicit JVSIOResponseWriter(std::vector<u8>* buffer) : m_buffer{*buffer} {}

  void AddData(u8 value) { m_buffer.emplace_back(value); }

  void AddData(std::span<const u8> data)
  {
#if defined(__cpp_lib_containers_ranges)
    m_buffer.append_range(data);
#else
    m_buffer.insert(m_buffer.end(), data.begin(), data.end());
#endif
  }

  void StartFrame(u8 destination_node)
  {
    m_buffer.clear();
    m_buffer.reserve(2 + 255);

    // Note: The JVSIO_SYNC is not included here.
    AddData(destination_node);
    AddData(0);  // Later becomes byte count.
    AddData(0);  // Later becomes status code.
  }

  void EndFrame(JVSIOStatusCode status_code)
  {
    std::size_t count_with_checksum = m_buffer.size() - 1;

    if (count_with_checksum > 255)
    {
      status_code = JVSIOStatusCode::ResponseOverflow;
      m_buffer.resize(3);
      count_with_checksum = 2;
      return;
    }

    // Write byte count to header.
    m_buffer[1] = u8(count_with_checksum);
    m_buffer[2] = u8(status_code);

    // Write checksum.
    const u8 checksum = std::accumulate(m_buffer.begin(), m_buffer.end(), u8{});
    AddData(checksum);
  }

  void StartReport()
  {
    // To be filled in later with SetLastReportCode.
    m_last_report_code_index = m_buffer.size();
    m_buffer.emplace_back();
  }

  void SetLastReportCode(JVSIOReportCode code) { m_buffer[m_last_report_code_index] = u8(code); }

private:
  std::vector<u8>& m_buffer;

  std::size_t m_last_report_code_index{};
};

// Attempts to decode exactly output.size() bytes.
// On success, returns the number of escaped bytes read.
static std::optional<std::size_t> UnescapeData(std::span<const u8> input, std::span<u8> output)
{
  auto in = input.begin();
  const auto in_end = input.end();

  auto out = output.begin();
  const auto out_end = output.end();

  while (true)
  {
    if (out == out_end)
      return in - input.begin();  // Success.

    if (in == in_end)
      return std::nullopt;

    u8 byte_value = *(in++);
    if (byte_value == JVSIO_MARK)
    {
      if (in == in_end)
        return std::nullopt;

      byte_value = *(in++) + 0x01;
    }

    *(out++) = byte_value;
  }
}

JVSIOBoard::JVSIOBoard(IOPorts* io_ports) : m_io_ports{io_ports}
{
}

void JVSIOBoard::Update()
{
  // Update coin counters.
  const auto coin_inputs = m_io_ports->GetCoinInputs();
  for (std::size_t i = 0; i != coin_inputs.size(); ++i)
  {
    if (!std::exchange(m_coin_prev_states[i], coin_inputs[i]) && coin_inputs[i])
      ++m_coin_counts[i];
  }

  while (true)
  {
    const auto rx_span = GetRxByteSpan();

    if (rx_span.empty())
      break;  // Wait for more data.

    if (rx_span[0] != JVSIO_SYNC)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "Expected JVSIO_SYNC");
      ConsumeRxBytes(1);
      continue;
    }

    std::array<u8, 2 + 255> unescaped_data;

    // Read 2 header bytes.
    const auto header_escaped_size =
        UnescapeData(rx_span.subspan(1), std::span{unescaped_data}.first(2));

    if (!header_escaped_size.has_value())
      break;  // Wait for more data.

    const u8 destination_node = unescaped_data[0];
    const u8 payload_size = unescaped_data[1];

    // Read remaining frame bytes.
    const auto payload_escaped_size =
        UnescapeData(rx_span.subspan(1 + *header_escaped_size),
                     std::span{unescaped_data}.subspan(2, payload_size));

    if (!payload_escaped_size.has_value())
      break;  // Wait for more data.

    Common::ScopeGuard consume_frame{
        [&] { ConsumeRxBytes(1 + *header_escaped_size + *payload_escaped_size); }};

    if (payload_size < 1)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "Empty payload");
      continue;
    }

    // Verify checksum.
    const auto range_to_checksum = std::span{unescaped_data}.first(payload_size + 1);
    const u8 expected_checksum =
        std::accumulate(range_to_checksum.begin(), range_to_checksum.end(), u8());
    if (expected_checksum != unescaped_data[payload_size + 1])
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "Bad checksum");

      JVSIOResponseWriter writer{&m_last_response};
      writer.StartFrame(JVSIO_HOST_ADDRESS);
      writer.EndFrame(JVSIOStatusCode::ChecksumError);
      WriteResponse(m_last_response);
      continue;
    }

    JVSIORequestReader request{range_to_checksum.subspan(2)};

    if (destination_node == JVSIO_BROADCAST_ADDRESS)
    {
      ProcessBroadcastRequest(&request);
    }
    else if (destination_node == m_client_address)
    {
      ProcessUnicastRequest(&request);
    }
    else
    {
      WARN_LOG_FMT(SERIALINTERFACE_JVSIO, "Unexpected destination node: 0x{:02x}",
                   destination_node);
    }

    if (const auto remaining = request.RemainingByteCount(); remaining > 0)
      WARN_LOG_FMT(SERIALINTERFACE_JVSIO, "Excess request bytes: {}", remaining);
  }
}

void JVSIOBoard::ProcessBroadcastRequest(JVSIORequestReader* request)
{
  if (request->RemainingByteCount() < 1)
    return;

  const auto cmd = JVSIOCommand(request->ReadByte());
  switch (cmd)
  {
  case JVSIOCommand::Reset:
  {
    if (request->RemainingByteCount() < 1)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "Reset: ParameterSizeError");
      break;
    }

    if (request->ReadByte() == 0xd9)
    {
      NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "Reset");
      m_client_address = 0;
      m_io_ports->GetStatusSwitches()[1] |= 0x01u;  // Update "sense" line.
    }

    // TODO: Does the real hardware reset coin counts ?

    m_io_ports->ResetGenericOutputs();
    m_last_response.clear();
    break;
  }
  case JVSIOCommand::SetAddress:
  {
    if (request->RemainingByteCount() < 1)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "SetAddress: ParameterSizeError");
      break;
    }

    const u8 address = request->ReadByte();

    // Note: We don't currently support daisy chaining.
    // This message would otherwise be conditionally ignored.

    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "SetAddress: {}", address);
    m_client_address = address;
    m_io_ports->GetStatusSwitches()[1] &= ~0x01u;  // Update "sense" line.

    JVSIOResponseWriter writer{&m_last_response};
    writer.StartFrame(JVSIO_HOST_ADDRESS);
    writer.AddData(u8(JVSIOReportCode::Okay));
    writer.EndFrame(JVSIOStatusCode::Okay);
    WriteResponse(m_last_response);
    break;
  }
  default:
    ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "ProcessBroadcastFrame: UnknownCommand: {:02x}", u8(cmd));
    break;
  }
}

void JVSIOBoard::ProcessUnicastRequest(JVSIORequestReader* request)
{
  if (request->RemainingByteCount() > 0 &&
      JVSIOCommand(request->PeekBytes().front()) == JVSIOCommand::DataRetransmit)
  {
    request->SkipBytes(1);
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "DataRetransmit");
    WriteResponse(m_last_response);
    return;
  }

  JVSIOResponseWriter response{&m_last_response};
  response.StartFrame(JVSIO_HOST_ADDRESS);

  FrameContext ctx{*request, response};

  while (true)
  {
    if (ctx.request.RemainingByteCount() == 0)
    {
      ctx.response.EndFrame(JVSIOStatusCode::Okay);
      break;
    }

    ctx.response.StartReport();

    const auto cmd = JVSIOCommand(ctx.request.ReadByte());
    const auto command_result = HandleCommand(cmd, ctx);

    // Entire frame error.
    if (!command_result.has_value())
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "HandleCommand: cmd:{:02x} StatusCode:{}", u8(cmd),
                    u8(command_result.error()));
      ctx.response.EndFrame(command_result.error());
      break;
    }

    ctx.response.SetLastReportCode(*command_result);

    // Command error.
    if (*command_result != JVSIOReportCode::Okay)
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "HandleCommand: cmd:{:02x} ReportCode:{}", u8(cmd),
                    u8(*command_result));
      ctx.response.EndFrame(JVSIOStatusCode::Okay);
      break;
    }
  }

  WriteResponse(m_last_response);
}

void JVSIOBoard::WriteResponse(std::span<const u8> response)
{
  WriteTxByte(JVSIO_SYNC);

  for (const u8 byte_value : response)
  {
    const bool needs_escaping = byte_value == JVSIO_SYNC || byte_value == JVSIO_MARK;
    if (needs_escaping)
    {
      WriteTxByte(JVSIO_MARK);
      WriteTxByte(byte_value - 0x01);
    }
    else
    {
      WriteTxByte(byte_value);
    }
  }
}

auto JVSIOBoard::HandleCommand(JVSIOCommand cmd, FrameContext ctx) -> HandlerResponse
{
  switch (cmd)
  {
  case JVSIOCommand::CommandRevision:
  {
    ctx.response.AddData(0x11);
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "CommandRevision");
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::JVSRevision:
  {
    ctx.response.AddData(0x20);
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "JVSRevision");
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::CommVersion:
  {
    ctx.response.AddData(0x10);
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "CommVersion");
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::MainID:
  {
    const auto remaining_bytes = ctx.request.PeekBytes();
    const auto null_iter = std::ranges::find(remaining_bytes, u8{});

    if (null_iter == remaining_bytes.end())
      return JVSIOReportCode::ParameterDataError;

    std::string_view main_id_str{reinterpret_cast<const char*>(remaining_bytes.data()),
                                 reinterpret_cast<const char*>(std::to_address(null_iter))};
    ctx.request.SkipBytes(main_id_str.size() + 1);

    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "MainID: {}", main_id_str);
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::SwitchInput:
  {
    if (ctx.request.RemainingByteCount() < 2)
      return JVSIOReportCode::ParameterSizeError;

    const u8 player_count = ctx.request.ReadByte();
    const u8 bytes_per_player = ctx.request.ReadByte();

    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "SwitchInput: player_count:{} bytes_per_player:{}",
                  player_count, bytes_per_player);

    ctx.response.AddData(*m_io_ports->GetSystemInputs());

    for (u8 player_index = 0; player_index != player_count; ++player_index)
    {
      const auto switch_inputs = m_io_ports->GetSwitchInputs(player_index);
      const auto bytes_to_use = std::min<std::size_t>(bytes_per_player, switch_inputs.size());

      ctx.response.AddData(switch_inputs.first(bytes_to_use));

      // Pad data if the game requests more inputs than we have (unlikely).
      for (std::size_t i = bytes_to_use; i != bytes_per_player; ++i)
        ctx.response.AddData(u8{});
    }

    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::AnalogInput:
  {
    if (ctx.request.RemainingByteCount() < 1)
      return JVSIOReportCode::ParameterSizeError;

    const u8 channel_count = ctx.request.ReadByte();

    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "AnalogInput: channel_count:{}", channel_count);

    const auto analog_inputs = m_io_ports->GetAnalogInputs();
    const auto inputs_to_use = std::min<std::size_t>(channel_count, analog_inputs.size());

    for (auto value : analog_inputs.first(inputs_to_use))
      ctx.response.AddData(Common::AsU8Span(Common::swap16(value)));

    // Pad data if the game requests more inputs than we have (unlikely).
    for (std::size_t i = inputs_to_use; i != channel_count; ++i)
      ctx.response.AddData(Common::AsU8Span(Common::swap16(IOPorts::NEUTRAL_ANALOG_VALUE)));

    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::GenericOutput:
  {
    if (ctx.request.RemainingByteCount() < 1)
      return JVSIOReportCode::ParameterSizeError;

    const u8 byte_count = ctx.request.ReadByte();

    if (ctx.request.RemainingByteCount() < byte_count)
      return JVSIOReportCode::ParameterSizeError;

    const auto output_span = ctx.request.PeekBytes().first(byte_count);

    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "GenericOutput: {}", fmt::join(output_span, " "));

    m_io_ports->SetGenericOutputs(output_span);

    ctx.request.SkipBytes(byte_count);

    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::CoinInput:
  {
    if (ctx.request.RemainingByteCount() < 1)
      return JVSIOReportCode::ParameterSizeError;

    const u8 slot_count = ctx.request.ReadByte();

    DEBUG_LOG_FMT(SERIALINTERFACE_JVSIO, "CoinInput: slot_count:{}", slot_count);

    for (u8 i = 0; i != slot_count; ++i)
    {
      auto condition_code = JVSIOCoinConditionCode::Normal;
      u16 coin_count = 0;

      if (i < m_coin_counts.size())
      {
        coin_count = m_coin_counts[i];
      }
      else
      {
        condition_code = JVSIOCoinConditionCode::CounterDisconnected;
      }

      // Top 2 bits contain the condition code.
      // Bottom 14 bits contain the coin count.
      const Common::BigEndianValue<u16> data{
          u16((u32(condition_code) << 14) | (coin_count & 0x3fffu))};

      ctx.response.AddData(Common::AsU8Span(data));
    }

    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::CoinCounterDec:
  case JVSIOCommand::CoinCounterInc:
  {
    if (ctx.request.RemainingByteCount() < 3)
      return JVSIOReportCode::ParameterSizeError;

    const u8 slot = ctx.request.ReadByte();
    const u32 coin_msb = ctx.request.ReadByte();
    const u32 coin_lsb = ctx.request.ReadByte();

    const auto adjustment =
        s32((coin_msb << 8u) | coin_lsb) * ((cmd == JVSIOCommand::CoinCounterDec) ? -1 : +1);

    NOTICE_LOG_FMT(SERIALINTERFACE_JVSIO, "CoinCounter: slot:{} adjustment:{}", slot, adjustment);

    if (slot < m_coin_counts.size())
      m_coin_counts[slot] = std::clamp(m_coin_counts[slot] + adjustment, 0, 0x3fff);

    return JVSIOReportCode::Okay;
  }
  default:
    return std::unexpected{JVSIOStatusCode::UnknownCommand};
  }
}

// JVS board specs largely sourced from:
// https://www.arcade-projects.com/threads/jvs-information-extractor.2071/

auto Namco_FCA_JVSIOBoard::HandleCommand(JVSIOCommand cmd, FrameContext ctx) -> HandlerResponse
{
  switch (cmd)
  {
  case JVSIOCommand::IOIdentify:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "IOIdentify");
    ctx.response.AddData(
        Common::AsU8Span(std::span{"namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder"}));
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::FeatureCheck:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "FeatureCheck");
    constexpr auto features = std::to_array<JVSIOFeatureSpec>({
        {JVSIOFeature::SwitchInput, 1, 16},
        {JVSIOFeature::CoinInput, 2},
        {JVSIOFeature::AnalogInput, 7},
        // Disabled to avoid unnecessary implementation of JVSIOCommand::RotaryInput
        // {JVSIOFeature::RotaryInput, 2},
        {JVSIOFeature::GeneralPurposeOutput, 6},
        {JVSIOFeature::AnalogOutput, 4},
    });
    ctx.response.AddData(Common::AsU8Span(features));
    ctx.response.AddData(u8{});  // End code

    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::NamcoCommand:
  {
    if (ctx.request.RemainingByteCount() < 1)
      return JVSIOReportCode::ParameterSizeError;

    const u8 namco_command = ctx.request.ReadByte();

    if (namco_command == 0x18)
    {
      constexpr u32 param_size = 4;

      if (ctx.request.RemainingByteCount() < param_size)
        return JVSIOReportCode::ParameterSizeError;

      const auto params = ctx.request.PeekBytes().first(param_size);

      INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "NAMCO_COMMAND({:02x}): {:02x}", namco_command,
                   fmt::join(params, " "));

      ctx.request.SkipBytes(param_size);
      ctx.response.AddData(0xff);
    }
    else
    {
      ERROR_LOG_FMT(SERIALINTERFACE_JVSIO, "Unknown NAMCO_COMMAND: {:02x}", namco_command);
      return JVSIOReportCode::ParameterDataError;
    }

    return JVSIOReportCode::Okay;
  }
  default:
    return JVSIOBoard::HandleCommand(cmd, ctx);
  }
}

auto Sega_837_13551_JVSIOBoard::HandleCommand(JVSIOCommand cmd, FrameContext ctx) -> HandlerResponse
{
  switch (cmd)
  {
  case JVSIOCommand::IOIdentify:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "IOIdentify");
    ctx.response.AddData(
        Common::AsU8Span(std::span{"SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"}));
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::FeatureCheck:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "FeatureCheck");
    constexpr auto features = std::to_array<JVSIOFeatureSpec>({
        {JVSIOFeature::SwitchInput, 2, 13},
        {JVSIOFeature::CoinInput, 2},
        {JVSIOFeature::AnalogInput, 8},
        {JVSIOFeature::GeneralPurposeOutput, 6},
    });
    ctx.response.AddData(Common::AsU8Span(features));
    ctx.response.AddData(u8{});  // End code

    return JVSIOReportCode::Okay;
  }
  default:
    return JVSIOBoard::HandleCommand(cmd, ctx);
  }
}

auto Sega_837_13844_JVSIOBoard::HandleCommand(JVSIOCommand cmd, FrameContext ctx) -> HandlerResponse
{
  switch (cmd)
  {
  case JVSIOCommand::IOIdentify:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "IOIdentify");
    ctx.response.AddData(Common::AsU8Span(
        std::span{"SEGA ENTERPRISES,LTD.;837-13844-01 I/O CNTL BD2 ;Ver1.00;99/07"}));
    return JVSIOReportCode::Okay;
  }
  case JVSIOCommand::FeatureCheck:
  {
    INFO_LOG_FMT(SERIALINTERFACE_JVSIO, "FeatureCheck");
    constexpr auto features = std::to_array<JVSIOFeatureSpec>({
        {JVSIOFeature::SwitchInput, 2, 12},
        {JVSIOFeature::CoinInput, 2},
        {JVSIOFeature::AnalogInput, 8},
        {JVSIOFeature::GeneralPurposeOutput, 22},
    });
    ctx.response.AddData(Common::AsU8Span(features));
    ctx.response.AddData(u8{});  // End code

    return JVSIOReportCode::Okay;
  }
  default:
    return JVSIOBoard::HandleCommand(cmd, ctx);
  }
}

void JVSIOBoard::DoState(PointerWrap& p)
{
  SerialDevice::DoState(p);

  p.Do(m_client_address);

  p.Do(m_last_response);

  p.Do(m_coin_counts);
  p.Do(m_coin_prev_states);
}

}  // namespace Triforce
