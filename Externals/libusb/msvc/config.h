/* config.h.  Manual config for MSVC.  */

#ifndef _MSC_VER
#warn "msvc/config.h shouldn't be included for your development environment."
#error "Please make sure the msvc/ directory is removed from your build path."
#endif

/* Visual Studio 2015 and later defines timespec */
#if (_MSC_VER >= 1900)
#define _TIMESPEC_DEFINED 1
#endif

/* Disable: warning C4200: nonstandard extension used : zero-sized array in struct/union */
#pragma warning(disable:4200)
/* Disable: warning C4324: structure was padded due to __declspec(align()) */
#pragma warning(disable:4324)
/* Disable: warning C6258: Using TerminateThread does not allow proper thread clean up */
#pragma warning(disable:6258)
/* Disable: warning C4996: 'GetVersionA': was declared deprecated */
#pragma warning(disable:4996)

#if defined(_PREFAST_)
/* Disable "Banned API" errors when using the MS's WDK OACR/Prefast */
#pragma warning(disable:28719)
/* Disable "The function 'InitializeCriticalSection' must be called from within a try/except block" */
#pragma warning(disable:28125)
#endif

/* Default visibility */
#define DEFAULT_VISIBILITY /**/

/* Enable global message logging */
#define ENABLE_LOGGING 1

/* Uncomment to start with debug message logging enabled */
// #define ENABLE_DEBUG_LOGGING 1

/* Uncomment to enabling logging to system log */
// #define USE_SYSTEM_LOGGING_FACILITY

/* type of second poll() argument */
#define POLL_NFDS_TYPE unsigned int

/* Windows/WinCE backend */
#if defined(_WIN32_WCE)
#define OS_WINCE 1
#define HAVE_MISSING_H
#else
#define OS_WINDOWS 1
#define HAVE_SYS_TYPES_H 1
#endif
