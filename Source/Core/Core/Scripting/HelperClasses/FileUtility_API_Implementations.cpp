#include "Core/Scripting/HelperClasses/FileUtility_API_Implementations.h"

std::string user_path = "";
std::string sys_path = "";

void setUserPath(const std::string new_user_path)
{
  user_path = new_user_path;
}

void setSysPath(const std::string new_sys_path)
{
  sys_path = new_sys_path;
}

const char* GetUserPath_impl()
{
  return user_path.c_str();
}

const char* GetSystemDirectory_impl()
{
  return sys_path.c_str();
}
