#ifndef FILE_UTILITY_APIS
#define FILE_UTILITY_APIS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FileUtility_APIs
{
  // Returns the user directory path for Dolphin.
  const char* (*GetUserPath)();

  // Returns the system directory path for Dolphin.
  const char* (*GetSystemDirectory)();

} FileUtility_APIs;

#ifdef __cplusplus
}
#endif

#endif
