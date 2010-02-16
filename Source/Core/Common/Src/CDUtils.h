#ifndef _CDUTILS_H_
#define _CDUTILS_H_

#include <stdlib.h>
#include <vector>
#include <string>
#include <stdio.h>

// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices();

// Returns true if device is cdrom/dvd
bool cdio_is_cdrom(std::string device);

#endif // _CDUTILS_H_
