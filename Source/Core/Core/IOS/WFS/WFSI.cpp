// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/WFS/WFSI.h"

#include <mbedtls/aes.h>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/WFS/WFSSRV.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
void ARCUnpacker::Reset()
{
  m_whole_file.clear();
}

void ARCUnpacker::AddBytes(const std::vector<u8>& bytes)
{
  m_whole_file.insert(m_whole_file.end(), bytes.begin(), bytes.end());
}

void ARCUnpacker::Extract(const WriteCallback& callback)
{
  u32 fourcc = Common::swap32(m_whole_file.data());
  if (fourcc != 0x55AA382D)
  {
    ERROR_LOG(IOS_WFS, "ARCUnpacker: invalid fourcc (%08x)", fourcc);
    return;
  }

  // Read the root node to get the number of nodes.
  u8* nodes_directory = m_whole_file.data() + 0x20;
  u32 nodes_count = Common::swap32(nodes_directory + 8);
  constexpr u32 NODE_SIZE = 0xC;
  char* string_table = reinterpret_cast<char*>(nodes_directory + nodes_count * NODE_SIZE);

  std::stack<std::pair<u32, std::string>> directory_stack;
  directory_stack.emplace(std::make_pair(nodes_count, ""));
  for (u32 i = 1; i < nodes_count; ++i)
  {
    while (i >= directory_stack.top().first)
    {
      directory_stack.pop();
    }
    const std::string& current_directory = directory_stack.top().second;
    u8* node = nodes_directory + i * NODE_SIZE;
    u32 name_offset = (node[1] << 16) | Common::swap16(node + 2);
    u32 data_offset = Common::swap32(node + 4);
    u32 size = Common::swap32(node + 8);
    std::string basename = string_table + name_offset;
    std::string fullname =
        current_directory.empty() ? basename : current_directory + "/" + basename;

    u8 flags = *node;
    if (flags == 1)
    {
      directory_stack.emplace(std::make_pair(size, fullname));
    }
    else
    {
      std::vector<u8> contents(m_whole_file.data() + data_offset,
                               m_whole_file.data() + data_offset + size);
      callback(fullname, contents);
    }
  }
}

