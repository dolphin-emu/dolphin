// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cwchar>
#include <unordered_set>
#include <vector>

#include <Windows.h>
#include <SetupAPI.h>

namespace ciface::DInput
{
// Code for enumerating hardware devices that use the XINPUT device driver.
// The MSDN recommended code suffers from massive performance problems when using language packs,
// if the system and user languages differ then WMI Queries become incredibly slow (multiple
// seconds). This is more or less equivalent and much faster.
std::unordered_set<DWORD> GetXInputGUIDS()
{
  static const GUID s_GUID_devclass_HID = {
      0x745a17a0, 0x74d3, 0x11d0, {0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda}};
  std::unordered_set<DWORD> guids;

  // Enumerate everything under the "Human Interface Devices" tree in the Device Manager
  // NOTE: Some devices show up multiple times due to sub-devices, we rely on the set to
  //   prevent duplicates.
  HDEVINFO setup_enum = SetupDiGetClassDevsW(&s_GUID_devclass_HID, nullptr, nullptr, DIGCF_PRESENT);
  if (setup_enum == INVALID_HANDLE_VALUE)
    return guids;

  std::vector<wchar_t> buffer(128);
  SP_DEVINFO_DATA dev_info;
  dev_info.cbSize = sizeof(SP_DEVINFO_DATA);
  for (DWORD i = 0; SetupDiEnumDeviceInfo(setup_enum, i, &dev_info); ++i)
  {
    // Need to find the size of the data and set the buffer appropriately
    DWORD buffer_size = 0;
    while (!SetupDiGetDeviceRegistryPropertyW(setup_enum, &dev_info, SPDRP_HARDWAREID, nullptr,
                                              reinterpret_cast<BYTE*>(buffer.data()),
                                              static_cast<DWORD>(buffer.size()), &buffer_size))
    {
      if (buffer_size > buffer.size())
        buffer.resize(buffer_size);
      else
        break;
    }
    if (GetLastError() != ERROR_SUCCESS)
      continue;

    // HARDWAREID is a REG_MULTI_SZ
    // There are multiple strings separated by NULs, the list is ended by an empty string.
    for (std::size_t j = 0; buffer[j]; j += std::wcslen(&buffer[j]) + 1)
    {
      // XINPUT devices have "IG_xx" embedded in their IDs which is what we look for.
      if (!std::wcsstr(&buffer[j], L"IG_"))
        continue;

      unsigned int vid = 0;
      unsigned int pid = 0;

      // Extract Vendor and Product IDs for matching against DirectInput's device list.
      wchar_t* pos = std::wcsstr(&buffer[j], L"VID_");
      if (!pos || !std::swscanf(pos, L"VID_%4X", &vid))
        continue;
      pos = std::wcsstr(&buffer[j], L"PID_");
      if (!pos || !std::swscanf(pos, L"PID_%4X", &pid))
        continue;

      guids.insert(MAKELONG(vid, pid));
      break;
    }
  }

  SetupDiDestroyDeviceInfoList(setup_enum);
  return guids;
}
}  // namespace ciface::DInput
