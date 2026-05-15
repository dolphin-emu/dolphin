#include <fmt/format.h>
#include <string>
#include "Common/FileUtil.h"
#include "jni/AndroidCommon/AndroidCommon.h"

namespace HW::GBA::Android
{
std::string GetAndroidSavePath(std::string_view rom_path, int device_number)
{
  std::string_view path_to_use = rom_path;
  if (IsPathAndroidContent(rom_path))
  {
    static std::string display_name;
    display_name = GetAndroidContentDisplayName(rom_path);
    if (!display_name.empty())
      path_to_use = display_name;
  }

  size_t slash_pos = path_to_use.find_last_of("\\/");
  std::string_view filename =
      (slash_pos == std::string::npos) ? path_to_use : path_to_use.substr(slash_pos + 1);
  size_t dot_pos = filename.find_last_of('.');
  std::string_view stem = (dot_pos == std::string::npos) ? filename : filename.substr(0, dot_pos);

  std::string save_filename = fmt::format("{}-{}.sav", stem, device_number + 1);

  if (IsPathAndroidContent(rom_path))
    return File::GetUserPath(D_GBASAVES_IDX) + save_filename;
  return "";
}
}  // namespace HW::GBA::Android
