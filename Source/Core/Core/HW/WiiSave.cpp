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
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
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

enum
{
  BLOCK_SZ = 0x40,
  ICON_SZ = 0x1200,
  BNR_SZ = 0x60a0,
  FULL_BNR_MIN = 0x72a0,  // BNR_SZ + 1*ICON_SZ
  FULL_BNR_MAX = 0xF0A0,  // BNR_SZ + 8*ICON_SZ
  BK_LISTED_SZ = 0x70,    // Size before rounding to nearest block
  SIG_SZ = 0x40,
  FULL_CERT_SZ = 0x3C0,  // SIG_SZ + NG_CERT_SZ + AP_CERT_SZ + 0x80?

  BK_HDR_MAGIC = 0x426B0001,
  FILE_HDR_MAGIC = 0x03adf17e
};

#pragma pack(push, 1)
struct DataBinHeader
{
  Common::BigEndianValue<u64> tid;
  Common::BigEndianValue<u32> banner_size;  // (0x72A0 or 0xF0A0, also seen 0xBAA0)
  u8 permissions;
  u8 unk1;                   // maybe permissions is a be16
  std::array<u8, 0x10> md5;  // md5 of plaintext header with md5 blanker applied
  Common::BigEndianValue<u16> unk2;
};

struct Header
{
  DataBinHeader hdr;
  u8 banner[FULL_BNR_MAX];
};

struct BkHeader
{
  Common::BigEndianValue<u32> size;  // 0x00000070
  // u16 magic;  // 'Bk'
  // u16 magic2; // or version (0x0001)
  Common::BigEndianValue<u32> magic;  // 0x426B0001
  Common::BigEndianValue<u32> ngid;
  Common::BigEndianValue<u32> number_of_files;
  Common::BigEndianValue<u32> size_of_files;
  Common::BigEndianValue<u32> unk1;
  Common::BigEndianValue<u32> unk2;
  Common::BigEndianValue<u32> total_size;
  std::array<u8, 64> unk3;
  Common::BigEndianValue<u64> tid;
  std::array<u8, 6> mac_address;
  std::array<u8, 0x12> padding;
};

struct FileHDR
{
  Common::BigEndianValue<u32> magic;  // 0x03adf17e
  Common::BigEndianValue<u32> size;
  u8 permissions;
  u8 attrib;
  u8 type;  // (1=file, 2=directory)
  std::array<char, 0x45> name;
  std::array<u8, 0x10> iv;
  std::array<u8, 0x20> unk;
};
#pragma pack(pop)

class Storage
{
public:
  struct SaveFile
  {
    u8 mode, attributes, type;
    std::string path;
    // Only valid for regular (i.e. non-directory) files.
    Common::Lazy<std::optional<std::vector<u8>>> data;
  };

  virtual ~Storage() = default;
  virtual bool SaveExists() { return true; }
  virtual std::optional<Header> ReadHeader() = 0;
  virtual std::optional<BkHeader> ReadBkHeader() = 0;
  virtual std::optional<std::vector<SaveFile>> ReadFiles() = 0;
  virtual bool WriteHeader(const Header& header) = 0;
  virtual bool WriteBkHeader(const BkHeader& bk_header) = 0;
  virtual bool WriteFiles(const std::vector<SaveFile>& files) = 0;
};

void StorageDeleter::operator()(Storage* p) const
{
  delete p;
}

class NandStorage final : public Storage
{
public:
  explicit NandStorage(u64 tid) : m_tid{tid}
  {
    m_wii_title_path = Common::GetTitleDataPath(tid, Common::FromWhichRoot::FROM_CONFIGURED_ROOT);
    File::CreateFullPath(m_wii_title_path);
    ScanForFiles();
  }

  bool SaveExists() override { return File::Exists(m_wii_title_path + "/banner.bin"); }

