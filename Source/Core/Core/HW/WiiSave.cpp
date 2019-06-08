// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Based off of tachtig/twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "Core/HW/WiiSave.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/ec.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Lazy.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/CommonTitles.h"
#include "Core/HW/WiiSaveStructs.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"

namespace WiiSave
{
using Md5 = std::array<u8, 0x10>;

constexpr std::array<u8, 0x10> s_sd_initial_iv{{0x21, 0x67, 0x12, 0xE6, 0xAA, 0x1F, 0x68, 0x9F,
                                                0x95, 0xC5, 0xA2, 0x23, 0x24, 0xDC, 0x6A, 0x98}};
constexpr Md5 s_md5_blanker{{0x0E, 0x65, 0x37, 0x81, 0x99, 0xBE, 0x45, 0x17, 0xAB, 0x06, 0xEC, 0x22,
                             0x45, 0x1A, 0x57, 0x93}};
constexpr u32 s_ng_id = 0x0403AC68;

void StorageDeleter::operator()(Storage* p) const
{
  delete p;
}

namespace FS = IOS::HLE::FS;

class NandStorage final : public Storage
{
public:
  explicit NandStorage(FS::FileSystem* fs, u64 tid) : m_fs{fs}, m_tid{tid}
  {
    m_data_dir = Common::GetTitleDataPath(tid);
    InitTitleUidAndGid();
    ScanForFiles(m_data_dir);
  }

  bool SaveExists() override
  {
    return m_uid && m_gid && m_fs->GetMetadata(*m_uid, *m_gid, m_data_dir + "/banner.bin");
  }

  std::optional<Header> ReadHeader() override
  {
    if (!m_uid || !m_gid)
      return {};

    const auto banner = m_fs->OpenFile(*m_uid, *m_gid, m_data_dir + "/banner.bin", FS::Mode::Read);
    if (!banner)
      return {};
    Header header{};
    header.banner_size = banner->GetStatus()->size;
    header.tid = m_tid;
    header.md5 = s_md5_blanker;
    const u8 mode = GetBinMode(m_data_dir + "/banner.bin");
    if (!mode || !banner->Read(header.banner, header.banner_size))
      return {};
    header.permissions = mode;
    // remove nocopy flag
    header.banner[7] &= ~1;

    Md5 md5_calc;
    mbedtls_md5_ret(reinterpret_cast<const u8*>(&header), sizeof(Header), md5_calc.data());
    header.md5 = std::move(md5_calc);
    return header;
  }

  std::optional<BkHeader> ReadBkHeader() override
  {
    BkHeader bk_hdr{};
    bk_hdr.size = BK_LISTED_SZ;
    bk_hdr.magic = BK_HDR_MAGIC;
    bk_hdr.ngid = s_ng_id;
    bk_hdr.number_of_files = static_cast<u32>(m_files_list.size());
    bk_hdr.size_of_files = m_files_size;
    bk_hdr.total_size = m_files_size + FULL_CERT_SZ;
    bk_hdr.tid = m_tid;
    return bk_hdr;
  }

  std::optional<std::vector<SaveFile>> ReadFiles() override { return m_files_list; }

  bool WriteHeader(const Header& header) override
  {
    if (!m_uid || !m_gid)
      return false;

    const std::string banner_file_path = m_data_dir + "/banner.bin";
    const FS::Modes modes = GetFsMode(header.permissions);
    const auto file = m_fs->CreateAndOpenFile(*m_uid, *m_gid, banner_file_path, modes);
    return file && file->Write(header.banner, header.banner_size);
  }

  bool WriteBkHeader(const BkHeader& bk_header) override { return true; }

