#ifndef FILE_UTILITY_API_IMPLS
#define FILE_UTILITY_API_IMPLS

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

extern std::string user_path;
extern std::string sys_path;

void setUserPath(const std::string new_user_path);
void setSysPath(const std::string new_sys_path);

const char* GetUserPath_impl();
const char* GetSystemDirectory_impl();

#ifdef __cplusplus
}
#endif

#endif