  std::optional<Header> ReadHeader() override
  {
    Header header{};
    std::string banner_file_path = m_wii_title_path + "/banner.bin";
    u32 banner_size = static_cast<u32>(File::GetSize(banner_file_path));
    header.hdr.banner_size = banner_size;
    header.hdr.tid = m_tid;
    header.hdr.md5 = s_md5_blanker;
    header.hdr.permissions = 0x3C;

    File::IOFile banner_file(banner_file_path, "rb");
    if (!banner_file.ReadBytes(header.banner, banner_size))
      return {};
    // remove nocopy flag
    header.banner[7] &= ~1;

    Md5 md5_calc;
    mbedtls_md5(reinterpret_cast<const u8*>(&header), sizeof(Header), md5_calc.data());
    header.hdr.md5 = std::move(md5_calc);
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

  std::optional<std::vector<SaveFile>> ReadFiles() override
  {
    std::vector<SaveFile> ret(m_files_list.size());
    std::transform(m_files_list.begin(), m_files_list.end(), ret.begin(), [this](const auto& path) {
      const File::FileInfo file_info{path};
      SaveFile save_file;
      save_file.mode = 0x3c;
      save_file.attributes = 0;
      save_file.type = file_info.IsDirectory() ? 2 : 1;
      save_file.path = Common::UnescapeFileName(path.substr(m_wii_title_path.length() + 1));
      save_file.data = [path]() -> std::optional<std::vector<u8>> {
        File::IOFile file{path, "rb"};
        std::vector<u8> data(file.GetSize());
        if (!file || !file.ReadBytes(data.data(), data.size()))
          return std::nullopt;
        return data;
      };
      return save_file;
    });
    return ret;
  }

  bool WriteHeader(const Header& header) override
  {
    File::IOFile banner_file(m_wii_title_path + "/banner.bin", "wb");
    return banner_file.WriteBytes(header.banner, header.hdr.banner_size);
  }

  bool WriteBkHeader(const BkHeader& bk_header) override { return true; }

  bool WriteFiles(const std::vector<SaveFile>& files) override
  {
    for (const SaveFile& file : files)
    {
      // Allows files in subfolders to be escaped properly (ex: "nocopy/data00")
      // Special characters in path components will be escaped such as /../
      std::string file_path = Common::EscapePath(file.path);
      std::string file_path_full = m_wii_title_path + '/' + file_path;
      File::CreateFullPath(file_path_full);

      if (file.type == 1)
      {
        File::IOFile raw_save_file(file_path_full, "wb");
        const std::optional<std::vector<u8>>& data = *file.data;
        if (!data)
          return false;
        raw_save_file.WriteBytes(data->data(), data->size());
      }
      else if (file.type == 2)
      {
        File::CreateDir(file_path_full);
        if (!File::IsDirectory(file_path_full))
          return false;
      }
    }
    return true;
  }

private:
  void ScanForFiles()
  {
    std::vector<std::string> directories;
    directories.push_back(m_wii_title_path);
    u32 size = 0;

    for (u32 i = 0; i < directories.size(); ++i)
    {
      if (i != 0)
      {
        // add dir to fst
        m_files_list.push_back(directories[i]);
      }

      File::FSTEntry fst_tmp = File::ScanDirectoryTree(directories[i], false);
      for (const File::FSTEntry& elem : fst_tmp.children)
      {
        if (elem.virtualName != "banner.bin")
        {
          size += sizeof(FileHDR);
          if (elem.isDirectory)
          {
            if (elem.virtualName == "nocopy" || elem.virtualName == "nomove")
            {
              NOTICE_LOG(CONSOLE,
                         "This save will likely require homebrew tools to copy to a real Wii.");
            }

            directories.push_back(elem.physicalName);
          }
          else
          {
            m_files_list.push_back(elem.physicalName);
            size += static_cast<u32>(Common::AlignUp(elem.size, BLOCK_SZ));
          }
        }
      }
    }
    m_files_size = size;
  }

  u64 m_tid = 0;
  std::string m_wii_title_path;
  std::vector<std::string> m_files_list;
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
    u32 banner_size = header.hdr.banner_size;
    if ((banner_size < FULL_BNR_MIN) || (banner_size > FULL_BNR_MAX) ||
        (((banner_size - BNR_SZ) % ICON_SZ) != 0))
    {
      ERROR_LOG(CONSOLE, "Not a Wii save or read failure for file header size %x", banner_size);
      return {};
    }

    Md5 md5_file = header.hdr.md5;
    header.hdr.md5 = s_md5_blanker;
    Md5 md5_calc;
    mbedtls_md5(reinterpret_cast<const u8*>(&header), sizeof(Header), md5_calc.data());
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
      save_file.type = file_hdr.type;
      save_file.path = file_hdr.name.data();
      if (file_hdr.type == 1)
      {
        const u32 rounded_size = Common::AlignUp<u32>(file_hdr.size, BLOCK_SZ);
        const u64 pos = m_file.Tell();
        std::array<u8, 0x10> iv = file_hdr.iv;

        save_file.data = [this, rounded_size, iv, pos]() mutable -> std::optional<std::vector<u8>> {
          std::vector<u8> file_data(rounded_size);
          if (!m_file.Seek(pos, SEEK_SET) || !m_file.ReadBytes(file_data.data(), rounded_size))
            return {};

          m_iosc.Decrypt(IOS::HLE::IOSC::HANDLE_SD_KEY, iv.data(), file_data.data(), rounded_size,
                         file_data.data(), IOS::PID_ES);
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
      file_hdr.type = save_file.type;
      if (save_file.path.length() > 0x44)
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
    const u32 data_size = bk_header->size_of_files + 0x80;
    auto data = std::make_unique<u8[]>(data_size);
    m_file.Seek(0xf0c0, SEEK_SET);
    if (!m_file.ReadBytes(data.get(), data_size))
      return false;

    // Sign the data.
    IOS::CertECC ap_cert;
    Common::ec::Signature ap_sig;
    m_iosc.Sign(ap_sig.data(), reinterpret_cast<u8*>(&ap_cert), Titles::SYSTEM_MENU, data.get(),
                data_size);

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

StoragePointer MakeNandStorage(IOS::HLE::FS::FileSystem* fs, u64 tid)
{
  // fs parameter is not used yet but will be after WiiSave is migrated to the new FS interface.
  return StoragePointer{new NandStorage{tid}};
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

bool Import(const std::string& data_bin_path)
{
  IOS::HLE::Kernel ios;
  const auto data_bin = MakeDataBinStorage(&ios.GetIOSC(), data_bin_path, "rb");
  const std::optional<Header> header = data_bin->ReadHeader();
  if (!header)
  {
    ERROR_LOG(CORE, "WiiSave::Import: Failed to read header");
    return false;
  }
  const auto nand = MakeNandStorage(ios.GetFS().get(), header->hdr.tid);
  if (nand->SaveExists() && !AskYesNoT("Save data for this title already exists. Consider backing "
                                       "up the current data before overwriting.\nOverwrite now?"))
  {
    return false;
  }
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
