// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayCommon.h"

#include <algorithm>

#include <fmt/format.h>
#include <zstd.h>

#include "Common/Buffer.h"
#include "Common/DirectIOFile.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/MsgHandler.h"
#include "Common/SFMLHelper.h"
#include "Common/ScopeGuard.h"

namespace NetPlay
{
bool CompressFileIntoPacket(const std::string& file_path, sf::Packet& packet)
{
  File::DirectIOFile file(file_path, File::AccessMode::Read);
  if (!file.IsOpen())
  {
    PanicAlertFmtT("Failed to open file \"{0}\".", file_path);
    return false;
  }

  const u64 file_size = file.GetSize();
  packet << file_size;

  if (file_size == 0)
    return true;

  auto* const zcs = ZSTD_createCStream();
  Common::ScopeGuard free_zcs{[&] { ZSTD_freeCStream(zcs); }};

  ZSTD_CCtx_setPledgedSrcSize(zcs, file_size);

  Common::UniqueBuffer<u8> in_buffer{ZSTD_CStreamInSize()};
  Common::UniqueBuffer<u8> out_buffer{ZSTD_CStreamOutSize()};

  for (u64 file_offset = 0; file_offset != file_size;)
  {
    ZSTD_inBuffer input{
        .src = in_buffer.data(),
        .size = std::min<size_t>(file_size - file_offset, in_buffer.size()),
    };

    if (!file.OffsetRead(file_offset, std::span{in_buffer}.first(input.size)))
    {
      PanicAlertFmtT("Error reading file: {0}", file_path);
      return false;
    }

    ZSTD_outBuffer output{.dst = out_buffer.data(), .size = out_buffer.size()};

    const size_t hint = ZSTD_compressStream(zcs, &output, &input);
    if (ZSTD_isError(hint))
    {
      PanicAlertFmtT("Internal Error - compression failed");
      return false;
    }

    packet.append(out_buffer.data(), output.pos);

    file_offset += input.pos;
  }

  while (true)
  {
    ZSTD_outBuffer output{.dst = out_buffer.data(), .size = out_buffer.size()};

    const size_t hint = ZSTD_endStream(zcs, &output);
    if (ZSTD_isError(hint))
    {
      PanicAlertFmtT("Internal Error - compression failed");
      return false;
    }

    packet.append(out_buffer.data(), output.pos);

    if (hint == 0)
      break;
  }

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

  Common::UniqueBuffer<u8> out_buffer{ZSTD_compressBound(in_buffer.size())};

  const size_t result = ZSTD_compress(out_buffer.data(), out_buffer.size(), in_buffer.data(),
                                      in_buffer.size(), ZSTD_CLEVEL_DEFAULT);
  if (ZSTD_isError(result))
  {
    PanicAlertFmtT("Internal Error - compression failed");
    return false;
  }

  packet.append(out_buffer.data(), result);
  return true;
}

bool DecompressPacketIntoFile(sf::Packet& packet, const std::string& file_path)
{
  const u64 file_size = Common::PacketReadU64(packet);
  if (file_size == 0)
    return true;

  auto* const zds = ZSTD_createDStream();
  Common::ScopeGuard free_zds{[&] { ZSTD_freeDStream(zds); }};

  File::DirectIOFile file(file_path, File::AccessMode::Write);
  if (!file.IsOpen())
  {
    PanicAlertFmtT("Failed to open file \"{0}\". Verify your write permissions.", file_path);
    return false;
  }

  // Avoid fragmentation.
  File::Resize(file, file_size);

  Common::UniqueBuffer<u8> out_buffer{ZSTD_DStreamOutSize()};

  ZSTD_inBuffer input{
      .src = packet.getData(),
      .size = +packet.getDataSize(),
      .pos = packet.getReadPosition(),
  };

  while (true)
  {
    ZSTD_outBuffer output{.dst = out_buffer.data(), .size = out_buffer.size()};

    const size_t hint = ZSTD_decompressStream(zds, &output, &input);

    // Bad data or ZSTD wants to read more bytes than we have.
    if (ZSTD_isError(hint) || (hint > (input.size - input.pos)))
    {
      PanicAlertFmtT("Internal Error - decompression failed");
      return {};
    }

    if (!file.Write(std::span{out_buffer}.first(output.pos)))
    {
      PanicAlertFmtT("Error writing file: {0}", file_path);
      return false;
    }

    if (hint == 0)
      break;
  }

  // Consume the bytes that we used from the packet.
  // TODO: Do this better when we no longer use sf::Packet.
  for (u8 dummy{}; packet.getReadPosition() != input.pos;)
    packet >> dummy;

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

  auto* const zds = ZSTD_createDStream();
  Common::ScopeGuard free_zds{[&] { ZSTD_freeDStream(zds); }};

  std::vector<u8> out_buffer(size);
  ZSTD_outBuffer output{.dst = out_buffer.data(), .size = out_buffer.size()};

  ZSTD_inBuffer input{
      .src = packet.getData(),
      .size = +packet.getDataSize(),
      .pos = packet.getReadPosition(),
  };

  while (true)
  {
    const size_t hint = ZSTD_decompressStream(zds, &output, &input);

    if (hint == 0)
      break;

    // Bad data or ZSTD wants to read/write more bytes than we expected.
    if (ZSTD_isError(hint) || (hint > (input.size - input.pos)) || (output.pos == output.size))
    {
      PanicAlertFmtT("Internal Error - decompression failed");
      return {};
    }
  }

  // Consume the bytes that we used from the packet.
  // TODO: Do this better when we no longer use sf::Packet.
  for (u8 dummy{}; packet.getReadPosition() != input.pos;)
    packet >> dummy;

  if (output.pos != size)
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
