#ifdef _WIN32

#define EXPORT	__declspec(dllexport)
#define CALL	__cdecl

#else

#define EXPORT	__attribute__ ((visibility("default")))
#define CALL

#endif

#if defined(__cplusplus)
extern "C" {
#endif