  bool WriteFiles(const std::vector<SaveFile>& files) override
  {
    if (!m_uid || !m_gid)
      return false;

    for (const SaveFile& file : files)
    {
      const FS::Modes modes = GetFsMode(file.mode);
      const std::string path = m_data_dir + '/' + file.path;
      if (file.type == SaveFile::Type::File)
      {
        const auto raw_file = m_fs->CreateAndOpenFile(*m_uid, *m_gid, path, modes);
        const std::optional<std::vector<u8>>& data = *file.data;
        if (!data || !raw_file || !raw_file->Write(data->data(), data->size()))
          return false;
      }
      else if (file.type == SaveFile::Type::Directory)
      {
        const FS::Result<FS::Metadata> meta = m_fs->GetMetadata(*m_uid, *m_gid, path);
        if (meta && meta->is_file)
          return false;

        const FS::ResultCode result = m_fs->CreateDirectory(*m_uid, *m_gid, path, 0, modes);
        if (result != FS::ResultCode::Success)
          return false;
      }
    }
    return true;
  }

private:
  void ScanForFiles(const std::string& dir)
  {
    if (!m_uid || !m_gid)
      return;

    const auto entries = m_fs->ReadDirectory(*m_uid, *m_gid, dir);
    if (!entries)
      return;

    for (const std::string& elem : *entries)
    {
      if (elem == "banner.bin")
        continue;

      const std::string path = dir + '/' + elem;
      const FS::Result<FS::Metadata> metadata = m_fs->GetMetadata(*m_uid, *m_gid, path);
      if (!metadata)
        return;

      SaveFile save_file;
      save_file.mode = GetBinMode(metadata->modes);
      save_file.attributes = 0;
      save_file.type = metadata->is_file ? SaveFile::Type::File : SaveFile::Type::Directory;
      save_file.path = path.substr(m_data_dir.size() + 1);
      save_file.data = [this, path]() -> std::optional<std::vector<u8>> {
        const auto file = m_fs->OpenFile(*m_uid, *m_gid, path, FS::Mode::Read);
        if (!file)
          return {};
        std::vector<u8> data(file->GetStatus()->size);
        if (!file->Read(data.data(), data.size()))
          return std::nullopt;
        return data;
      };
      m_files_list.emplace_back(std::move(save_file));
      m_files_size += sizeof(FileHDR);

      if (metadata->is_file)
        m_files_size += static_cast<u32>(Common::AlignUp(metadata->size, BLOCK_SZ));
      else
        ScanForFiles(path);
    }
  }

  void InitTitleUidAndGid()
  {
    const auto metadata = m_fs->GetMetadata(IOS::PID_KERNEL, IOS::PID_KERNEL, m_data_dir);
    if (!metadata)
      return;
    m_uid = metadata->uid;
    m_gid = metadata->gid;
  }

  static constexpr FS::Modes GetFsMode(u8 bin_mode)
  {
    return {FS::Mode(bin_mode >> 4 & 3), FS::Mode(bin_mode >> 2 & 3), FS::Mode(bin_mode >> 0 & 3)};
  }

  static constexpr u8 GetBinMode(const FS::Modes& modes)
  {
    return u8(modes.owner) << 4 | u8(modes.group) << 2 | u8(modes.other) << 0;
  }

  u8 GetBinMode(const std::string& path) const
  {
    if (const FS::Result<FS::Metadata> meta = m_fs->GetMetadata(*m_uid, *m_gid, path))
      return GetBinMode(meta->modes);
    return 0;
  }

  FS::FileSystem* m_fs = nullptr;
  std::string m_data_dir;
  u64 m_tid = 0;
  std::optional<u32> m_uid;
  std::optional<u16> m_gid;
  std::vector<SaveFile> m_files_list;
  u32 m_files_size = 0;
};

class DataBinStorage final : public Storage
{
public:
  explicit DataBinStorage(IOS::HLE::IOSC* iosc, const std::string& path, const char* mode)
      : m_iosc{*iosc}
  {
    File::CreateFullPath(path);
    m_file = File::IOFile{path, mode};
  }

  std::optional<Header> ReadHeader() override
  {
    Header header;
    if (!m_file.Seek(0, SEEK_SET) || !m_file.ReadArray(&header, 1))
      return {};

    std::array<u8, 0x10> iv = s_sd_initial_iv;
    m_iosc.Decrypt(IOS::HLE::IOSC::HANDLE_SD_KEY, iv.data(), reinterpret_cast<const u8*>(&header),
                   sizeof(Header), reinterpret_cast<u8*>(&header), IOS::PID_ES);
    u32 banner_size = header.banner_size;
    if ((banner_size < FULL_BNR_MIN) || (banner_size > FULL_BNR_MAX) ||
        (((banner_size - BNR_SZ) % ICON_SZ) != 0))
    {
      ERROR_LOG(CONSOLE, "Not a Wii save or read failure for file header size %x", banner_size);
      return {};
    }

    Md5 md5_file = header.md5;
    header.md5 = s_md5_blanker;
    Md5 md5_calc;
    mbedtls_md5_ret(reinterpret_cast<const u8*>(&header), sizeof(Header), md5_calc.data());
    if (md5_file != md5_calc)
    {
      ERROR_LOG(CONSOLE, "MD5 mismatch\n %016" PRIx64 "%016" PRIx64 " != %016" PRIx64 "%016" PRIx64,
                Common::swap64(md5_file.data()), Common::swap64(md5_file.data() + 8),
                Common::swap64(md5_calc.data()), Common::swap64(md5_calc.data() + 8));
      return {};
    }
    return header;
  }

