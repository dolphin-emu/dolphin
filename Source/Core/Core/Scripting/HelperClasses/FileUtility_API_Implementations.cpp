#include "Core/Scripting/HelperClasses/FileUtility_API_Implementations.h"

namespace Scripting
{
static std::string user_path = "";
static std::string sys_path = "";

void SetUserPath(std::string new_user_path)
{
  user_path = std::move(new_user_path);
}

void SetSysPath(std::string new_sys_path)
{
  sys_path = std::move(new_sys_path);
}

const char* GetUserDirectoryPath_impl()
{
  return user_path.c_str();
}

const char* GetSystemDirectoryPath_impl()
{
  return sys_path.c_str();
}

}  // namespace Scripting
