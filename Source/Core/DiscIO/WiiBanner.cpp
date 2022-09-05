// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WiiBanner.h"

#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/IOS/WFS/WFSI.h"

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Volume.h"

#include "fastlz.h"

namespace DiscIO
{
constexpr u32 BANNER_WIDTH = 192;
constexpr u32 BANNER_HEIGHT = 64;
constexpr u32 BANNER_SIZE = BANNER_WIDTH * BANNER_HEIGHT * 2;

constexpr u32 ICON_WIDTH = 48;
constexpr u32 ICON_HEIGHT = 48;
constexpr u32 ICON_SIZE = ICON_WIDTH * ICON_HEIGHT * 2;

WiiBanner::WiiBanner(u64 title_id)
{
  m_path = (Common::GetTitleDataPath(title_id, Common::FROM_CONFIGURED_ROOT) + "/opening.bnr");
  ExtractARC(m_path);
}

WiiBanner::WiiBanner(const Volume& volume, Partition partition)
{
  m_valid = ExportFile(volume, partition, m_path, (File::CreateTempDir() + "/opening.bnr"));
  ExtractARC(m_path);
}

void WiiBanner::ExtractARC(std::string path)
{ 
  IOS::HLE::ARCUnpacker m_arc_unpacker;
  File::IOFile file(path, "rb");
  std::vector<unsigned char> bytes;
  u32 header;

  if (!file)
    m_valid = false;
  else if (file.GetSize() <= BANNER_SIZE * ICON_SIZE)
    m_valid = false;

  file.Seek(600, File::SeekOrigin::Begin);

  file.ReadBytes(&header, sizeof(header));
  if (header != 0x55AA382D) // "U.8-"
    m_valid = false;

  file.ReadBytes(&bytes, (file.GetSize() - 600));
  m_arc_unpacker.AddBytes(bytes);

  const std::string outdir = File::CreateTempDir();

  const auto callback = [=](const std::string& filename, const std::vector<u8>& outbytes)
  {
    const std::string outpath = (outdir + filename);
    File::CreateFullPath(outpath);
    File::IOFile outfile(outpath, "wb");
    outfile.WriteBytes(outbytes.data(), outbytes.size());
  };

  m_arc_unpacker.Extract(callback);

  ExtractIconBin(outdir + "/meta/icon.bin");
}

void WiiBanner::ExtractIconBin(std::string path)
{ 
  IOS::HLE::ARCUnpacker m_arc_unpacker;
  File::IOFile file(path, "rb");
  u32 header;

  if (!file)
    m_valid = false;
  else if (file.GetSize() <= BANNER_SIZE * ICON_SIZE)
    m_valid = false;

  file.Seek(20, File::SeekOrigin::Begin);

  file.ReadBytes(&header, sizeof(header));
  if (header == 0x4C5A3737) // "LZ77"
  {
    DecompressLZ77(path);
  }
  else if (header == 0x55AA382D) // "U.8-"
  {
    std::vector<unsigned char> bytes;

    file.ReadBytes(&bytes, (file.GetSize() - 20));
    m_arc_unpacker.AddBytes(bytes);

    const std::string outdir = File::CreateTempDir();

    const auto callback = [=](const std::string& filename, const std::vector<u8>& outbytes)
    {
      const std::string outpath = (outdir + filename);
      File::CreateFullPath(outpath);
      File::IOFile outfile(outpath, "wb");
      outfile.WriteBytes(outbytes.data(), outbytes.size());
    };

    m_arc_unpacker.Extract(callback);
  }
  else
  {
    m_valid = false;
  }
}

void WiiBanner::DecompressLZ77(std::string path)
{
  File::IOFile file(path, "rb");
  std::vector<unsigned char> bytes;
  std::vector<unsigned char> outbytes;

  file.Seek(20, File::SeekOrigin::Begin);
  file.ReadBytes(&bytes, (file.GetSize() - 20));

  if (!(fastlz_decompress(bytes.data(), bytes.size(), &outbytes, (bytes.size()*4))))
    m_valid = false;

  const std::string outpath = (File::CreateTempDir() + "lz77tmp.bin");
  File::IOFile outfile(outpath, "wb");
  outfile.WriteBytes(outbytes.data(), outbytes.size());
}

std::string WiiBanner::GetName() const
{
  // In the wii save banner.bin the name is from 0x20 to 0x3F
  // In the opening.bnr the name is SOMETIMES from 0x5C to 0x7B
  // of course the current extaction is also currently broken (see: the smurfs dance party)
  // but this wont be any more broken
  // ok well not exactly
  // it will be broken in a different way
  //return UTF16BEToUTF8(m_header.name, std::size(m_header.name));
  return std::string();
}

std::vector<u32> WiiBanner::GetBanner(u32* width, u32* height) const
{
  return std::vector<u32>();
  // *width = 0;
  // *height = 0;

  // File::IOFile file(m_path, "rb");
  // if (!file.Seek(sizeof(Header), File::SeekOrigin::Begin))
  //   return std::vector<u32>();

  // std::vector<u16> banner_data(BANNER_WIDTH * BANNER_HEIGHT);
  // if (!file.ReadArray(banner_data.data(), banner_data.size()))
  //   return std::vector<u32>();

  // std::vector<u32> image_buffer(BANNER_WIDTH * BANNER_HEIGHT);
  // Common::Decode5A3Image(image_buffer.data(), banner_data.data(), BANNER_WIDTH, BANNER_HEIGHT);

  // *width = BANNER_WIDTH;
  // *height = BANNER_HEIGHT;
  // return image_buffer;
}

// const auto callback = [this](const std::string& filename, const std::vector<u8>& bytes)
// {
//   INFO_LOG_FMT(IOS_WFS, "Extract: {} ({} bytes)", filename, bytes.size());

//   const std::string path = WFS::NativePath(m_base_extract_path + '/' + filename);
//   File::CreateFullPath(path);
//   File::IOFile f(path, "wb");
//   if (!f)
//   {
//     ERROR_LOG_FMT(IOS_WFS, "Could not extract {} to {}", filename, path);
//     return;
//   }
//   f.WriteBytes(bytes.data(), bytes.size());
// };
// m_arc_unpacker.Extract(callback);

// bool WiiBanner::ParseIMET(std::string path)
// {
//   constexpr u32 IMET_MAGIC = 0x494D4554;
//   u32 magic;

//   File::IOFile file(path, "rb");

//   if (file.GetSize() < (BANNER_SIZE + ICON_SIZE))
//     m_valid = false;

//   file.Seek(40, File::SeekOrigin::Begin);
//   file.ReadBytes(&magic, 4);
//   if (magic == IMET_MAGIC)
//     ParseArc(path);
//   else if (magic != IMET_MAGIC)
//     m_valid = false;

//   return 0;
// }

// bool WiiBanner::ParseARC(std::string path)
// {
//   U8Header header;
//   u32 magic;

//   File::IOFile file(path, "rb");

//   file.Seek(600, File::SeekOrigin::Current);
//   file.ReadBytes(&magic, 4);
//   if (magic != header.magic)
//     m_valid = false;


//   return 0;
// }

}  // namespace DiscIO
