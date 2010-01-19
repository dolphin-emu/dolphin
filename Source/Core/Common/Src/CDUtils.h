#ifndef _CDUTILS_H_
#define _CDUTILS_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Returns a pointer to an array of strings with the device names
char **cdio_get_devices();

// Free device list returned by cdio_get_devices or
// cdio_get_devices_with_cap.
void cdio_free_device_list(char * ppsz_device_list[]);

// Returns true if device is cdrom/dvd
bool cdio_is_cdrom(const char *device);

#endif // _CDUTILS_H_
