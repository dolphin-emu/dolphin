// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "FileByHandle.h"

#include <memory>

#include <Windows.h>

#include "StringUtil.h"

namespace File
{
struct IOFileByHandle::Impl
{
  HANDLE m_handle;
};

IOFileByHandle::IOFileByHandle(const std::string& filename) : m_data(std::make_unique<Impl>())
{
  auto wide_filename = UTF8ToTStr(filename);
  m_data->m_handle = CreateFile(wide_filename.c_str(), GENERIC_READ | GENERIC_WRITE | DELETE, 0,
                                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, nullptr);
  if (m_data->m_handle == INVALID_HANDLE_VALUE)
    return;
}

IOFileByHandle::~IOFileByHandle()
{
  if (m_data->m_handle == INVALID_HANDLE_VALUE)
    return;

  CloseHandle(m_data->m_handle);
}

bool IOFileByHandle::Write(const u8* data, size_t length)
{
  if (m_data->m_handle == INVALID_HANDLE_VALUE)
    return false;

  DWORD bytes_written;
  if (WriteFile(m_data->m_handle, data, static_cast<DWORD>(length), &bytes_written, nullptr))
    return bytes_written == length;

  return false;
}

bool IOFileByHandle::Rename(const std::string& filename)
{
  if (m_data->m_handle == INVALID_HANDLE_VALUE)
    return false;

  auto wide_filename = UTF8ToWString(filename);
  size_t rename_info_length = sizeof(FILE_RENAME_INFO) + wide_filename.length() * sizeof(wchar_t);
  auto rename_info_buffer = std::make_unique<u8[]>(rename_info_length);

  FILE_RENAME_INFO* rename_info = reinterpret_cast<FILE_RENAME_INFO*>(rename_info_buffer.get());
  rename_info->ReplaceIfExists = TRUE;
  rename_info->FileNameLength = static_cast<DWORD>(wide_filename.length());
  std::memcpy(rename_info->FileName, wide_filename.c_str(),
              wide_filename.length() * sizeof(wchar_t));

  if (SetFileInformationByHandle(m_data->m_handle, FileRenameInfo, rename_info_buffer.get(),
                                 static_cast<DWORD>(rename_info_length)))
  {
    return true;
  }

  return false;
}
}  // namespace File
