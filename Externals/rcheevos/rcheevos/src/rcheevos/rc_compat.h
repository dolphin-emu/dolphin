#ifndef RC_COMPAT_H
#define RC_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MINGW) || defined(__MINGW32__) || defined(__MINGW64__)

/* MinGW redefinitions */

#elif defined(_MSC_VER)

/* Visual Studio redefinitions */

#ifndef strcasecmp
 #define strcasecmp _stricmp
#endif
#ifndef strncasecmp
 #define strncasecmp _strnicmp
#endif
#ifndef strdup
 #define strdup _strdup
#endif

#elif __STDC_VERSION__ < 199901L

/* C89 redefinitions */

#ifndef snprintf
 extern int rc_snprintf(char* buffer, size_t size, const char* format, ...);
 #define snprintf rc_snprintf
#endif

#ifndef strncasecmp
 extern int rc_strncasecmp(const char* left, const char* right, size_t length);
 #define strncasecmp rc_strncasecmp
#endif

#ifndef strcasecmp
 extern int rc_strcasecmp(const char* left, const char* right);
 #define strcasecmp rc_strcasecmp
#endif

#ifndef strdup
 extern char* rc_strdup(const char* str);
 #define strdup rc_strdup
#endif

#endif /* __STDC_VERSION__ < 199901L */

#ifdef __cplusplus
}
#endif

#endif /* RC_COMPAT_H */
