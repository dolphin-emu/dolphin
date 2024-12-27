// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayCommon.h"

#include <algorithm>

#include <fmt/format.h>
#include <lzo/lzo1x.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/MsgHandler.h"
#include "Common/SFMLHelper.h"

namespace NetPlay
{
constexpr u32 LZO_IN_LEN = 1024 * 64;
constexpr u32 LZO_OUT_LEN = LZO_IN_LEN + (LZO_IN_LEN / 16) + 64 + 3;

bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet)
{
  File::IOFile file(file_path, "rb");
  if (!file)
  {
    PanicAlertFmtT("Failed to open file \"{0}\".", file_path);
    return false;
  }

  const sf::Uint64 size = file.GetSize();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> in_buffer(LZO_IN_LEN);
  std::vector<u8> out_buffer(LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // EOF

    if (!file.ReadBytes(in_buffer.data(), cur_len))
    {
      PanicAlertFmtT("Error reading file: {0}", file_path.c_str());
      return false;
    }

    if (lzo1x_1_compress(in_buffer.data(), cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertFmtT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

static bool CompressFolderIntoPacketInternal(const File::FSTEntry& folder, sf::Packet& packet)
{
  const sf::Uint64 size = folder.children.size();
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

bool CompressBufferIntoPacket(const std::vector<u8>& in_buffer, sf::Packet& packet)
{
  const sf::Uint64 size = in_buffer.size();
  packet << size;

  if (size == 0)
    return true;

  std::vector<u8> out_buffer(LZO_OUT_LEN);
  std::vector<u8> wrkmem(LZO1X_1_MEM_COMPRESS);

  lzo_uint i = 0;
  while (true)
  {
    lzo_uint32 cur_len = 0;  // number of bytes to read
    lzo_uint out_len = 0;    // number of bytes to write

    if ((i + LZO_IN_LEN) >= size)
    {
      cur_len = static_cast<lzo_uint32>(size - i);
    }
    else
    {
      cur_len = LZO_IN_LEN;
    }

    if (cur_len <= 0)
      break;  // end of buffer

    if (lzo1x_1_compress(&in_buffer[i], cur_len, out_buffer.data(), &out_len, wrkmem.data()) !=
        LZO_E_OK)
    {
      PanicAlertFmtT("Internal LZO Error - compression failed");
      return false;
    }

    // The size of the data to write is 'out_len'
    packet << static_cast<u32>(out_len);
    for (size_t j = 0; j < out_len; j++)
    {
      packet << out_buffer[j];
    }

    if (cur_len != LZO_IN_LEN)
      break;

    i += cur_len;
  }

  // Mark end of data
  packet << static_cast<u32>(0);

  return true;
}

bool DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path)
{
  u64 file_size = Common::PacketReadU64(packet);

  if (file_size == 0)
    return true;

  File::IOFile file(file_path, "wb");
  if (!file)
  {
    PanicAlertFmtT("Failed to open file \"{0}\". Verify your write permissions.", file_path);
    return false;
  }

  std::vector<u8> in_buffer(LZO_OUT_LEN);
  std::vector<u8> out_buffer(LZO_IN_LEN);

  while (true)
  {
    u32 cur_len = 0;       // number of bytes to read
    lzo_uint new_len = 0;  // number of bytes to write

    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    for (size_t j = 0; j < cur_len; j++)
    {
      packet >> in_buffer[j];
    }

    if (lzo1x_decompress(in_buffer.data(), cur_len, out_buffer.data(), &new_len, nullptr) !=
        LZO_E_OK)
    {
      PanicAlertFmtT("Internal LZO Error - decompression failed");
      return false;
    }

    if (!file.WriteBytes(out_buffer.data(), new_len))
    {
      PanicAlertFmtT("Error writing file: {0}", file_path);
      return false;
    }
  }

  return true;
}

static bool DecompressPacketIntoFolderInternal(sf::Packet& packet, const std::string& folder_path)
{
  if (!File::CreateFullPath(folder_path + "/"))
    return false;

  sf::Uint64 size;
  packet >> size;
  for (size_t i = 0; i < size; ++i)
  {
    std::string name;
    packet >> name;

    if (name.find('/') != std::string::npos)
      return false;
#ifdef _WIN32
    if (name.find('\\') != std::string::npos)
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
  u64 size = Common::PacketReadU64(packet);

  std::vector<u8> out_buffer(size);

  if (size == 0)
    return out_buffer;

  std::vector<u8> in_buffer(LZO_OUT_LEN);

  lzo_uint i = 0;
  while (true)
  {
    u32 cur_len = 0;       // number of bytes to read
    lzo_uint new_len = 0;  // number of bytes to write

    packet >> cur_len;
    if (!cur_len)
      break;  // We reached the end of the data stream

    for (size_t j = 0; j < cur_len; j++)
    {
      packet >> in_buffer[j];
    }

    if (lzo1x_decompress(in_buffer.data(), cur_len, &out_buffer[i], &new_len, nullptr) != LZO_E_OK)
    {
      PanicAlertFmtT("Internal LZO Error - decompression failed");
      return {};
    }

    i += new_len;
  }

  return out_buffer;
}
}  // namespace NetPlay
