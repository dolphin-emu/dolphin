// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>

#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace Core
{
class System;
}

namespace HSP
{
class CHSPDevice_GBPlayer;

class IGBPlayer
{
public:
  // Audio/Video data is read in chunks.
  static constexpr std::size_t AV_REGION_SIZE = 0x400;

  static constexpr std::size_t AUDIO_SAMPLE_RATE = 0x8000;
  static constexpr std::size_t AUDIO_READ_SIZE = 8;
  static constexpr std::size_t AUDIO_CHANNEL_COUNT = 2;

  IGBPlayer(Core::System&, CHSPDevice_GBPlayer*);
  virtual ~IGBPlayer();

  virtual void Reset() = 0;
  virtual void Stop() = 0;

  virtual bool IsLoaded() const = 0;
  virtual bool IsGBA() const = 0;

  virtual void ReadScanlines(std::span<u32, AV_REGION_SIZE>) = 0;
  virtual void ReadAudio(std::span<u8, AV_REGION_SIZE>) = 0;

  virtual void SetKeys(u16) = 0;

  virtual void DoState(PointerWrap& p) = 0;

protected:
  Core::System& m_system;
  CHSPDevice_GBPlayer* const m_player;
};

class CHSPDevice_GBPlayer final : public IHSPDevice
{
public:
  enum class IRQ : int;

  explicit CHSPDevice_GBPlayer(Core::System& system);

  HSPDeviceType GetDeviceType() const override { return HSPDeviceType::GBPlayer; }

  void Read(u32 address, std::span<u8, TRANSFER_SIZE> data) override;
  void Write(u32 address, std::span<const u8, TRANSFER_SIZE> data) override;

  void DoState(PointerWrap& p) override;

  void AssertIRQ(IRQ);

private:
  void UpdateInterrupts();

  Core::System& m_system;

  std::array<u8, 0x20> m_test{};
  u8 m_control{0xCC};
  u16 m_irq{0};

  std::array<u32, IGBPlayer::AV_REGION_SIZE> m_scanlines{};
  std::array<u8, IGBPlayer::AV_REGION_SIZE> m_audio{};

  std::unique_ptr<IGBPlayer> m_gbp;
};

}  // namespace HSP
