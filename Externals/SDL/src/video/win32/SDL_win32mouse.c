/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/* we need to define it, so that raw input is included*/

#if (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include "SDL_config.h"

#include "SDL_win32video.h"

#include "../../events/SDL_mouse_c.h"

extern HANDLE *mice;
extern int total_mice;
extern int tablet;

void
WIN_InitMouse(_THIS)
{
    int index = 0;
    RAWINPUTDEVICELIST *deviceList = NULL;
    int devCount = 0;
    int i;
    UINT tmp = 0;
    char *buffer = NULL;
    char *tab = "wacom";        /* since windows does't give us handles to tablets, we have to detect a tablet by it's name */
    const char *rdp = "rdp_mou";
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

/* WinCE has no RawInputDeviceList */
#ifdef _WIN32_WCE
    SDL_Mouse mouse;
    SDL_zero(mouse);
    mouse.id = 0;
    SDL_AddMouse(&mouse, "Stylus", 0, 0, 1);
#else
    /* we're checking for the number of rawinput devices */
    if (GetRawInputDeviceList(NULL, &devCount, sizeof(RAWINPUTDEVICELIST))) {
        return;
    }

    deviceList = SDL_malloc(sizeof(RAWINPUTDEVICELIST) * devCount);

    /* we're getting the raw input device list */
    GetRawInputDeviceList(deviceList, &devCount, sizeof(RAWINPUTDEVICELIST));
    mice = SDL_malloc(devCount * sizeof(HANDLE));

    /* we're getting the details of the devices */
    for (i = 0; i < devCount; ++i) {
        int is_rdp = 0;
        UINT j;
        UINT k;
        char *default_device_name = "Pointing device xx";
        const char *reg_key_root = "System\\CurrentControlSet\\Enum\\";
        char *device_name = SDL_malloc(256 * sizeof(char));
        char *key_name = NULL;
        char *tmp_name = NULL;
        LONG rc = 0;
        HKEY hkey;
        DWORD regtype = REG_SZ;
        DWORD out = 256 * sizeof(char);
        SDL_Mouse mouse;
        size_t l;
        if (deviceList[i].dwType != RIM_TYPEMOUSE) {    /* if a device isn't a mouse type we don't want it */
            continue;
        }
        if (GetRawInputDeviceInfoA
            (deviceList[i].hDevice, RIDI_DEVICENAME, NULL, &tmp) < 0) {
            continue;
        }
        buffer = SDL_malloc((tmp + 1) * sizeof(char));
        key_name =
            SDL_malloc((tmp + SDL_strlen(reg_key_root) + 1) * sizeof(char));

        /* we're getting the device registry path and polishing it to get it's name,
           surely there must be an easier way, but we haven't found it yet */
        if (GetRawInputDeviceInfoA
            (deviceList[i].hDevice, RIDI_DEVICENAME, buffer, &tmp) < 0) {
            continue;
        }
        buffer += 4;
        tmp -= 4;
        tmp_name = buffer;
        for (j = 0; j < tmp; ++j) {
            if (*tmp_name == '#') {
                *tmp_name = '\\';
            }

            else if (*tmp_name == '{') {
                break;
            }
            ++tmp_name;
        }
        *tmp_name = '\0';
        SDL_memcpy(key_name, reg_key_root, SDL_strlen(reg_key_root));
        SDL_memcpy(key_name + (SDL_strlen(reg_key_root)), buffer, j + 1);
        l = SDL_strlen(key_name);
        is_rdp = 0;
        if (l >= 7) {
            for (j = 0; j < l - 7; ++j) {
                for (k = 0; k < 7; ++k) {
                    if (rdp[k] !=
                        SDL_tolower((unsigned char) key_name[j + k])) {
                        break;
                    }
                }
                if (k == 7) {
                    is_rdp = 1;
                    break;
                }
            }
        }

        buffer -= 4;

        if (is_rdp == 1) {
            SDL_free(buffer);
            SDL_free(key_name);
            SDL_free(device_name);
            is_rdp = 0;
            continue;
        }

        /* we're opening the registry key to get the mouse name */
        rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key_name, 0, KEY_READ, &hkey);
        if (rc != ERROR_SUCCESS) {
            SDL_memcpy(device_name, default_device_name,
                       SDL_strlen(default_device_name));
        }
        rc = RegQueryValueExA(hkey, "DeviceDesc", NULL, &regtype, device_name,
                              &out);
        RegCloseKey(hkey);
        if (rc != ERROR_SUCCESS) {
            SDL_memcpy(device_name, default_device_name,
                       SDL_strlen(default_device_name));
        }

        /* we're saving the handle to the device */
        mice[index] = deviceList[i].hDevice;
        SDL_zero(mouse);
        mouse.id = index;
        l = SDL_strlen(device_name);

        /* we're checking if the device isn't by any chance a tablet */
        if (data->wintabDLL && tablet == -1) {
            for (j = 0; j < l - 5; ++j) {
                for (k = 0; k < 5; ++k) {
                    if (tab[k] !=
                        SDL_tolower((unsigned char) device_name[j + k])) {
                        break;
                    }
                }
                if (k == 5) {
                    tablet = index;
                    break;
                }
            }
        }

        /* if it's a tablet, let's read it's maximum and minimum pressure */
        if (tablet == index) {
            AXIS pressure;
            int cursors;
            data->WTInfoA(WTI_DEVICES, DVC_NPRESSURE, &pressure);
            data->WTInfoA(WTI_DEVICES, DVC_NCSRTYPES, &cursors);
            SDL_AddMouse(&mouse, device_name, pressure.axMax, pressure.axMin,
                         cursors);
        } else {
            SDL_AddMouse(&mouse, device_name, 0, 0, 1);
        }
        ++index;
        SDL_free(buffer);
        SDL_free(key_name);
    }
    total_mice = index;
    SDL_free(deviceList);
#endif /*_WIN32_WCE*/
}

void
WIN_QuitMouse(_THIS)
{
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    /* let's delete all of the mice */
    SDL_MouseQuit();
}

/* vi: set ts=4 sw=4 expandtab: */
