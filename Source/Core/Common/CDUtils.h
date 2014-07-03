#pragma once

#include <string>
#include <vector>

// Returns a pointer to an array of strings with the device names
std::vector<std::string> cdio_get_devices();

// Returns true if device is cdrom/dvd
bool cdio_is_cdrom(std::string device);
