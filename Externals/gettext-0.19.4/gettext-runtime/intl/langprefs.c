/* Determine the user's language preferences.
   Copyright (C) 2004-2007 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>.
   Win32 code originally by Michele Cicciotti <hackbunny@reactos.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#if HAVE_CFPREFERENCESCOPYAPPVALUE
# include <string.h>
# include <CoreFoundation/CFPreferences.h>
# include <CoreFoundation/CFPropertyList.h>
# include <CoreFoundation/CFArray.h>
# include <CoreFoundation/CFString.h>
extern void _nl_locale_name_canonicalize (char *name);
#endif

#if defined _WIN32 || defined __WIN32__
# define WIN32_NATIVE
#endif

#ifdef WIN32_NATIVE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

# ifndef MUI_LANGUAGE_NAME
# define MUI_LANGUAGE_NAME 8
# endif
# ifndef STATUS_BUFFER_OVERFLOW
# define STATUS_BUFFER_OVERFLOW 0x80000005
# endif

extern void _nl_locale_name_canonicalize (char *name);
extern const char *_nl_locale_name_from_win32_LANGID (LANGID langid);
extern const char *_nl_locale_name_from_win32_LCID (LCID lcid);

/* Get the preferences list through the MUI APIs. This works on Windows Vista
   and newer.  */
static const char *
_nl_language_preferences_win32_mui (HMODULE kernel32)
{
  /* DWORD GetUserPreferredUILanguages (ULONG dwFlags,
                                        PULONG pulNumLanguages,
                                        PWSTR pwszLanguagesBuffer,
                                        PULONG pcchLanguagesBuffer);  */
  typedef DWORD (WINAPI *GetUserPreferredUILanguages_func) (ULONG, PULONG, PWSTR, PULONG);
  GetUserPreferredUILanguages_func p_GetUserPreferredUILanguages;

  p_GetUserPreferredUILanguages =
   (GetUserPreferredUILanguages_func)
   GetProcAddress (kernel32, "GetUserPreferredUILanguages");
  if (p_GetUserPreferredUILanguages != NULL)
    {
      ULONG num_languages;
      ULONG bufsize;
      DWORD ret;

      bufsize = 0;
      ret = p_GetUserPreferredUILanguages (MUI_LANGUAGE_NAME,
                                           &num_languages,
                                           NULL, &bufsize);
      if (ret == 0
          && GetLastError () == STATUS_BUFFER_OVERFLOW
          && bufsize > 0)
        {
          WCHAR *buffer = (WCHAR *) malloc (bufsize * sizeof (WCHAR));
          if (buffer != NULL)
            {
              ret = p_GetUserPreferredUILanguages (MUI_LANGUAGE_NAME,
                                                   &num_languages,
                                                   buffer, &bufsize);
              if (ret)
                {
                  /* Convert the list from NUL-delimited WCHAR[] Win32 locale
                     names to colon-delimited char[] Unix locale names.
                     We assume that all these locale names are in ASCII,
                     nonempty and contain no colons.  */
                  char *languages =
                    (char *) malloc (bufsize + num_languages * 10 + 1);
                  if (languages != NULL)
                    {
                      const WCHAR *p = buffer;
                      char *q = languages;
                      ULONG i;
                      for (i = 0; i < num_languages; i++)
                        {
                          char *q1;
                          char *q2;

                          q1 = q;
                          if (i > 0)
                            *q++ = ':';
                          q2 = q;
                          for (; *p != (WCHAR)'\0'; p++)
                            {
                              if ((unsigned char) *p != *p || *p == ':')
                                {
                                  /* A non-ASCII character or a colon inside
                                     the Win32 locale name! Punt.  */
                                  q = q1;
                                  break;
                                }
                              *q++ = (unsigned char) *p;
                            }
                          if (q == q1)
                            /* An unexpected Win32 locale name occurred.  */
                            break;
                          *q = '\0';
                          _nl_locale_name_canonicalize (q2);
                          q = q2 + strlen (q2);
                          p++;
                        }
                      *q = '\0';
                      if (q > languages)
                        {
                          free (buffer);
                          return languages;
                        }
                      free (languages);
                    }
                }
              free (buffer);
            }
        }
    }
  return NULL;
}

/* Get a preference.  This works on Windows ME and newer.  */
static const char *
_nl_language_preferences_win32_ME (HMODULE kernel32)
{
  /* LANGID GetUserDefaultUILanguage (void);  */
  typedef LANGID (WINAPI *GetUserDefaultUILanguage_func) (void);
  GetUserDefaultUILanguage_func p_GetUserDefaultUILanguage;

  p_GetUserDefaultUILanguage =
   (GetUserDefaultUILanguage_func)
   GetProcAddress (kernel32, "GetUserDefaultUILanguage");
  if (p_GetUserDefaultUILanguage != NULL)
    return _nl_locale_name_from_win32_LANGID (p_GetUserDefaultUILanguage ());
  return NULL;
}

