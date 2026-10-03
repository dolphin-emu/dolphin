// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <expected>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/HW/Triforce/IOPorts.h"
#include "Core/HW/Triforce/SerialDevice.h"

class PointerWrap;

namespace Triforce
{
enum class JVSIOStatusCode : u8;
enum class JVSIOReportCode : u8;
enum class JVSIOCommand : u8;

class JVSIORequestReader;
class JVSIOResponseWriter;

// "JAMMA Video Standard" I/O
class JVSIOBoard : public SerialDevice
{
public:
  explicit JVSIOBoard(IOPorts* io_ports);

  void Update() override;

  void DoState(PointerWrap& p) override;

protected:
  using HandlerResponse = std::expected<JVSIOReportCode, JVSIOStatusCode>;

  struct FrameContext
  {
    JVSIORequestReader& request;
    JVSIOResponseWriter& response;
  };

  virtual HandlerResponse HandleCommand(JVSIOCommand cmd, FrameContext ctx);

private:
  void ProcessBroadcastRequest(JVSIORequestReader* request);
  void ProcessUnicastRequest(JVSIORequestReader* request);

  void WriteResponse(std::span<const u8> response);

  // 0 == address not yet assigned.
  u8 m_client_address = 0;

  // Note: Does not include JVSIO_SYNC.
  std::vector<u8> m_last_response;

  std::array<u16, IOPorts::COIN_SLOT_COUNT> m_coin_counts{};
  std::array<bool, IOPorts::COIN_SLOT_COUNT> m_coin_prev_states{};

  IOPorts* const m_io_ports;
};

// Used by MarioKartGP and MarioKartGP2.
class Namco_FCA_JVSIOBoard final : public JVSIOBoard
{
public:
  using JVSIOBoard::JVSIOBoard;

  HandlerResponse HandleCommand(JVSIOCommand cmd, FrameContext ctx) override;
};

// Used by VirtuaStriker4 and others.
class Sega_837_13551_JVSIOBoard final : public JVSIOBoard
{
public:
  using JVSIOBoard::JVSIOBoard;

  HandlerResponse HandleCommand(JVSIOCommand cmd, FrameContext ctx) override;
};

// Used by FZeroAX (Deluxe).
class Sega_837_13844_JVSIOBoard final : public JVSIOBoard
{
public:
  using JVSIOBoard::JVSIOBoard;

  HandlerResponse HandleCommand(JVSIOCommand cmd, FrameContext ctx) override;
};

}  // namespace Triforce
