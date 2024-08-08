// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

class PointerWrap;

namespace Core
{
class System;
}
namespace Memcard
{
struct HeaderData;
}

namespace ExpansionInterface
{
enum class EXIDeviceType : int
{
  Dummy,
  MemoryCard,
  MaskROM,
  AD16,
  Microphone,
  Ethernet,
  // Was used for Triforce in the past, but the implementation is no longer in Dolphin.
  // It's kept here so that values below will stay constant.
  AMBaseboard,
  Gecko,
  // Only used when creating a device by EXIDevice_Create.
  // Converted to MemoryCard internally.
  MemoryCardFolder,
  AGP,
  EthernetXLink,
  EthernetTapServer,
  EthernetBuiltIn,
  ModemTapServer,
  SD,
  None = 0xFF
};

class IEXIDevice
{
public:
  explicit IEXIDevice(Core::System& system);
  virtual ~IEXIDevice() = default;

  // Immediate copy functions
  virtual void ImmWrite(u32 data, u32 size);
  virtual u32 ImmRead(u32 size);
  virtual void ImmReadWrite(u32& data, u32 size);

  // DMA copy functions
  virtual void DMAWrite(u32 address, u32 size);
  virtual void DMARead(u32 address, u32 size);

  virtual bool UseDelayedTransferCompletion() const;
  virtual bool IsPresent() const;
  virtual void SetCS(u32 cs, bool was_selected, bool is_selected);
  virtual void DoState(PointerWrap& p);

  // Is generating interrupt ?
  virtual bool IsInterruptSet();

  // For savestates. storing it here seemed cleaner than requiring each implementation to report its
  // type. I know this class is set up like an interface, but no code requires it to be strictly
  // such.
  EXIDeviceType m_device_type = EXIDeviceType::None;

protected:
  Core::System& m_system;

private:
  // Byte transfer function for this device
  virtual void TransferByte(u8& byte);
};

std::unique_ptr<IEXIDevice> EXIDevice_Create(Core::System& system, EXIDeviceType device_type,
                                             int channel_num,
                                             const Memcard::HeaderData& memcard_header_data);
}  // namespace ExpansionInterface

template <>
struct fmt::formatter<ExpansionInterface::EXIDeviceType>
    : EnumFormatter<ExpansionInterface::EXIDeviceType::SD>
{
  static constexpr array_type names = {
      _trans("Dummy"),
      _trans("Memory Card"),
      _trans("Mask ROM"),
      // i18n: A mysterious debugging/diagnostics peripheral for the GameCube.
      _trans("AD16"),
      _trans("Microphone"),
      _trans("Broadband Adapter (TAP)"),
      _trans("Triforce AM Baseboard"),
      _trans("USB Gecko"),
      _trans("GCI Folder"),
      _trans("Advance Game Port"),
      _trans("Broadband Adapter (XLink Kai)"),
      _trans("Broadband Adapter (tapserver)"),
      _trans("Broadband Adapter (HLE)"),
      _trans("Modem Adapter (tapserver)"),
      _trans("SD Adapter"),
  };

  constexpr formatter() : EnumFormatter(names) {}

  template <typename FormatContext>
  auto format(const ExpansionInterface::EXIDeviceType& e, FormatContext& ctx)
  {
    if (e != ExpansionInterface::EXIDeviceType::None)
    {
      return EnumFormatter::format(e, ctx);
    }
    else
    {
      // Special-case None since it has a fixed ID (0xff) that is much larger than the rest; we
      // don't need 200 nullptr entries in names.  We also want to format it specially in the UI.
      switch (format_type)
      {
      default:
      case 'u':
        return fmt::format_to(ctx.out(), "None");
      case 's':
        return fmt::format_to(ctx.out(), "0xffu /* None */");
      case 'n':
        return fmt::format_to(ctx.out(), _trans("<Nothing>"));
      }
    }
  }
};
