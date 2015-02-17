/*
 * Source file for missing WinCE functionality
 * Copyright © 2012 RealVNC Ltd.
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

#include "missing.h"

#include <config.h>
#include <libusbi.h>

#include <windows.h>

// The registry path to store environment variables
#define ENVIRONMENT_REG_PATH _T("Software\\libusb\\environment")

/* Workaround getenv not being available on WinCE.
 * Instead look in HKLM\Software\libusb\environment */
char *getenv(const char *name)
{
	static char value[MAX_PATH];
	TCHAR wValue[MAX_PATH];
	WCHAR wName[MAX_PATH];
	DWORD dwType, dwData;
	HKEY hkey;
	LONG rc;

	if (!name)
		return NULL;

	if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wName, MAX_PATH) <= 0) {
		usbi_dbg("Failed to convert environment variable name to wide string");
		return NULL;
	}
	wName[MAX_PATH - 1] = 0; // Be sure it's NUL terminated

	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ENVIRONMENT_REG_PATH, 0, KEY_QUERY_VALUE, &hkey);
	if (rc != ERROR_SUCCESS) {
		usbi_dbg("Failed to open registry key for getenv with error %d", rc);
		return NULL;
	}

	// Attempt to read the key
	dwData = sizeof(wValue);
	rc = RegQueryValueEx(hkey, wName, NULL, &dwType,
		(LPBYTE)&wValue, &dwData);
	RegCloseKey(hkey);
	if (rc != ERROR_SUCCESS) {
		usbi_dbg("Failed to read registry key value for getenv with error %d", rc);
		return NULL;
	}
	if (dwType != REG_SZ) {
		usbi_dbg("Registry value was of type %d instead of REG_SZ", dwType);
		return NULL;
	}

	// Success in reading the key, convert from WCHAR to char
	if (WideCharToMultiByte(CP_UTF8, 0,
			wValue, dwData / sizeof(*wValue),
			value, MAX_PATH,
			NULL, NULL) <= 0) {
		usbi_dbg("Failed to convert environment variable value to narrow string");
		return NULL;
	}
	value[MAX_PATH - 1] = 0; // Be sure it's NUL terminated
	return value;
}
