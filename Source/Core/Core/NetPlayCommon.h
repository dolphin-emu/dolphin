// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/NetPlayProto.h"

namespace NetPlay
{
using namespace std::chrono_literals;
// An arbitrary amount of time of no acknowledgement of sent packets before netplay decides a
// connection is disconnected
constexpr std::chrono::milliseconds PEER_TIMEOUT = 30s;

std::string GetExternalIPAddress();

bool CompressFileIntoPacket(const std::string& file_path, NetPacket& packet);
bool CompressFolderIntoPacket(const std::string& folder_path, NetPacket& packet);
bool CompressBufferIntoPacket(std::span<const u8> in_buffer, NetPacket& packet);
bool DecompressPacketIntoFile(NetPacket& packet, const std::string& file_path);
bool DecompressPacketIntoFolder(NetPacket& packet, const std::string& folder_path);
std::optional<std::vector<u8>> DecompressPacketIntoBuffer(NetPacket& packet);
}  // namespace NetPlay
