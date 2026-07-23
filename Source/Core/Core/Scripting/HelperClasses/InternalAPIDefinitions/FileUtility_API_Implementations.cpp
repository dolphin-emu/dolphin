#include "Core/Scripting/HelperClasses/InternalAPIDefinitions/FileUtility_API_Implementations.h"

#include <string>
#include "Common/FileUtil.h"

namespace Scripting::FileUtilityAPIsImpl
{
FileUtility_APIs fileUtility_APIs = {};

std::string user_directory_path;
std::string system_directory_path;

void Init()
{
  fileUtility_APIs.GetUserDirectoryPath = GetUserDirectoryPath_impl;
  fileUtility_APIs.GetSystemDirectoryPath = GetSystemDirectoryPath_impl;
  user_directory_path = File::GetUserPath(D_LOAD_IDX);
  system_directory_path = File::GetSysDirectory();
}

const char* GetUserDirectoryPath_impl()
{
  return user_directory_path.c_str();
}

const char* GetSystemDirectoryPath_impl()
{
  return system_directory_path.c_str();
}

}  // namespace Scripting::FileUtilityAPIsImpl
