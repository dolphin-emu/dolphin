#ifndef _CDUTILS_H_
#define _CDUTILS_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH

#elif __APPLE__
#include <paths.h>
#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

#elif __linux__
#include <mntent.h>
#include <unistd.h> 
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
 
#include <linux/cdrom.h>
#endif // WIN32

// Returns a pointer to an array of strings with the device names
char **cdio_get_devices();

// Free device list returned by cdio_get_devices or
// cdio_get_devices_with_cap.
void cdio_free_device_list(char * ppsz_device_list[]);

// Returns true if device is cdrom/dvd
bool cdio_is_cdrom(const char *device);

#endif // _CDUTILS_H_
