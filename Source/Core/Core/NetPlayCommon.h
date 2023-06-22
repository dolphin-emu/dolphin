// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SFML/Network/Packet.hpp>

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace NetPlay
{
using namespace std::chrono_literals;
// An arbitrary amount of time of no acknowledgement of sent packets before netplay decides a
// connection is disconnected
constexpr std::chrono::milliseconds PEER_TIMEOUT = 30s;

bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet);
bool CompressFolderIntoPacket(const std::string& folder_path, sf::Packet& packet);
bool CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet);
bool DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path);
bool DecompressPacketIntoFolder(sf::Packet& packet, const std::string& folder_path);
std::optional<std::vector<u8>> DecompressPacketIntoBuffer(sf::Packet& packet);
}  // namespace NetPlay
