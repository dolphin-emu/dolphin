// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

namespace Config
{
// Configuration Information

// Main.NetPlay

extern const Info<std::string> NETPLAY_TRAVERSAL_SERVER;
extern const Info<u16> NETPLAY_TRAVERSAL_PORT;
extern const Info<std::string> NETPLAY_TRAVERSAL_CHOICE;
extern const Info<std::string> NETPLAY_HOST_CODE;
extern const Info<std::string> NETPLAY_INDEX_URL;

extern const Info<u16> NETPLAY_HOST_PORT;
extern const Info<std::string> NETPLAY_ADDRESS;
extern const Info<u16> NETPLAY_CONNECT_PORT;
extern const Info<u16> NETPLAY_LISTEN_PORT;

extern const Info<std::string> NETPLAY_NICKNAME;
extern const Info<bool> NETPLAY_USE_UPNP;

extern const Info<bool> NETPLAY_ENABLE_QOS;

extern const Info<bool> NETPLAY_USE_INDEX;
extern const Info<std::string> NETPLAY_INDEX_REGION;
extern const Info<std::string> NETPLAY_INDEX_NAME;
extern const Info<std::string> NETPLAY_INDEX_PASSWORD;

extern const Info<bool> NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT;
extern const Info<u32> NETPLAY_CHUNKED_UPLOAD_LIMIT;

extern const Info<u32> NETPLAY_BUFFER_SIZE;
extern const Info<u32> NETPLAY_CLIENT_BUFFER_SIZE;

extern const Info<bool> NETPLAY_WRITE_SAVE_SDCARD_DATA;
extern const Info<bool> NETPLAY_LOAD_WII_SAVE;
extern const Info<bool> NETPLAY_SYNC_SAVES;
extern const Info<bool> NETPLAY_SYNC_CODES;
extern const Info<bool> NETPLAY_RECORD_INPUTS;
extern const Info<bool> NETPLAY_STRICT_SETTINGS_SYNC;
extern const Info<std::string> NETPLAY_NETWORK_MODE;
extern const Info<bool> NETPLAY_SYNC_ALL_WII_SAVES;
extern const Info<bool> NETPLAY_GOLF_MODE_OVERLAY;

}  // namespace Config
