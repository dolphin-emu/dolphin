#include "rc_compat.h"

#include <ctype.h>
#include <stdarg.h>

int rc_strncasecmp(const char* left, const char* right, size_t length)
{
  while (length)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
    --length;
  }

  return 0;
}

int rc_strcasecmp(const char* left, const char* right)
{
  while (*left || *right)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
  }

  return 0;
}

char* rc_strdup(const char* str)
{
  const size_t length = strlen(str);
  char* buffer = (char*)malloc(length + 1);
  memcpy(buffer, str, length + 1);
  return buffer;
}

int rc_snprintf(char* buffer, size_t size, const char* format, ...)
{
   int result;
   va_list args;

   va_start(args, format);
   /* assume buffer is large enough and ignore size */
   (void)size;
   result = vsprintf(buffer, format, args);
   va_end(args);

   return result;
}
