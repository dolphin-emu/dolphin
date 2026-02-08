#ifndef MINIUPNPCSTRINGS_H_INCLUDED
#define MINIUPNPCSTRINGS_H_INCLUDED

#if   defined(_WIN32)
#define OS_STRING "Windows"
#elif defined(__linux__)
#define OS_STRING "Linux"
#elif defined(__OSX__)
#define OS_STRING "Mac OS X"
#elif defined(__APPLE__)
#define OS_STRING "Mac OS X"
#elif defined(__DARWIN__)
#define OS_STRING "Darwin"
#else
#define OS_STRING "Generic"
#endif
#define MINIUPNPC_VERSION_STRING "1.9"
#define UPNP_VERSION_STRING "UPnP/1.1"

#endif