/* Get a preference.  This works on Windows 95 and newer.  */
static const char *
_nl_language_preferences_win32_95 ()
{
  HKEY desktop_resource_locale_key;

  if (RegOpenKeyExA (HKEY_CURRENT_USER,
                     "Control Panel\\Desktop\\ResourceLocale",
                     0, KEY_QUERY_VALUE, &desktop_resource_locale_key)
      == NO_ERROR)
    {
      DWORD type;
      char data[8 + 1];
      DWORD data_size = sizeof (data);
      DWORD ret;

      ret = RegQueryValueExA (desktop_resource_locale_key, NULL, NULL,
                              &type, data, &data_size);
      RegCloseKey (desktop_resource_locale_key);

      if (ret == NO_ERROR)
        {
          /* We expect a string, at most 8 bytes long, that parses as a
             hexadecimal number.  */
          if (type == REG_SZ
              && data_size <= sizeof (data)
              && (data_size < sizeof (data)
                  || data[sizeof (data) - 1] == '\0'))
            {
              LCID lcid;
              char *endp;
              /* Ensure it's NUL terminated.  */
              if (data_size < sizeof (data))
                data[data_size] = '\0';
              /* Parse it as a hexadecimal number.  */
              lcid = strtoul (data, &endp, 16);
              if (endp > data && *endp == '\0')
                return _nl_locale_name_from_win32_LCID (lcid);
            }
        }
    }
  return NULL;
}

/* Get the system's preference.  This can be used as a fallback.  */
static BOOL CALLBACK
ret_first_language (HMODULE h, LPCSTR type, LPCSTR name, WORD lang, LONG_PTR param)
{
  *(const char **)param = _nl_locale_name_from_win32_LANGID (lang);
  return FALSE;
}
static const char *
_nl_language_preferences_win32_system (HMODULE kernel32)
{
  const char *languages = NULL;
  /* Ignore the warning on mingw here. mingw has a wrong definition of the last
     parameter type of ENUMRESLANGPROC.  */
  EnumResourceLanguages (kernel32, RT_VERSION, MAKEINTRESOURCE (1),
                         ret_first_language, (LONG_PTR)&languages);
  return languages;
}

#endif

/* Determine the user's language preferences, as a colon separated list of
   locale names in XPG syntax
     language[_territory][.codeset][@modifier]
   The result must not be freed; it is statically allocated.
   The LANGUAGE environment variable does not need to be considered; it is
   already taken into account by the caller.  */

const char *
_nl_language_preferences_default (void)
{
#if HAVE_CFPREFERENCESCOPYAPPVALUE /* MacOS X 10.2 or newer */
  {
    /* Cache the preferences list, since CoreFoundation calls are expensive.  */
    static const char *cached_languages;
    static int cache_initialized;

    if (!cache_initialized)
      {
        CFTypeRef preferences =
          CFPreferencesCopyAppValue (CFSTR ("AppleLanguages"),
                                     kCFPreferencesCurrentApplication);
        if (preferences != NULL
            && CFGetTypeID (preferences) == CFArrayGetTypeID ())
          {
            CFArrayRef prefArray = (CFArrayRef)preferences;
            int n = CFArrayGetCount (prefArray);
            char buf[256];
            size_t size = 0;
            int i;

            for (i = 0; i < n; i++)
              {
                CFTypeRef element = CFArrayGetValueAtIndex (prefArray, i);
                if (element != NULL
                    && CFGetTypeID (element) == CFStringGetTypeID ()
                    && CFStringGetCString ((CFStringRef)element,
                                           buf, sizeof (buf),
                                           kCFStringEncodingASCII))
                  {
                    _nl_locale_name_canonicalize (buf);
                    size += strlen (buf) + 1;
                    /* Most GNU programs use msgids in English and don't ship
                       an en.mo message catalog.  Therefore when we see "en"
                       in the preferences list, arrange for gettext() to
                       return the msgid, and ignore all further elements of
                       the preferences list.  */
                    if (strcmp (buf, "en") == 0)
                      break;
                  }
                else
                  break;
              }
            if (size > 0)
              {
                char *languages = (char *) malloc (size);

                if (languages != NULL)
                  {
                    char *p = languages;

                    for (i = 0; i < n; i++)
                      {
                        CFTypeRef element =
                          CFArrayGetValueAtIndex (prefArray, i);
                        if (element != NULL
                            && CFGetTypeID (element) == CFStringGetTypeID ()
                            && CFStringGetCString ((CFStringRef)element,
                                                   buf, sizeof (buf),
                                                   kCFStringEncodingASCII))
                          {
                            _nl_locale_name_canonicalize (buf);
                            strcpy (p, buf);
                            p += strlen (buf);
                            *p++ = ':';
                            if (strcmp (buf, "en") == 0)
                              break;
                          }
                        else
                          break;
                      }
                    *--p = '\0';

                    cached_languages = languages;
                  }
              }
          }
        cache_initialized = 1;
      }
    if (cached_languages != NULL)
      return cached_languages;
  }
#endif

#ifdef WIN32_NATIVE
  {
    /* Cache the preferences list, since computing it is expensive.  */
    static const char *cached_languages;
    static int cache_initialized;

    /* Activate the new code only when the GETTEXT_MUI environment variable is
       set, for the time being, since the new code is not well tested.  */
    if (!cache_initialized && getenv ("GETTEXT_MUI") != NULL)
      {
        const char *languages = NULL;
        HMODULE kernel32 = GetModuleHandle ("kernel32");

        if (kernel32 != NULL)
          languages = _nl_language_preferences_win32_mui (kernel32);

        if (languages == NULL && kernel32 != NULL)
          languages = _nl_language_preferences_win32_ME (kernel32);

        if (languages == NULL)
          languages = _nl_language_preferences_win32_95 ();

        if (languages == NULL && kernel32 != NULL)
          languages = _nl_language_preferences_win32_system (kernel32);

        cached_languages = languages;
        cache_initialized = 1;
      }
    if (cached_languages != NULL)
      return cached_languages;
  }
#endif

  return NULL;
}