  std::optional<BkHeader> ReadBkHeader() override
  {
    BkHeader bk_header;
    m_file.Seek(sizeof(Header), SEEK_SET);
    if (!m_file.ReadArray(&bk_header, 1))
      return {};
    if (bk_header.size != BK_LISTED_SZ || bk_header.magic != BK_HDR_MAGIC)
      return {};
    if (bk_header.size_of_files + FULL_CERT_SZ != bk_header.total_size)
      return {};
    return bk_header;
  }

  std::optional<std::vector<SaveFile>> ReadFiles() override
  {
    const std::optional<BkHeader> bk_header = ReadBkHeader();
    if (!bk_header || !m_file.Seek(sizeof(Header) + sizeof(BkHeader), SEEK_SET))
      return {};

    std::vector<SaveFile> files;
    for (u32 i = 0; i < bk_header->number_of_files; ++i)
    {
      SaveFile save_file;
      FileHDR file_hdr;
      if (!m_file.ReadArray(&file_hdr, 1) || file_hdr.magic != FILE_HDR_MAGIC)
        return {};

      save_file.mode = file_hdr.permissions;
      save_file.attributes = file_hdr.attrib;
      const SaveFile::Type type = static_cast<SaveFile::Type>(file_hdr.type);
      if (type != SaveFile::Type::Directory && type != SaveFile::Type::File)
        return {};
      save_file.type = type;
      save_file.path =
          std::string{file_hdr.name.data(), strnlen(file_hdr.name.data(), file_hdr.name.size())};
      if (type == SaveFile::Type::File)
      {
        const u32 size = file_hdr.size;
        const u32 rounded_size = Common::AlignUp<u32>(size, BLOCK_SZ);
        const u64 pos = m_file.Tell();
        std::array<u8, 0x10> iv = file_hdr.iv;

        save_file.data = [this, size, rounded_size, iv,
                          pos]() mutable -> std::optional<std::vector<u8>> {
          std::vector<u8> file_data(rounded_size);
          if (!m_file.Seek(pos, SEEK_SET) || !m_file.ReadBytes(file_data.data(), rounded_size))
            return {};

          m_iosc.Decrypt(IOS::HLE::IOSC::HANDLE_SD_KEY, iv.data(), file_data.data(), rounded_size,
                         file_data.data(), IOS::PID_ES);
          file_data.resize(size);
          return file_data;
        };
        m_file.Seek(pos + rounded_size, SEEK_SET);
      }
      files.emplace_back(std::move(save_file));
    }
    return files;
  }

  bool WriteHeader(const Header& header) override
  {
    Header encrypted_header;
    std::array<u8, 0x10> iv = s_sd_initial_iv;
    m_iosc.Encrypt(IOS::HLE::IOSC::HANDLE_SD_KEY, iv.data(), reinterpret_cast<const u8*>(&header),
                   sizeof(Header), reinterpret_cast<u8*>(&encrypted_header), IOS::PID_ES);
    return m_file.Seek(0, SEEK_SET) && m_file.WriteArray(&encrypted_header, 1);
  }

  bool WriteBkHeader(const BkHeader& bk_header) override
  {
    return m_file.Seek(sizeof(Header), SEEK_SET) && m_file.WriteArray(&bk_header, 1);
  }

