// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <mutex>
#include <queue>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Core/IOS/USB/Common.h"

namespace IOS::HLE::USB
{
constexpr u8 INFINITY_NUM_BLOCKS = 0x14;
constexpr u8 INFINITY_BLOCK_SIZE = 0x10;

struct InfinityFigure final
{
  void Save();

  File::IOFile inf_file;
  std::array<u8, INFINITY_NUM_BLOCKS * INFINITY_BLOCK_SIZE> data{};
  bool present = false;
  u8 order_added = 255;
};

class InfinityUSB final : public Device
{
public:
  InfinityUSB(EmulationKernel& ios, const std::string& device_name);
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
  void ScheduleTransfer(std::unique_ptr<TransferCommand> command, const std::array<u8, 32>& data,
                        u64 expected_time_us);

  EmulationKernel& m_ios;
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
};

class InfinityBase final
{
public:
  bool HasFigureBeenAddedRemoved() const;
  std::array<u8, 32> PopAddedRemovedResponse();
  void GetBlankResponse(u8 sequence, std::array<u8, 32>& reply_buf);
  void GetPresentFigures(u8 sequence, std::array<u8, 32>& reply_buf);
  void GetFigureIdentifier(u8 fig_num, u8 sequence, std::array<u8, 32>& reply_buf);
  void QueryBlock(u8 fig_num, u8 block, std::array<u8, 32>& reply_buf, u8 sequence);
  void WriteBlock(u8 fig_num, u8 block, const u8* to_write_buf, std::array<u8, 32>& reply_buf,
                  u8 sequence);
  void DescrambleAndSeed(u8* buf, u8 sequence, std::array<u8, 32>& reply_buf);
  void GetNextAndScramble(u8 sequence, std::array<u8, 32>& reply_buf);
  void RemoveFigure(u8 position);
  // Returns Infinity Figure name based on data from in_file param
  std::string LoadFigure(const std::array<u8, INFINITY_NUM_BLOCKS * INFINITY_BLOCK_SIZE>& buf,
                         File::IOFile in_file, u8 position);
  bool CreateFigure(const std::string& file_path, u32 character);
  static std::span<const std::pair<const char*, const u32>> GetFigureList();
  std::string FindFigure(u32 character) const;

protected:
  std::mutex m_infinity_mutex;
  std::array<InfinityFigure, 7> m_figures;

private:
  InfinityFigure& GetFigureByOrder(u8 order_added);
  std::array<u8, 16> GenerateInfinityFigureKey(const std::vector<u8>& sha1_data);
  std::array<u8, 16> GenerateBlankFigureData(u32 figure_num);
  u8 DeriveFigurePosition(u8 position);
  void GenerateSeed(u32 seed);
  u32 GetNext();
  u64 Scramble(u32 num_to_scramble, u32 garbage);
  u32 Descramble(u64 num_to_descramble);
  u8 GenerateChecksum(const std::array<u8, 32>& data, int num_of_bytes) const;

  // These 4 variables are state variables used during the seeding and use of the random number
  // generator.
  u32 m_random_a;
  u32 m_random_b;
  u32 m_random_c;
  u32 m_random_d;

  u8 m_figure_order = 0;
  std::queue<std::array<u8, 32>> m_figure_added_removed_response;
};
}  // namespace IOS::HLE::USB
