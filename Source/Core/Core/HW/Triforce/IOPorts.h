// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class System;
}

namespace Triforce
{

class ICCardReader;
class IOAdapter;

// Triforce GPIO peripherals connect to JVS-IO and eachother in game specific ways.
// This class hopes to handle those customizable connections.
class IOPorts
{
public:
  void Update();

  std::span<u8> GetStatusSwitches() { return m_status_switches; }
  std::span<const u8> GetStatusSwitches() const { return m_status_switches; }

  u8* GetSystemInputs() { return &m_system_inputs; }
  const u8* GetSystemInputs() const { return &m_system_inputs; }

  std::span<u8> GetSwitchInputs(u32 player_index);
  std::span<const u8> GetSwitchInputs(u32 player_index) const;

  std::span<u16> GetAnalogInputs() { return m_analog_inputs; }
  std::span<const u16> GetAnalogInputs() const { return m_analog_inputs; }

  std::span<const u8> GetGenericOutputs() const { return m_generic_outputs; }

  void SetGenericOutputs(std::span<const u8> bytes);
  void ResetGenericOutputs();

  static constexpr std::size_t PLAYER_COUNT = 2;
  static constexpr std::size_t COIN_SLOT_COUNT = 2;

  std::span<bool, COIN_SLOT_COUNT> GetCoinInputs() { return m_coin_inputs; }
  std::span<const bool, COIN_SLOT_COUNT> GetCoinInputs() const { return m_coin_inputs; }

  void AddIOAdapter(std::unique_ptr<IOAdapter> adapter);

  void DoState(PointerWrap& p);

  static constexpr u16 NEUTRAL_ANALOG_VALUE = 0x8000;

private:
  std::vector<std::unique_ptr<IOAdapter>> m_io_adapters;

  std::array<u8, 2> m_status_switches{0xff, 0xff};

  // The first byte in JVSIO switch input data.
  u8 m_system_inputs = 0x00;

  static constexpr std::size_t SWITCH_INPUT_BYTES_PER_PLAYER = 2;

  std::array<u8, PLAYER_COUNT * SWITCH_INPUT_BYTES_PER_PLAYER> m_switch_inputs{};

  static constexpr std::size_t ANALOG_INPUT_COUNT = 8;

  std::array<u16, ANALOG_INPUT_COUNT> m_analog_inputs{};

  static constexpr std::size_t GENERIC_OUTPUT_BYTE_COUNT = 4;

  std::array<u8, GENERIC_OUTPUT_BYTE_COUNT> m_generic_outputs{};

  std::array<bool, COIN_SLOT_COUNT> m_coin_inputs{};
};

class IOAdapter
{
  friend IOPorts;

public:
  IOAdapter();
  virtual ~IOAdapter();

  IOAdapter(const IOAdapter&) = delete;
  IOAdapter& operator=(const IOAdapter&) = delete;
  IOAdapter(IOAdapter&&) = delete;
  IOAdapter& operator=(IOAdapter&&) = delete;

  virtual void Update();

  virtual void DoState(PointerWrap& p);

protected:
  IOPorts* GetIOPorts() { return m_io_ports; }

  // Invoked with newly set and cleared bits.
  virtual void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                           std::span<const u8> bits_cleared);

private:
  void SetIOPorts(IOPorts* io_ports) { m_io_ports = io_ports; }

  IOPorts* m_io_ports{};
};

class Common_IOAdapter final : public IOAdapter
{
public:
  explicit Common_IOAdapter(Core::System& system) : m_system{system} {}

  void Update() override;

private:
  Core::System& m_system;
};

class VirtuaStriker3_IOAdapter final : public IOAdapter
{
public:
  void Update() override;
};

// Common functionality for both VirtuaStriker4 and VirtuaStriker4_2006.
class VirtuaStriker4Common_IOAdapter final : public IOAdapter
{
public:
  VirtuaStriker4Common_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_readers{card_reader_a, card_reader_b}
  {
  }

  void Update() override;

protected:
  static constexpr u8 SLOT_A_LOCK_BIT = 0x40;
  static constexpr u8 SLOT_B_LOCK_BIT = 0x10;

  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  std::array<ICCardReader*, IOPorts::PLAYER_COUNT> const m_card_readers;
};

class VirtuaStriker4_2006_IOAdapter final : public IOAdapter
{
public:
  VirtuaStriker4_2006_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_readers{card_reader_a, card_reader_b}
  {
  }

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  static constexpr u8 SLOT_A_EJECT_BIT = 0x80;
  static constexpr u8 SLOT_B_EJECT_BIT = 0x20;

  std::array<ICCardReader*, IOPorts::PLAYER_COUNT> const m_card_readers;
};

class GekitouProYakyuu_IOAdapter final : public IOAdapter
{
public:
  GekitouProYakyuu_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_readers{card_reader_a, card_reader_b}
  {
  }

  void Update() override;

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  static constexpr u8 SLOT_A_LOCK_BIT = 0x40;
  static constexpr u8 SLOT_B_LOCK_BIT = 0x10;

  std::array<ICCardReader*, IOPorts::PLAYER_COUNT> const m_card_readers;
};

class KeyOfAvalon_IOAdapter final : public IOAdapter
{
public:
  explicit KeyOfAvalon_IOAdapter(ICCardReader* card_reader) : m_card_reader{card_reader} {}

  void Update() override;

private:
  ICCardReader* const m_card_reader;
};

}  // namespace Triforce
