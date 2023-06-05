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

// The maximum possible characters the portal can handle.
// The status array is 32 bits and every character takes 2 bits.
// 32/2 = 16
constexpr u8 MAX_SKYLANDERS = 16;

namespace IOS::HLE::USB
{
extern const std::map<const std::pair<const u16, const u16>, const char*> list_skylanders;
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
  File::IOFile sky_file;
  u8 status = 0;
  std::queue<u8> queued_status;
  std::array<u8, 0x40 * 0x10> data{};
  u32 last_id = 0;
  void Save();

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

  bool CreateSkylander(const std::string& file_path, u16 sky_id, u16 sky_var);
  bool RemoveSkylander(u8 sky_num);
  u8 LoadSkylander(u8* buf, File::IOFile in_file);
  std::pair<u16, u16> CalculateIDs(const std::array<u8, 0x40 * 0x10>& file_data);

protected:
  std::mutex sky_mutex;

  bool m_activated = true;
  bool m_status_updated = false;
  u8 m_interrupt_counter = 0;
  SkylanderLEDColor m_color_right = {};
  SkylanderLEDColor m_color_left = {};
  SkylanderLEDColor m_color_trap = {};

  std::array<Skylander, MAX_SKYLANDERS> skylanders;

private:
  static bool IsSkylanderNumberValid(u8 sky_num);
  static bool IsBlockNumberValid(u8 block);
};

}  // namespace IOS::HLE::USB
