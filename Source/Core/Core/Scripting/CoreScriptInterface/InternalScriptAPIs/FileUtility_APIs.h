#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// FileUtility_APIs contains the APIs for getting the user directory path and the system directory
// path for Dolphin.
typedef struct FileUtility_APIs
{
  // Returns the user directory path for Dolphin.
  const char* (*GetUserDirectoryPath)();

  // Returns the system directory path for Dolphin.
  const char* (*GetSystemDirectoryPath)();

} FileUtility_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
