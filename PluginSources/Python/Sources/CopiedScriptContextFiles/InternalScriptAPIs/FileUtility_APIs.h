#ifndef FILE_UTILITY_APIS
#define FILE_UTILITY_APIS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FileUtility_APIs
{
  const char* (*GetUserPath)();
  const char* (*GetSystemDirectory)();
} FileUtility_APIs;


#ifdef __cplusplus
}
#endif



#endif