  bool WriteFiles(const std::vector<SaveFile>& files) override
  {
    if (!m_file.Seek(sizeof(Header) + sizeof(BkHeader), SEEK_SET))
      return false;

    for (const SaveFile& save_file : files)
    {
      FileHDR file_hdr{};
      file_hdr.magic = FILE_HDR_MAGIC;
      file_hdr.permissions = save_file.mode;
      file_hdr.attrib = save_file.attributes;
      file_hdr.type = static_cast<u8>(save_file.type);
      if (save_file.path.length() > file_hdr.name.size())
        return false;
      std::strncpy(file_hdr.name.data(), save_file.path.data(), file_hdr.name.size());

      std::optional<std::vector<u8>> data;
      if (file_hdr.type == 1)
      {
        data = *save_file.data;
        if (!data)
          return false;
        file_hdr.size = static_cast<u32>(data->size());
      }

      if (!m_file.WriteArray(&file_hdr, 1))
        return false;

      if (data)
      {
        std::vector<u8> file_data_enc(Common::AlignUp(data->size(), BLOCK_SZ));
        std::copy(data->cbegin(), data->cend(), file_data_enc.begin());
        m_iosc.Encrypt(IOS::HLE::IOSC::HANDLE_SD_KEY, file_hdr.iv.data(), file_data_enc.data(),
                       file_data_enc.size(), file_data_enc.data(), IOS::PID_ES);
        if (!m_file.WriteBytes(file_data_enc.data(), file_data_enc.size()))
          return false;
      }
    }

    if (!WriteSignatures())
    {
      ERROR_LOG(CORE, "WiiSave::WriteFiles: Failed to write signatures");
      return false;
    }
    return true;
  }

private:
  bool WriteSignatures()
  {
    const std::optional<BkHeader> bk_header = ReadBkHeader();
    if (!bk_header)
      return false;

    // Read data to sign.
    std::array<u8, 20> data_sha1;
    {
      const u32 data_size = bk_header->size_of_files + sizeof(BkHeader);
      auto data = std::make_unique<u8[]>(data_size);
      m_file.Seek(sizeof(Header), SEEK_SET);
      if (!m_file.ReadBytes(data.get(), data_size))
        return false;
      mbedtls_sha1_ret(data.get(), data_size, data_sha1.data());
    }

    // Sign the data.
    IOS::CertECC ap_cert;
    Common::ec::Signature ap_sig;
    m_iosc.Sign(ap_sig.data(), reinterpret_cast<u8*>(&ap_cert), Titles::SYSTEM_MENU,
                data_sha1.data(), static_cast<u32>(data_sha1.size()));

    // Write signatures.
    if (!m_file.Seek(0, SEEK_END))
      return false;
    const u32 SIGNATURE_END_MAGIC = Common::swap32(0x2f536969);
    const IOS::CertECC device_certificate = m_iosc.GetDeviceCertificate();
    return m_file.WriteArray(ap_sig.data(), ap_sig.size()) &&
           m_file.WriteArray(&SIGNATURE_END_MAGIC, 1) &&
           m_file.WriteArray(&device_certificate, 1) && m_file.WriteArray(&ap_cert, 1);
  }

  IOS::HLE::IOSC& m_iosc;
  File::IOFile m_file;
};

StoragePointer MakeNandStorage(FS::FileSystem* fs, u64 tid)
{
  return StoragePointer{new NandStorage{fs, tid}};
}

StoragePointer MakeDataBinStorage(IOS::HLE::IOSC* iosc, const std::string& path, const char* mode)
{
  return StoragePointer{new DataBinStorage{iosc, path, mode}};
}

template <typename T>
static bool Copy(const char* description, Storage* source, std::optional<T> (Storage::*read_fn)(),
                 Storage* dest, bool (Storage::*write_fn)(const T&))
{
  const std::optional<T> data = (source->*read_fn)();
  if (data && (dest->*write_fn)(*data))
    return true;
  ERROR_LOG(CORE, "WiiSave::Copy: Failed to %s %s", !data ? "read" : "write", description);
  return false;
}

bool Copy(Storage* source, Storage* dest)
{
  return Copy("header", source, &Storage::ReadHeader, dest, &Storage::WriteHeader) &&
         Copy("bk header", source, &Storage::ReadBkHeader, dest, &Storage::WriteBkHeader) &&
         Copy("files", source, &Storage::ReadFiles, dest, &Storage::WriteFiles);
}

bool Import(const std::string& data_bin_path, std::function<bool()> can_overwrite)
{
  IOS::HLE::Kernel ios;
  const auto data_bin = MakeDataBinStorage(&ios.GetIOSC(), data_bin_path, "rb");
  const std::optional<Header> header = data_bin->ReadHeader();
  if (!header)
  {
    ERROR_LOG(CORE, "WiiSave::Import: Failed to read header");
    return false;
  }
  const auto nand = MakeNandStorage(ios.GetFS().get(), header->tid);
  if (nand->SaveExists() && !can_overwrite())
    return false;
  return Copy(data_bin.get(), nand.get());
}

static bool Export(u64 tid, const std::string& export_path, IOS::HLE::Kernel* ios)
{
  std::string path = StringFromFormat("%s/private/wii/title/%c%c%c%c/data.bin", export_path.c_str(),
                                      static_cast<char>(tid >> 24), static_cast<char>(tid >> 16),
                                      static_cast<char>(tid >> 8), static_cast<char>(tid));
  return Copy(MakeNandStorage(ios->GetFS().get(), tid).get(),
              MakeDataBinStorage(&ios->GetIOSC(), path, "w+b").get());
}

bool Export(u64 tid, const std::string& export_path)
{
  IOS::HLE::Kernel ios;
  return Export(tid, export_path, &ios);
}

size_t ExportAll(const std::string& export_path)
{
  IOS::HLE::Kernel ios;
  size_t exported_save_count = 0;
  for (const u64 title : ios.GetES()->GetInstalledTitles())
  {
    if (Export(title, export_path, &ios))
      ++exported_save_count;
  }
  return exported_save_count;
}
}  // namespace WiiSave
