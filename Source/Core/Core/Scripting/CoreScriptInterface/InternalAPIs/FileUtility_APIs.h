#pragma once

namespace Scripting
{

// FileUtility_APIs contains the APIs for getting the user directory path and the system directory
// path for Dolphin.
typedef struct FileUtility_APIs
{
  // Returns the user directory path for Dolphin.
  const char* (*GetUserDirectoryPath)();

  // Returns the system directory path for Dolphin.
  const char* (*GetSystemDirectoryPath)();

} FileUtility_APIs;

}  // namespace Scripting
