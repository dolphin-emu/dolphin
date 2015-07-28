/*
 * Windows backend common header for libusb 1.0
 *
 * This file brings together header code common between
 * the desktop Windows and Windows CE backends.
 * Copyright © 2012-2013 RealVNC Ltd.
 * Copyright © 2009-2012 Pete Batard <pete@akeo.ie>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of this code adapted from libusb-win32-v1 by Stephan Meyer
 * Major code testing contribution by Xiaofan Chen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

// Windows API default is uppercase - ugh!
#if !defined(bool)
#define bool BOOL
#endif
#if !defined(true)
#define true TRUE
#endif
#if !defined(false)
#define false FALSE
#endif

#define safe_free(p) do {if (p != NULL) {free((void*)p); p = NULL;}} while(0)
#define safe_closehandle(h) do {if (h != INVALID_HANDLE_VALUE) {CloseHandle(h); h = INVALID_HANDLE_VALUE;}} while(0)
#define safe_min(a, b) min((size_t)(a), (size_t)(b))
#define safe_strcp(dst, dst_max, src, count) do {memcpy(dst, src, safe_min(count, dst_max)); \
	((char*)dst)[safe_min(count, dst_max)-1] = 0;} while(0)
#define safe_strcpy(dst, dst_max, src) safe_strcp(dst, dst_max, src, safe_strlen(src)+1)
#define safe_strncat(dst, dst_max, src, count) strncat(dst, src, safe_min(count, dst_max - safe_strlen(dst) - 1))
#define safe_strcat(dst, dst_max, src) safe_strncat(dst, dst_max, src, safe_strlen(src)+1)
#define safe_strcmp(str1, str2) strcmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2))
#define safe_stricmp(str1, str2) _stricmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2))
#define safe_strncmp(str1, str2, count) strncmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2), count)
#define safe_strlen(str) ((str==NULL)?0:strlen(str))
#define safe_sprintf(dst, count, ...) do {_snprintf(dst, count, __VA_ARGS__); (dst)[(count)-1] = 0; } while(0)
#define safe_stprintf _sntprintf
#define safe_tcslen(str) ((str==NULL)?0:_tcslen(str))
#define safe_unref_device(dev) do {if (dev != NULL) {libusb_unref_device(dev); dev = NULL;}} while(0)
#define wchar_to_utf8_ms(wstr, str, strlen) WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, strlen, NULL, NULL)
#ifndef ARRAYSIZE
#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#endif

#define ERR_BUFFER_SIZE             256
#define TIMER_REQUEST_RETRY_MS      100
#define MAX_TIMER_SEMAPHORES        128


/*
 * API macros - from libusb-win32 1.x
 */
#define DLL_DECLARE_PREFIXNAME(api, ret, prefixname, name, args)    \
	typedef ret (api * __dll_##name##_t)args;                       \
	static __dll_##name##_t prefixname = NULL

#ifndef _WIN32_WCE
#define DLL_STRINGIFY(dll) #dll
#define DLL_GET_MODULE_HANDLE(dll) GetModuleHandleA(DLL_STRINGIFY(dll))
#define DLL_LOAD_LIBRARY(dll) LoadLibraryA(DLL_STRINGIFY(dll))
#else
#define DLL_STRINGIFY(dll) L#dll
#define DLL_GET_MODULE_HANDLE(dll) GetModuleHandle(DLL_STRINGIFY(dll))
#define DLL_LOAD_LIBRARY(dll) LoadLibrary(DLL_STRINGIFY(dll))
#endif

#define DLL_LOAD_PREFIXNAME(dll, prefixname, name, ret_on_failure) \
	do {                                                           \
		HMODULE h = DLL_GET_MODULE_HANDLE(dll);                    \
	if (!h)                                                        \
		h = DLL_LOAD_LIBRARY(dll);                                 \
	if (!h) {                                                      \
		if (ret_on_failure) { return LIBUSB_ERROR_NOT_FOUND; }     \
		else { break; }                                            \
	}                                                              \
	prefixname = (__dll_##name##_t)GetProcAddress(h,               \
	                        DLL_STRINGIFY(name));                  \
	if (prefixname) break;                                         \
	prefixname = (__dll_##name##_t)GetProcAddress(h,               \
	                        DLL_STRINGIFY(name) DLL_STRINGIFY(A)); \
	if (prefixname) break;                                         \
	prefixname = (__dll_##name##_t)GetProcAddress(h,               \
	                        DLL_STRINGIFY(name) DLL_STRINGIFY(W)); \
	if (prefixname) break;                                         \
	if(ret_on_failure)                                             \
		return LIBUSB_ERROR_NOT_FOUND;                             \
	} while(0)

#define DLL_DECLARE(api, ret, name, args)   DLL_DECLARE_PREFIXNAME(api, ret, name, name, args)
#define DLL_LOAD(dll, name, ret_on_failure) DLL_LOAD_PREFIXNAME(dll, name, name, ret_on_failure)
#define DLL_DECLARE_PREFIXED(api, ret, prefix, name, args)   DLL_DECLARE_PREFIXNAME(api, ret, prefix##name, name, args)
#define DLL_LOAD_PREFIXED(dll, prefix, name, ret_on_failure) DLL_LOAD_PREFIXNAME(dll, prefix##name, name, ret_on_failure)
