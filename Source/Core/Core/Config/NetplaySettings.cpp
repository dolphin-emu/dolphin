// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Config/NetplaySettings.h"

#include "Common/Config/Config.h"

namespace Config
{
static constexpr u16 DEFAULT_LISTEN_PORT = 2626;

// Configuration Information

// Main.NetPlay

const ConfigInfo<std::string> NETPLAY_TRAVERSAL_SERVER{{System::Main, "NetPlay", "TraversalServer"},
                                                       "stun.dolphin-emu.org"};
const ConfigInfo<u16> NETPLAY_TRAVERSAL_PORT{{System::Main, "NetPlay", "TraversalPort"}, 6262};
const ConfigInfo<std::string> NETPLAY_TRAVERSAL_CHOICE{{System::Main, "NetPlay", "TraversalChoice"},
                                                       "direct"};
const ConfigInfo<std::string> NETPLAY_HOST_CODE{{System::Main, "NetPlay", "HostCode"}, "00000000"};

const ConfigInfo<u16> NETPLAY_HOST_PORT{{System::Main, "NetPlay", "HostPort"}, DEFAULT_LISTEN_PORT};
const ConfigInfo<std::string> NETPLAY_ADDRESS{{System::Main, "NetPlay", "Address"}, "127.0.0.1"};
const ConfigInfo<u16> NETPLAY_CONNECT_PORT{{System::Main, "NetPlay", "ConnectPort"},
                                           DEFAULT_LISTEN_PORT};
const ConfigInfo<u16> NETPLAY_LISTEN_PORT{{System::Main, "NetPlay", "ListenPort"},
                                          DEFAULT_LISTEN_PORT};

const ConfigInfo<std::string> NETPLAY_NICKNAME{{System::Main, "NetPlay", "Nickname"}, "Player"};
const ConfigInfo<std::string> NETPLAY_SELECTED_HOST_GAME{
    {System::Main, "NetPlay", "SelectedHostGame"}, ""};
const ConfigInfo<bool> NETPLAY_USE_UPNP{{System::Main, "NetPlay", "UseUPNP"}, false};

}  // namespace Config
