// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

enum MACConsumer
{
	BBA = 0,
	IOS = 1
};

enum
{
	MAC_ADDRESS_SIZE = 6
};

void GenerateMacAddress(const MACConsumer type, u8* mac);
std::string MacAddressToString(const u8* mac);
bool StringToMacAddress(const std::string& mac_string, u8* mac);
