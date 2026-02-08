// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/WFS/WFSI.h"

#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/EnumUtils.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/WFS/WFSSRV.h"
#include "Core/System.h"

namespace
{
std::string TitleIdStr(u64 tid)
{
  return fmt::format("{}{}{}{}", static_cast<char>(tid >> 24), static_cast<char>(tid >> 16),
                     static_cast<char>(tid >> 8), static_cast<char>(tid));
}

std::string GroupIdStr(u16 gid)
{
  return fmt::format("{}{}", static_cast<char>(gid >> 8), static_cast<char>(gid & 0xFF));
}
}  // namespace

namespace IOS::HLE
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
  const u32 fourcc = Common::swap32(m_whole_file.data());
  if (fourcc != 0x55AA382D)
  {
    ERROR_LOG_FMT(IOS_WFS, "ARCUnpacker: invalid fourcc ({:08x})", fourcc);
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

WFSIDevice::WFSIDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

void WFSIDevice::SetCurrentTitleIdAndGroupId(u64 tid, u16 gid)
{
  m_current_title_id = tid;
  m_current_group_id = gid;

  m_current_title_id_str = TitleIdStr(tid);
  m_current_group_id_str = GroupIdStr(gid);
}

void WFSIDevice::SetImportTitleIdAndGroupId(u64 tid, u16 gid)
{
  m_import_title_id = tid;
  m_import_group_id = gid;

  m_import_title_id_str = TitleIdStr(tid);
  m_import_group_id_str = GroupIdStr(gid);
}

void WFSIDevice::FinalizePatchInstall()
{
  const std::string current_title_dir = fmt::format("/vol/{}/title/{}/{}", m_device_name,
                                                    m_current_group_id_str, m_current_title_id_str);
  const std::string patch_dir = current_title_dir + "/_patch";
  File::MoveWithOverwrite(WFS::NativePath(patch_dir), WFS::NativePath(current_title_dir));
}

std::optional<IPCReply> WFSIDevice::IOCtl(const IOCtlRequest& request)
{
  s32 return_error_code = IPC_SUCCESS;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  switch (request.request)
  {
  case IOCTL_WFSI_IMPORT_TITLE_INIT:
  {
    const u32 tmd_addr = memory.Read_U32(request.buffer_in);
    const u32 tmd_size = memory.Read_U32(request.buffer_in + 4);

    m_patch_type = static_cast<PatchType>(memory.Read_U32(request.buffer_in + 32));
    m_continue_install = memory.Read_U32(request.buffer_in + 36);

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_IMPORT_TITLE_INIT: patch type {}, continue install: {}",
                 Common::ToUnderlying(m_patch_type), m_continue_install ? "true" : "false");

    if (m_patch_type == PatchType::PATCH_TYPE_2)
    {
      const std::string content_dir = fmt::format("/vol/{}/title/{}/{}/content", m_device_name,
                                                  m_current_group_id_str, m_current_title_id_str);

      File::Rename(WFS::NativePath(content_dir + "/default.dol"),
                   WFS::NativePath(content_dir + "/_default.dol"));
    }

    if (!ES::IsValidTMDSize(tmd_size))
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFSI_IMPORT_TITLE_INIT: TMD size too large ({})", tmd_size);
      return_error_code = IPC_EINVAL;
      break;
    }
    std::vector<u8> tmd_bytes;
    tmd_bytes.resize(tmd_size);
    memory.CopyFromEmu(tmd_bytes.data(), tmd_addr, tmd_size);
    m_tmd.SetBytes(std::move(tmd_bytes));

    const ES::TicketReader ticket =
        GetEmulationKernel().GetESCore().FindSignedTicket(m_tmd.GetTitleId());
    if (!ticket.IsValid())
    {
      return_error_code = -11028;
      break;
    }

    m_aes_ctx = Common::AES::CreateContextDecrypt(ticket.GetTitleKey(m_ios.GetIOSC()).data());

    SetImportTitleIdAndGroupId(m_tmd.GetTitleId(), m_tmd.GetGroupId());

    if (m_patch_type == PatchType::PATCH_TYPE_1)
      CancelPatchImport(m_continue_install);
    else if (m_patch_type == PatchType::NOT_A_PATCH)
      CancelTitleImport(m_continue_install);

    break;
  }

  case IOCTL_WFSI_PREPARE_PROFILE:
    m_base_extract_path = fmt::format("/vol/{}/tmp/", m_device_name);
    [[fallthrough]];

  case IOCTL_WFSI_PREPARE_CONTENT:
  {
    const char* ioctl_name = request.request == IOCTL_WFSI_PREPARE_PROFILE ?
                                 "IOCTL_WFSI_PREPARE_PROFILE" :
                                 "IOCTL_WFSI_PREPARE_CONTENT";

    // Initializes the IV from the index of the content in the TMD contents.
    const u32 content_id = memory.Read_U32(request.buffer_in + 8);
    ES::Content content_info;
    if (!m_tmd.FindContentById(content_id, &content_info))
    {
      WARN_LOG_FMT(IOS_WFS, "{}: Content id {:08x} not found", ioctl_name, content_id);
      return_error_code = -10003;
      break;
    }

    memset(m_aes_iv, 0, sizeof(m_aes_iv));
    m_aes_iv[0] = content_info.index >> 8;
    m_aes_iv[1] = content_info.index & 0xFF;
    INFO_LOG_FMT(IOS_WFS, "{}: Content id {:08x} found at index {}", ioctl_name, content_id,
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

    const u32 content_id = memory.Read_U32(request.buffer_in + 0xC);
    const u32 input_ptr = memory.Read_U32(request.buffer_in + 0x10);
    const u32 input_size = memory.Read_U32(request.buffer_in + 0x14);
    INFO_LOG_FMT(IOS_WFS, "{}: {:08x} bytes of data at {:08x} from content id {}", ioctl_name,
                 input_size, input_ptr, content_id);

    std::vector<u8> decrypted(input_size);
    m_aes_ctx->Crypt(m_aes_iv, m_aes_iv, memory.GetPointerForRange(input_ptr, input_size),
                     decrypted.data(), input_size);

    m_arc_unpacker.AddBytes(decrypted);
    break;
  }

  case IOCTL_WFSI_IMPORT_CONTENT_END:
  case IOCTL_WFSI_IMPORT_PROFILE_END:
  {
    const char* ioctl_name = request.request == IOCTL_WFSI_IMPORT_PROFILE_END ?
                                 "IOCTL_WFSI_IMPORT_PROFILE_END" :
                                 "IOCTL_WFSI_IMPORT_CONTENT_END";
    INFO_LOG_FMT(IOS_WFS, "{}", ioctl_name);

    const auto callback = [this](const std::string& filename, const std::vector<u8>& bytes) {
      INFO_LOG_FMT(IOS_WFS, "Extract: {} ({} bytes)", filename, bytes.size());

      const std::string path = WFS::NativePath(m_base_extract_path + '/' + filename);
      File::CreateFullPath(path);
      File::IOFile f(path, "wb");
      if (!f)
      {
        ERROR_LOG_FMT(IOS_WFS, "Could not extract {} to {}", filename, path);
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

  case IOCTL_WFSI_FINALIZE_TITLE_INSTALL:
  {
    std::string tmd_path;

    switch (m_patch_type)
    {
    case PATCH_TYPE_2:
    {
      // Delete content's default.dol
      const std::string title_content_dir =
          fmt::format("/vol/{}/title/{}/{}/content/", m_device_name, m_current_group_id_str,
                      m_current_title_id_str);
      File::Delete(WFS::NativePath(title_content_dir + "default.dol"));

      // Copy content's _default.dol to patch's directory
      const std::string patch_dir = fmt::format("/vol/{}/title/{}/{}/_patch/", m_device_name,
                                                m_current_group_id_str, m_current_title_id_str);
      const std::string patch_content_dir = patch_dir + "content/";
      File::CreateDir(WFS::NativePath(patch_dir));
      File::CreateDir(WFS::NativePath(patch_content_dir));
      File::Rename(WFS::NativePath(title_content_dir + "_default.dol"),
                   WFS::NativePath(patch_content_dir + "default.dol"));

      FinalizePatchInstall();
      [[fallthrough]];
    }
    case PATCH_TYPE_1:
    {
      const std::string patch_dir = fmt::format("/vol/{}/title/{}/{}/_patch", m_device_name,
                                                m_current_group_id_str, m_current_title_id_str);
      File::DeleteDirRecursively(WFS::NativePath(patch_dir));

      tmd_path = fmt::format("/vol/{}/title/{}/{}/meta/{:016x}.tmd", m_device_name,
                             m_current_group_id_str, m_current_title_id_str, m_import_title_id);
      break;
    }
    case NOT_A_PATCH:
    {
      const std::string title_install_dir =
          fmt::format("/vol/{}/_install/{}", m_device_name, m_import_title_id_str);
      const std::string title_final_dir = fmt::format("/vol/{}/title/{}/{}", m_device_name,
                                                      m_import_group_id_str, m_import_title_id_str);
      File::Rename(WFS::NativePath(title_install_dir), WFS::NativePath(title_final_dir));

      tmd_path = fmt::format("/vol/{}/title/{}/{}/meta/{:016x}.tmd", m_device_name,
                             m_import_group_id_str, m_import_title_id_str, m_import_title_id);
      break;
    }
    }

    File::IOFile tmd_file(WFS::NativePath(tmd_path), "wb");
    tmd_file.WriteBytes(m_tmd.GetBytes().data(), m_tmd.GetBytes().size());
    break;
  }

  case IOCTL_WFSI_FINALIZE_PATCH_INSTALL:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_FINALIZE_PATCH_INSTALL");
    if (m_patch_type != NOT_A_PATCH)
      FinalizePatchInstall();
    break;
  }

  case IOCTL_WFSI_DELETE_TITLE:
    // Bytes 0-4: ??
    // Bytes 4-8: game id
    // Bytes 1c-1e: title id?
    WARN_LOG_FMT(IOS_WFS, "IOCTL_WFSI_DELETE_TITLE: unimplemented");
    break;

  case IOCTL_WFSI_CHANGE_TITLE:
  {
    const u64 title_id = memory.Read_U64(request.buffer_in);
    const u16 group_id = memory.Read_U16(request.buffer_in + 0x1C);

    // TODO: Handle permissions
    SetCurrentTitleIdAndGroupId(title_id, group_id);

    // Change home directory
    const std::string homedir_path = fmt::format("/vol/{}/title/{}/{}/", m_device_name,
                                                 m_current_group_id_str, m_current_title_id_str);
    const u16 homedir_path_len = static_cast<u16>(homedir_path.size());
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_CHANGE_TITLE: {} (path_len: {:#x})", homedir_path,
                 homedir_path_len);

    return_error_code = -3;
    if (homedir_path_len > 0x1FD)
      break;
    auto device = GetEmulationKernel().GetDeviceByName("/dev/usb/wfssrv");
    if (!device)
      break;
    std::static_pointer_cast<WFSSRVDevice>(device)->SetHomeDir(homedir_path);
    return_error_code = IPC_SUCCESS;
    break;
  }

  case IOCTL_WFSI_GET_VERSION:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_GET_VERSION");
    memory.Write_U32(0x20, request.buffer_out);
    break;

  case IOCTL_WFSI_IMPORT_TITLE_CANCEL:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_IMPORT_TITLE_CANCEL");

    const bool continue_install = memory.Read_U32(request.buffer_in) != 0;
    if (m_patch_type == PatchType::NOT_A_PATCH)
      return_error_code = CancelTitleImport(continue_install);
    else if (m_patch_type == PatchType::PATCH_TYPE_1 || m_patch_type == PatchType::PATCH_TYPE_2)
      return_error_code = CancelPatchImport(continue_install);
    else
      return_error_code = WFS_EINVAL;

    m_tmd = {};
    break;
  }

  case IOCTL_WFSI_INIT:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_INIT");
    u64 tid;
    if (GetEmulationKernel().GetESCore().GetTitleId(&tid) < 0)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFSI_INIT: Could not get title id.");
      return_error_code = IPC_EINVAL;
      break;
    }

    const ES::TMDReader tmd = GetEmulationKernel().GetESCore().FindInstalledTMD(tid);
    SetCurrentTitleIdAndGroupId(tmd.GetTitleId(), tmd.GetGroupId());
    break;
  }

  case IOCTL_WFSI_SET_DEVICE_NAME:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_SET_DEVICE_NAME");
    m_device_name = memory.GetString(request.buffer_in);
    break;

  case IOCTL_WFSI_APPLY_TITLE_PROFILE:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_APPLY_TITLE_PROFILE");

    if (m_patch_type == NOT_A_PATCH)
    {
      const std::string install_directory = fmt::format("/vol/{}/_install", m_device_name);
      if (!m_continue_install && File::IsDirectory(WFS::NativePath(install_directory)))
      {
        File::DeleteDirRecursively(WFS::NativePath(install_directory));
      }

      m_base_extract_path = fmt::format("{}/{}/content", install_directory, m_import_title_id_str);
      File::CreateFullPath(WFS::NativePath(m_base_extract_path));
      File::CreateDir(WFS::NativePath(m_base_extract_path));

      for (const auto dir : {"work", "meta", "save"})
      {
        const std::string path =
            fmt::format("{}/{}/{}", install_directory, m_import_title_id_str, dir);
        File::CreateDir(WFS::NativePath(path));
      }

      const std::string group_path =
          fmt::format("/vol/{}/title/{}", m_device_name, m_import_group_id_str);
      File::CreateFullPath(WFS::NativePath(group_path));
      File::CreateDir(WFS::NativePath(group_path));
    }
    else
    {
      m_base_extract_path = fmt::format("/vol/{}/title/{}/{}/_patch/content", m_device_name,
                                        m_current_group_id_str, m_current_title_id_str);
      File::CreateFullPath(WFS::NativePath(m_base_extract_path));
      File::CreateDir(WFS::NativePath(m_base_extract_path));
    }

    break;
  }

  case IOCTL_WFSI_GET_TMD:
  {
    const u64 subtitle_id = memory.Read_U64(request.buffer_in);
    const u32 address = memory.Read_U32(request.buffer_in + 24);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_GET_TMD: subtitle ID {:016x}", subtitle_id);

    u32 tmd_size;
    return_error_code =
        GetTmd(m_current_group_id, m_current_title_id, subtitle_id, address, &tmd_size);
    memory.Write_U32(tmd_size, request.buffer_out);
    break;
  }

  case IOCTL_WFSI_GET_TMD_ABSOLUTE:
  {
    const u64 subtitle_id = memory.Read_U64(request.buffer_in);
    const u32 address = memory.Read_U32(request.buffer_in + 24);
    const u16 group_id = memory.Read_U16(request.buffer_in + 36);
    const u32 title_id = memory.Read_U32(request.buffer_in + 32);
    INFO_LOG_FMT(IOS_WFS,
                 "IOCTL_WFSI_GET_TMD_ABSOLUTE: tid {:08x}, gid {:04x}, subtitle ID {:016x}",
                 title_id, group_id, subtitle_id);

    u32 tmd_size;
    return_error_code = GetTmd(group_id, title_id, subtitle_id, address, &tmd_size);
    memory.Write_U32(tmd_size, request.buffer_out);
    break;
  }

  case IOCTL_WFSI_SET_FST_BUFFER:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_SET_FST_BUFFER: address {:08x}, size {:08x}",
                 request.buffer_in, request.buffer_in_size);
    break;
  }

  case IOCTL_WFSI_NOOP:
    break;

  case IOCTL_WFSI_LOAD_DOL:
  {
    std::string path = fmt::format("/vol/{}/title/{}/{}/content", m_device_name,
                                   m_current_group_id_str, m_current_title_id_str);

    const u32 dol_addr = memory.Read_U32(request.buffer_in + 0x18);
    const u32 max_dol_size = memory.Read_U32(request.buffer_in + 0x14);
    const u16 dol_extension_id = memory.Read_U16(request.buffer_in + 0x1e);

    if (dol_extension_id == 0)
    {
      path += "/default.dol";
    }
    else
    {
      path += fmt::format("/extension{}.dol", dol_extension_id);
    }

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFSI_LOAD_DOL: loading {} at address {:08x} (size {})", path,
                 dol_addr, max_dol_size);

    File::IOFile fp(WFS::NativePath(path), "rb");
    if (!fp)
    {
      WARN_LOG_FMT(IOS_WFS, "IOCTL_WFSI_LOAD_DOL: no such file or directory: {}", path);
      return_error_code = WFS_ENOENT;
      break;
    }

    const auto real_dol_size = static_cast<u32>(fp.GetSize());
    if (dol_addr == 0)
    {
      // Write the expected size to the size parameter, in the input.
      memory.Write_U32(real_dol_size, request.buffer_in + 0x14);
    }
    else
    {
      fp.ReadBytes(memory.GetPointerForRange(dol_addr, max_dol_size), max_dol_size);
    }
    memory.Write_U32(real_dol_size, request.buffer_out);
    break;
  }

  case IOCTL_WFSI_CHECK_HAS_SPACE:
    WARN_LOG_FMT(IOS_WFS, "IOCTL_WFSI_CHECK_HAS_SPACE: returning true");

    // TODO(wfs): implement this properly.
    //       1 is returned if there is free space, 0 otherwise.
    //
    // WFSI builds a path depending on the import state
    //   /vol/VOLUME_ID/title/GROUP_ID/GAME_ID
    //   /vol/VOLUME_ID/_install/GAME_ID
    // then removes everything after the last path separator ('/')
    // it then calls WFSISrvGetFreeBlkNum (ioctl 0x5a, aliased to 0x5b) with that path.
    // If the ioctl fails, WFSI returns 0.
    // If the ioctl succeeds, WFSI returns 0 or 1 depending on the three u32s in the input buffer
    // and the three u32s returned by WFSSRV (TODO: figure out what it does)
    return_error_code = 1;
    break;

  default:
    // TODO(wfs): Should be returning an error. However until we have
    // everything properly stubbed it's easier to simulate the methods
    // succeeding.
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_WFS,
                        Common::Log::LogLevel::LWARNING);
    memory.Memset(request.buffer_out, 0, request.buffer_out_size);
    break;
  }

  return IPCReply(return_error_code);
}