namespace Device
{
WFSI::WFSI(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

IPCCommandResult WFSI::IOCtl(const IOCtlRequest& request)
{
  s32 return_error_code = IPC_SUCCESS;

  switch (request.request)
  {
  case IOCTL_WFSI_PREPARE_DEVICE:
  {
    u32 tmd_addr = Memory::Read_U32(request.buffer_in);
    u32 tmd_size = Memory::Read_U32(request.buffer_in + 4);

    INFO_LOG(IOS_WFS, "IOCTL_WFSI_PREPARE_DEVICE");

    constexpr u32 MAX_TMD_SIZE = 0x4000;
    if (tmd_size > MAX_TMD_SIZE)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFSI_PREPARE_DEVICE: TMD size too large (%d)", tmd_size);
      return_error_code = IPC_EINVAL;
      break;
    }
    std::vector<u8> tmd_bytes;
    tmd_bytes.resize(tmd_size);
    Memory::CopyFromEmu(tmd_bytes.data(), tmd_addr, tmd_size);
    m_tmd.SetBytes(std::move(tmd_bytes));

    IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(m_tmd.GetTitleId());
    if (!ticket.IsValid())
    {
      return_error_code = -11028;
      break;
    }

    memcpy(m_aes_key, ticket.GetTitleKey(m_ios.GetIOSC()).data(), sizeof(m_aes_key));
    mbedtls_aes_setkey_dec(&m_aes_ctx, m_aes_key, 128);

    break;
  }

  case IOCTL_WFSI_PREPARE_PROFILE:
    m_base_extract_path = StringFromFormat("/vol/%s/tmp/", m_device_name.c_str());
  // Fall through intended.

  case IOCTL_WFSI_PREPARE_CONTENT:
  {
    const char* ioctl_name = request.request == IOCTL_WFSI_PREPARE_PROFILE ?
                                 "IOCTL_WFSI_PREPARE_PROFILE" :
                                 "IOCTL_WFSI_PREPARE_CONTENT";

    // Initializes the IV from the index of the content in the TMD contents.
    u32 content_id = Memory::Read_U32(request.buffer_in + 8);
    IOS::ES::Content content_info;
    if (!m_tmd.FindContentById(content_id, &content_info))
    {
      WARN_LOG(IOS_WFS, "%s: Content id %08x not found", ioctl_name, content_id);
      return_error_code = -10003;
      break;
    }

    memset(m_aes_iv, 0, sizeof(m_aes_iv));
    m_aes_iv[0] = content_info.index >> 8;
    m_aes_iv[1] = content_info.index & 0xFF;
    INFO_LOG(IOS_WFS, "%s: Content id %08x found at index %d", ioctl_name, content_id,
             content_info.index);

    m_arc_unpacker.Reset();
    break;
  }

  case IOCTL_WFSI_IMPORT_PROFILE:
  case IOCTL_WFSI_IMPORT_CONTENT:
  {
    const char* ioctl_name = request.request == IOCTL_WFSI_IMPORT_PROFILE ?
                                 "IOCTL_WFSI_IMPORT_PROFILE" :
                                 "IOCTL_WFSI_IMPORT_CONTENT";

    u32 content_id = Memory::Read_U32(request.buffer_in + 0xC);
    u32 input_ptr = Memory::Read_U32(request.buffer_in + 0x10);
    u32 input_size = Memory::Read_U32(request.buffer_in + 0x14);
    INFO_LOG(IOS_WFS, "%s: %08x bytes of data at %08x from content id %d", ioctl_name, input_size,
             input_ptr, content_id);

    std::vector<u8> decrypted(input_size);
    mbedtls_aes_crypt_cbc(&m_aes_ctx, MBEDTLS_AES_DECRYPT, input_size, m_aes_iv,
                          Memory::GetPointer(input_ptr), decrypted.data());

    m_arc_unpacker.AddBytes(decrypted);
    break;
  }

  case IOCTL_WFSI_FINALIZE_PROFILE:
  case IOCTL_WFSI_FINALIZE_CONTENT:
  {
    const char* ioctl_name = request.request == IOCTL_WFSI_FINALIZE_PROFILE ?
                                 "IOCTL_WFSI_FINALIZE_PROFILE" :
                                 "IOCTL_WFSI_FINALIZE_CONTENT";
    INFO_LOG(IOS_WFS, "%s", ioctl_name);

    auto callback = [this](const std::string& filename, const std::vector<u8>& bytes) {
      INFO_LOG(IOS_WFS, "Extract: %s (%zd bytes)", filename.c_str(), bytes.size());

      std::string path = WFS::NativePath(m_base_extract_path + "/" + filename);
      File::CreateFullPath(path);
      File::IOFile f(path, "wb");
      if (!f)
      {
        ERROR_LOG(IOS_WFS, "Could not extract %s to %s", filename.c_str(), path.c_str());
        return;
      }
      f.WriteBytes(bytes.data(), bytes.size());
    };
    m_arc_unpacker.Extract(callback);

    // Technically not needed, but let's not keep large buffers in RAM for no
    // reason if we can avoid it.
    m_arc_unpacker.Reset();
    break;
  }

  case IOCTL_WFSI_DELETE_TITLE:
    // Bytes 0-4: ??
    // Bytes 4-8: game id
    // Bytes 1c-1e: title id?
    WARN_LOG(IOS_WFS, "IOCTL_WFSI_DELETE_TITLE: unimplemented");
    break;

  case IOCTL_WFSI_IMPORT_TITLE:
    WARN_LOG(IOS_WFS, "IOCTL_WFSI_IMPORT_TITLE: unimplemented");
    break;

  case IOCTL_WFSI_INIT:
  {
    INFO_LOG(IOS_WFS, "IOCTL_WFSI_INIT");
    if (GetIOS()->GetES()->GetTitleId(&m_title_id) < 0)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFSI_INIT: Could not get title id.");
      return_error_code = IPC_EINVAL;
      break;
    }
    m_title_id_str = StringFromFormat(
        "%c%c%c%c", static_cast<char>(m_title_id >> 24), static_cast<char>(m_title_id >> 16),
        static_cast<char>(m_title_id >> 8), static_cast<char>(m_title_id));

    IOS::ES::TMDReader tmd = GetIOS()->GetES()->FindInstalledTMD(m_title_id);
    m_group_id = tmd.GetGroupId();
    m_group_id_str = StringFromFormat("%c%c", m_group_id >> 8, m_group_id & 0xFF);
    break;
  }

  case IOCTL_WFSI_SET_DEVICE_NAME:
    INFO_LOG(IOS_WFS, "IOCTL_WFSI_SET_DEVICE_NAME");
    m_device_name = Memory::GetString(request.buffer_in);
    break;

  case IOCTL_WFSI_APPLY_TITLE_PROFILE:
    INFO_LOG(IOS_WFS, "IOCTL_WFSI_APPLY_TITLE_PROFILE");

    m_base_extract_path = StringFromFormat("/vol/%s/_install/%s/content", m_device_name.c_str(),
                                           m_title_id_str.c_str());
    File::CreateFullPath(WFS::NativePath(m_base_extract_path));

    break;

  case IOCTL_WFSI_LOAD_DOL:
  {
    std::string path = StringFromFormat("/vol/%s/title/%s/%s/content", m_device_name.c_str(),
                                        m_group_id_str.c_str(), m_title_id_str.c_str());

    u32 dol_addr = Memory::Read_U32(request.buffer_in + 0x18);
    u32 max_dol_size = Memory::Read_U32(request.buffer_in + 0x14);
    u16 dol_extension_id = Memory::Read_U16(request.buffer_in + 0x1e);

    if (dol_extension_id == 0)
    {
      path += "/default.dol";
    }
    else
    {
      path += StringFromFormat("/extension%d.dol", dol_extension_id);
    }

    INFO_LOG(IOS_WFS, "IOCTL_WFSI_LOAD_DOL: loading %s at address %08x (size %d)", path.c_str(),
             dol_addr, max_dol_size);

    File::IOFile fp(WFS::NativePath(path), "rb");
    if (!fp)
    {
      WARN_LOG(IOS_WFS, "IOCTL_WFSI_LOAD_DOL: no such file or directory: %s", path.c_str());
      return_error_code = WFSI_ENOENT;
      break;
    }

    u32 real_dol_size = fp.GetSize();
    if (dol_addr == 0)
    {
      // Write the expected size to the size parameter, in the input.
      Memory::Write_U32(real_dol_size, request.buffer_in + 0x14);
    }
    else
    {
      fp.ReadBytes(Memory::GetPointer(dol_addr), max_dol_size);
    }
    Memory::Write_U32(real_dol_size, request.buffer_out);
    break;
  }

  default:
    // TODO(wfs): Should be returning an error. However until we have
    // everything properly stubbed it's easier to simulate the methods
    // succeeding.
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS, LogTypes::LWARNING);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    break;
  }

  return GetDefaultReply(return_error_code);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
