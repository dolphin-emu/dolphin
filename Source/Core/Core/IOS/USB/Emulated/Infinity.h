// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <mutex>
#include <queue>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/IOS/USB/Common.h"

namespace IOS::HLE::USB
{
const u8 INFINITY_NUM_BLOCKS = 0x14;
const u8 INFINITY_BLOCK_SIZE = 0x10;

// Enum type, used when generating blank toys
enum class InfinityFigureType
{
  Character = 1,
  Playset = 2,
  Ability_A = 3,
  Ability_B = 4,
  Item = 5
};

struct InfinityFigureInfo
{
  InfinityFigureType type;
  u16 figure_number;
};

struct InfinityFigure final
{
  File::IOFile inf_file;
  std::array<u8, INFINITY_NUM_BLOCKS * INFINITY_BLOCK_SIZE> data{};
  bool present = false;
  u8 order_added = 255;
  void Save();
};

class InfinityUSB final : public Device
{
public:
  InfinityUSB(Kernel& ios, const std::string& device_name);
  ~InfinityUSB() override;
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

private:
  Kernel& m_ios;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u8 m_active_interface = 0;
  bool m_device_attached = false;
  DeviceDescriptor m_device_descriptor;
  std::vector<ConfigDescriptor> m_config_descriptor;
  std::vector<InterfaceDescriptor> m_interface_descriptor;
  std::vector<EndpointDescriptor> m_endpoint_descriptor;
  std::queue<std::array<u8, 32>> m_queries;
  std::queue<std::unique_ptr<IntrMessage>> m_response_list;
  void ScheduleTransfer(std::unique_ptr<TransferCommand> command, const std::array<u8, 32>& data,
                        s32 expected_count, u64 expected_time_us);
};

class InfinityBase final
{
public:
  bool HasFigureBeenAddedRemoved() const;
  std::array<u8, 32> GetAddedRemovedResponse();
  void GetBlankResponse(u8 sequence, u8* reply_buf);
  void GetPresentFigures(u8 sequence, u8* reply_buf);
  void GetFigureIdentifier(u8 fig_num, u8 sequence, u8* reply_buf);
  void QueryBlock(u8 fig_num, u8 block, u8* reply_buf, u8 sequence);
  void WriteBlock(u8 fig_num, u8 block, const u8* to_write_buf, u8* reply_buf, u8 sequence);
  void RemoveFigure(u8 position);
  // Returns Infinity Figure name based on data from in_file param
  std::string LoadFigure(u8* buf, File::IOFile in_file, u8 position);
  bool CreateFigure(const std::string& file_path, u16 character);
  static const std::map<const char*, const InfinityFigureInfo>& GetFigureList();
  std::string FindFigure(u16 character) const;
  void GenerateSeed(u32 seed);
  u32 GetNext();
  u64 Scramble(u32 num_to_scramble, u32 garbage);
  u32 Descramble(u64 num_to_descramble);
  u8 GenerateChecksum(const u8* data, int num_of_bytes) const;

protected:
  std::mutex m_infinity_mutex;
  InfinityFigure m_player_one;
  InfinityFigure m_player_two;
  InfinityFigure m_player_one_ability_one;
  InfinityFigure m_player_one_ability_two;
  InfinityFigure m_player_two_ability_one;
  InfinityFigure m_player_two_ability_two;
  InfinityFigure m_hexagon;

private:
  // These 4 variables are state variables used during the seeding of, and use of the random number
  // generator.
  u32 m_random_a;
  u32 m_random_b;
  u32 m_random_c;
  u32 m_random_d;

  u8 m_figure_order = 0;
  std::queue<std::array<u8, 32>> m_figure_added_removed_response;
};
}  // namespace IOS::HLE::USB