u32 WFSIDevice::GetTmd(u16 group_id, u32 title_id, u64 subtitle_id, u32 address, u32* size) const
{
  const std::string path = fmt::format("/vol/{}/title/{}/{}/meta/{:016x}.tmd", m_device_name,
                                       GroupIdStr(group_id), TitleIdStr(title_id), subtitle_id);
  File::IOFile fp(WFS::NativePath(path), "rb");
  if (!fp)
  {
    WARN_LOG_FMT(IOS_WFS, "GetTmd: no such file or directory: {}", path);
    return WFS_ENOENT;
  }
  *size = static_cast<u32>(fp.GetSize());
  if (address)
  {
    auto& system = GetSystem();
    auto& memory = system.GetMemory();
    fp.ReadBytes(memory.GetPointerForRange(address, *size), *size);
  }
  return IPC_SUCCESS;
}

static s32 DeleteTemporaryFiles(const std::string& device_name, u64 title_id)
{
  File::Delete(WFS::NativePath(fmt::format("/vol/{}/tmp/{:016x}.ini", device_name, title_id)));
  File::Delete(WFS::NativePath(fmt::format("/vol/{}/tmp/{:016x}.ppcini", device_name, title_id)));
  return IPC_SUCCESS;
}

s32 WFSIDevice::CancelTitleImport(bool continue_install)
{
  m_arc_unpacker.Reset();

  if (!continue_install)
  {
    File::DeleteDirRecursively(WFS::NativePath(fmt::format("/vol/{}/_install", m_device_name)));
  }

  DeleteTemporaryFiles(m_device_name, m_import_title_id);

  return IPC_SUCCESS;
}

s32 WFSIDevice::CancelPatchImport(bool continue_install)
{
  m_arc_unpacker.Reset();

  if (!continue_install)
  {
    File::DeleteDirRecursively(
        WFS::NativePath(fmt::format("/vol/{}/title/{}/{}/_patch", m_device_name,
                                    m_current_group_id_str, m_current_title_id_str)));

    if (m_patch_type == PatchType::PATCH_TYPE_2)
    {
      // Move back _default.dol to default.dol.
      const std::string content_dir = fmt::format("/vol/{}/title/{}/{}/content", m_device_name,
                                                  m_current_group_id_str, m_current_title_id_str);
      File::Rename(WFS::NativePath(content_dir + "/_default.dol"),
                   WFS::NativePath(content_dir + "/default.dol"));
    }
  }

  DeleteTemporaryFiles(m_device_name, m_current_title_id);

  return IPC_SUCCESS;
}
}  // namespace IOS::HLE
