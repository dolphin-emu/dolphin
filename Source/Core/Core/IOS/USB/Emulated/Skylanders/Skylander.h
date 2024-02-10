// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <mutex>
#include <queue>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h"

// The maximum possible characters the portal can handle.
// The status array is 32 bits and every character takes 2 bits.
// 32/2 = 16
constexpr u8 MAX_SKYLANDERS = 16;
constexpr u8 NUM_SKYLANDER_GAMES = 5;
constexpr u8 NUM_SKYLANDER_TYPES = 10;
constexpr u8 NUM_SKYLANDER_ELEMENTS = 11;

namespace IOS::HLE::USB
{
enum class Game
{
  SpyrosAdv,
  Giants,
  SwapForce,
  TrapTeam,
  Superchargers
};
enum class Type : u8
{
  Skylander = 1,
  Giant,
  Swapper,
  TrapMaster,
  Mini,
  Item,
  Trophy,
  Vehicle,
  Trap,
  Unknown
};
Type NormalizeSkylanderType(Type type);
enum class Element
{
  Magic = 1,
  Fire,
  Air,
  Life,
  Undead,
  Earth,
  Water,
  Tech,
  Dark,
  Light,
  Other
};

struct SkyData
{
  const char* name = "";
  Game game = Game::SpyrosAdv;
  Element element = Element::Other;
  Type type = Type::Unknown;
};

extern const std::map<const std::pair<const u16, const u16>, SkyData> list_skylanders;
class SkylanderUSB final : public Device
{
public:
  SkylanderUSB(EmulationKernel& ios, const std::string& device_name);
  ~SkylanderUSB();
  DeviceDescriptor GetDeviceDescriptor() const override;
  std::vector<ConfigDescriptor> GetConfigurations() const override;
  std::vector<InterfaceDescriptor> GetInterfaces(u8 config) const override;
  std::vector<EndpointDescriptor> GetEndpoints(u8 config, u8 interface, u8 alt) const override;
  bool Attach() override;
  bool AttachAndChangeInterface(u8 interface) override;
  int CancelTransfer(u8 endpoint) override;
  int ChangeInterface(u8 interface) override;
  int GetNumberOfAltSettings(u8 interface) override;
  int SetAltSetting(u8 alt_setting) override;
  int SubmitTransfer(std::unique_ptr<CtrlMessage> message) override;
  int SubmitTransfer(std::unique_ptr<BulkMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IntrMessage> message) override;
  int SubmitTransfer(std::unique_ptr<IsoMessage> message) override;
  void ScheduleTransfer(std::unique_ptr<TransferCommand> command, const std::array<u8, 64>& data,
                        s32 expected_count, u64 expected_time_us);

private:
  EmulationKernel& m_ios;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  DeviceDescriptor m_device_descriptor;
  std::vector<ConfigDescriptor> m_config_descriptor;
  std::vector<InterfaceDescriptor> m_interface_descriptor;
  std::vector<EndpointDescriptor> m_endpoint_descriptor;
  std::queue<std::array<u8, 64>> m_queries;
};

struct Skylander final
{
  std::unique_ptr<SkylanderFigure> figure;
  u8 status = 0;
  std::queue<u8> queued_status;
  u32 last_id = 0;

  enum : u8
  {
    REMOVED = 0,
    READY = 1,
    REMOVING = 2,
    ADDED = 3
  };
};

struct SkylanderLEDColor final
{
  u8 red = 0;
  u8 green = 0;
  u8 blue = 0;
};

class SkylanderPortal final
{
public:
  void Activate();
  void Deactivate();
  bool IsActivated();
  void UpdateStatus();
  void SetLEDs(u8 side, u8 r, u8 g, u8 b);

  std::array<u8, 64> GetStatus();
  void QueryBlock(u8 sky_num, u8 block, u8* reply_buf);
  void WriteBlock(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf);

  bool RemoveSkylander(u8 sky_num);
  u8 LoadSkylander(std::unique_ptr<SkylanderFigure> figure);
  Skylander* GetSkylander(u8 slot);
  std::pair<u16, u16> CalculateIDs(const std::array<u8, 0x40 * 0x10>& file_data);

private:
  static bool IsSkylanderNumberValid(u8 sky_num);
  static bool IsBlockNumberValid(u8 block);

  std::mutex sky_mutex;

  bool m_activated = true;
  bool m_status_updated = false;
  u8 m_interrupt_counter = 0;
  SkylanderLEDColor m_color_right = {};
  SkylanderLEDColor m_color_left = {};
  SkylanderLEDColor m_color_trap = {};

  std::array<Skylander, MAX_SKYLANDERS> skylanders;
};

}  // namespace IOS::HLE::USB
