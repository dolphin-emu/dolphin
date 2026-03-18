// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

class PointerWrap;

namespace Triforce
{

class ICCardReader;
class IOAdapter;

// Triforce GPIO peripherals connect to JVS-IO and eachother in game specific ways.
// This class hopes to handle those customizable connections.
// TODO: Use this for other JVS-IO (Analog Input, Coin, etc.).
class IOPorts
{
public:
  void Update();

  std::span<u8> GetSwitchInputs(u32 player_index)
  {
    ASSERT(player_index < PLAYER_COUNT);

    return std::span{m_switch_input_data}.subspan(SWITCH_INPUT_BYTES_PER_PLAYER * player_index,
                                                  SWITCH_INPUT_BYTES_PER_PLAYER);
  }
  std::span<const u8> GetSwitchInputs(u32 player_index) const
  {
    ASSERT(player_index < PLAYER_COUNT);

    return std::span{m_switch_input_data}.subspan(SWITCH_INPUT_BYTES_PER_PLAYER * player_index,
                                                  SWITCH_INPUT_BYTES_PER_PLAYER);
  }

  std::span<u8> GetGenericOutputs() { return m_generic_output_data; }
  std::span<const u8> GetGenericOutputs() const { return m_generic_output_data; }

  void SetGenericOutputs(std::span<const u8> bytes);

  void AddIOAdapter(std::unique_ptr<IOAdapter> adapter);

  void DoState(PointerWrap& p);

private:
  std::vector<std::unique_ptr<IOAdapter>> m_io_adapters;

  static constexpr std::size_t PLAYER_COUNT = 2;
  static constexpr std::size_t SWITCH_INPUT_BYTES_PER_PLAYER = 2;

  std::array<u8, PLAYER_COUNT * SWITCH_INPUT_BYTES_PER_PLAYER> m_switch_input_data{};

  static constexpr std::size_t GENERIC_OUTPUT_BYTE_COUNT = 4;

  std::array<u8, GENERIC_OUTPUT_BYTE_COUNT> m_generic_output_data{};
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

protected:
  IOPorts* GetIOPorts() { return m_io_ports; }

  // Invoked with newly set and cleared bits.
  virtual void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                           std::span<const u8> bits_cleared);

private:
  void SetIOPorts(IOPorts* io_ports) { m_io_ports = io_ports; }

  IOPorts* m_io_ports{};
};

// Used for both MarioKartGP and MarioKartGP2.
class MarioKartGPCommon_IOAdapter final : public IOAdapter
{
protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  static constexpr u8 ITEM_LIGHT_BIT = 0x04;
  static constexpr u8 CANCEL_LIGHT_BIT = 0x08;
};

// Common functionality for both VirtuaStriker4 and VirtuaStriker4_2006.
class VirtuaStriker4Common_IOAdapter final : public IOAdapter
{
public:
  VirtuaStriker4Common_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_reader_a{card_reader_a}, m_card_reader_b{card_reader_b}
  {
  }

  void Update() override;

protected:
  static constexpr u8 SLOT_A_LOCK_BIT = 0x40;
  static constexpr u8 SLOT_B_LOCK_BIT = 0x10;

  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  ICCardReader* const m_card_reader_a;
  ICCardReader* const m_card_reader_b;
};

class VirtuaStriker4_2006_IOAdapter final : public IOAdapter
{
public:
  VirtuaStriker4_2006_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_reader_a{card_reader_a}, m_card_reader_b{card_reader_b}
  {
  }

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  static constexpr u8 SLOT_A_EJECT_BIT = 0x80;
  static constexpr u8 SLOT_B_EJECT_BIT = 0x20;

  ICCardReader* const m_card_reader_a;
  ICCardReader* const m_card_reader_b;
};

class GekitouProYakyuu_IOAdapter final : public IOAdapter
{
public:
  GekitouProYakyuu_IOAdapter(ICCardReader* card_reader_a, ICCardReader* card_reader_b)
      : m_card_reader_a{card_reader_a}, m_card_reader_b{card_reader_b}
  {
  }

  void Update() override;

protected:
  void HandleGenericOutputsChanged(std::span<const u8> bits_set,
                                   std::span<const u8> bits_cleared) override;

private:
  static constexpr u8 SLOT_A_LOCK_BIT = 0x40;
  static constexpr u8 SLOT_B_LOCK_BIT = 0x10;

  ICCardReader* const m_card_reader_a;
  ICCardReader* const m_card_reader_b;
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
