// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include "Common/CommonTypes.h"
#include "Core/HW/HSP/HSP_Device.h"

namespace HSP
{
class CHSPDevice_GBPlayer;

class IGBPlayer
{
public:
  explicit IGBPlayer(CHSPDevice_GBPlayer*);
  virtual ~IGBPlayer();

  virtual void Reset() = 0;
  virtual void Stop() = 0;
  virtual void Shutdown();

  virtual bool IsLoaded() const = 0;
  virtual bool IsGBA() const = 0;

  virtual bool LoadGame(const std::string& path) = 0;
  virtual bool LoadBootrom(const std::string& path) = 0;

  virtual void ReadScanlines(u32*) = 0;
  virtual void ReadAudio(u8*) = 0;

  virtual void SetKeys(u16) = 0;

protected:
  CHSPDevice_GBPlayer* const m_player;
};

class CHSPDevice_GBPlayer : public IHSPDevice
{
public:
  enum class IRQ : int;

  explicit CHSPDevice_GBPlayer(HSPDevices device);

  void Write(u32 address, u64 value) override;
  u64 Read(u32 address) override;

  void DoState(PointerWrap& p) override;

  void AssertIRQ(IRQ);

private:
  std::array<u8, 0x20> m_test;
  u8 m_control{0xCC};
  u16 m_irq{0};
  std::array<u32, 0x400> m_scanlines;
  std::array<u8, 0x400> m_audio;

  std::unique_ptr<IGBPlayer> m_gbp;
};
}  // namespace HSP
