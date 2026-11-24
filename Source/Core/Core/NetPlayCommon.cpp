// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayCommon.h"

#include <algorithm>

#include <fmt/format.h>
#include <lz4.h>

#include "Common/Buffer.h"
#include "Common/DirectIOFile.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/MsgHandler.h"
#include "Common/SFMLHelper.h"

namespace NetPlay
{
static constexpr u64 COMPRESSION_MAX_CHUNK_SIZE = 1024uz * 64;  // Arbitrary.
static_assert(COMPRESSION_MAX_CHUNK_SIZE <= LZ4_MAX_INPUT_SIZE);

bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet)
{
  File::DirectIOFile file(file_path, File::AccessMode::Read);
  if (!file.IsOpen())
  {
    PanicAlertFmtT("Failed to open file \"{0}\".", file_path);
    return false;
  }

  u64 remaining_size = file.GetSize();
  packet << remaining_size;

  if (remaining_size == 0)
    return true;

  const int in_buffer_size =
      MathUtil::SaturatingCast<int>(std::min(remaining_size, COMPRESSION_MAX_CHUNK_SIZE));
  const int out_buffer_size = LZ4_compressBound(in_buffer_size);

  Common::UniqueBuffer<u8> in_buffer(in_buffer_size);
  Common::UniqueBuffer<u8> out_buffer(out_buffer_size);

  while (true)
  {
    const auto bytes_to_read = int(std::min<u64>(remaining_size, in_buffer_size));

    if (!file.Read(std::span(in_buffer).first(bytes_to_read)))
    {
      PanicAlertFmtT("Error reading file: {0}", file_path);
      return false;
    }

    const int out_len = LZ4_compress_default(reinterpret_cast<const char*>(in_buffer.data()),
                                             reinterpret_cast<char*>(out_buffer.data()),
                                             bytes_to_read, out_buffer_size);
    if (out_len == 0)
    {
      PanicAlertFmtT("Internal Error - compression failed");
      return false;
    }

    packet << static_cast<u32>(out_len);
    packet.append(out_buffer.data(), out_len);

    remaining_size -= bytes_to_read;
    if (remaining_size == 0)
      break;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

static bool CompressFolderIntoPacketInternal(const File::FSTEntry& folder, sf::Packet& packet)
{
  const u64 size = folder.children.size();
  packet << size;
  for (const auto& child : folder.children)
  {
    const bool is_folder = child.isDirectory;
    packet << child.virtualName;
    packet << is_folder;
    const bool success = is_folder ? CompressFolderIntoPacketInternal(child, packet) :
                                     CompressFileIntoPacket(child.physicalName, packet);
    if (!success)
      return false;
  }
  return true;
}

bool CompressFolderIntoPacket(const std::string& folder_path, sf::Packet& packet)
{
  if (!File::IsDirectory(folder_path))
  {
    packet << false;
    return true;
  }

  packet << true;
  return CompressFolderIntoPacketInternal(File::ScanDirectoryTree(folder_path, true), packet);
}

bool CompressBufferIntoPacket(std::span<const u8> in_buffer, sf::Packet& packet)
{
  const u64 size = in_buffer.size();
  packet << size;

  if (size == 0)
    return true;

  const int chunk_size = MathUtil::SaturatingCast<int>(std::min(size, COMPRESSION_MAX_CHUNK_SIZE));
  const int out_buffer_size = LZ4_compressBound(chunk_size);

  Common::UniqueBuffer<char> out_buffer(out_buffer_size);

  while (true)
  {
    const auto bytes_to_read = int(std::min<u64>(in_buffer.size(), chunk_size));

    const int out_len = LZ4_compress_default(reinterpret_cast<const char*>(in_buffer.data()),
                                             out_buffer.data(), bytes_to_read, out_buffer_size);
    if (out_len == 0)
    {
      PanicAlertFmtT("Internal Error - compression failed");
      return false;
    }

    packet << static_cast<u32>(out_len);
    packet.append(out_buffer.data(), out_len);

    in_buffer = in_buffer.subspan(bytes_to_read);
    if (in_buffer.empty())
      break;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

bool DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path)
{
  const u64 file_size = Common::PacketReadU64(packet);
  if (file_size == 0)
    return true;

  File::DirectIOFile file(file_path, File::AccessMode::Write);
  if (!file.IsOpen())
  {
    PanicAlertFmtT("Failed to open file \"{0}\". Verify your write permissions.", file_path);
    return false;
  }

  // Avoid fragmentation.
  File::Resize(file, file_size);

  const int out_buffer_size =
      MathUtil::SaturatingCast<int>(std::min(file_size, COMPRESSION_MAX_CHUNK_SIZE));

  Common::UniqueBuffer<u8> out_buffer(out_buffer_size);

  while (true)
  {
    u32 cur_len = 0;
    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    const u64 bytes_in_packet = packet.getDataSize() - packet.getReadPosition();
    if (cur_len > bytes_in_packet)
    {
      PanicAlertFmt("Undersized packet - decompression failed");
      return false;
    }

    const int out_len = LZ4_decompress_safe(
        reinterpret_cast<const char*>(packet.getData()) + packet.getReadPosition(),
        reinterpret_cast<char*>(out_buffer.data()), int(cur_len), out_buffer_size);
    if (out_len < 0)
    {
      PanicAlertFmtT("Internal Error - decompression failed");
      return false;
    }

    if (!file.Write(std::span{out_buffer}.first(out_len)))
    {
      PanicAlertFmtT("Error writing file: {0}", file_path);
      return false;
    }

    // Consume the bytes that we used from the packet.
    for (u8 dummy{}; cur_len != 0; --cur_len)
      packet >> dummy;
  }

  return file.Tell() == file_size;
}

static bool DecompressPacketIntoFolderInternal(sf::Packet& packet, const std::string& folder_path)
{
  if (!File::CreateFullPath(folder_path + "/"))
    return false;

  u64 size;
  packet >> size;
  for (size_t i = 0; i < size; ++i)
  {
    std::string name;
    packet >> name;

    if (name.contains('/'))
      return false;
#ifdef _WIN32
    if (name.contains('\\'))
      return false;
#endif
    if (std::ranges::all_of(name, [](char c) { return c == '.'; }))
      return false;

    bool is_folder;
    packet >> is_folder;
    std::string path = fmt::format("{}/{}", folder_path, name);
    const bool success = is_folder ? DecompressPacketIntoFolderInternal(packet, path) :
                                     DecompressPacketIntoFile(packet, path);
    if (!success)
      return false;
  }
  return true;
}

bool DecompressPacketIntoFolder(sf::Packet& packet, const std::string& folder_path)
{
  bool folder_existed;
  packet >> folder_existed;
  if (!folder_existed)
    return true;
  return DecompressPacketIntoFolderInternal(packet, folder_path);
}

std::optional<std::vector<u8>> DecompressPacketIntoBuffer(sf::Packet& packet)
{
  const u64 size = Common::PacketReadU64(packet);
  if (size == 0)
    return {};

  std::vector<u8> out_buffer(size);
  u64 out_pos = 0;

  while (true)
  {
    u32 cur_len = 0;  // number of bytes to read
    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    const u64 bytes_in_packet = packet.getDataSize() - packet.getReadPosition();
    if (cur_len > bytes_in_packet)
    {
      PanicAlertFmt("Undersized packet - decompression failed");
      return {};
    }

    const int out_bytes_left = MathUtil::SaturatingCast<int>(out_buffer.size() - out_pos);

    const int out_len = LZ4_decompress_safe(
        reinterpret_cast<const char*>(packet.getData()) + packet.getReadPosition(),
        reinterpret_cast<char*>(out_buffer.data()) + out_pos, int(cur_len), out_bytes_left);
    if (out_len < 0)
    {
      PanicAlertFmtT("Internal Error - decompression failed");
      return {};
    }

    out_pos += out_len;

    // Consume the bytes that we used from the packet.
    for (u8 dummy{}; cur_len != 0; --cur_len)
      packet >> dummy;
  }

  if (out_pos != size)
  {
    PanicAlertFmtT("Error - output size mismatch");
    return {};
  }

  return out_buffer;
}

std::string GetExternalIPAddress()
{
  Common::HttpRequest request;
  // ENet does not support IPv6, so IPv4 has to be used
  request.UseIPv4();
  Common::HttpRequest::Response response =
      request.Get("https://ip.dolphin-emu.org/", {{"X-Is-Dolphin", "1"}});

  if (response.has_value())
    return std::string(response->begin(), response->end());
  return "";
}

}  // namespace NetPlay
