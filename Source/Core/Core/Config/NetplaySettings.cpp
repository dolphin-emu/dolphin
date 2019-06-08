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
const ConfigInfo<std::string> NETPLAY_INDEX_URL{{System::Main, "NetPlay", "IndexServer"},
                                                "https://lobby.dolphin-emu.org"};

const ConfigInfo<bool> NETPLAY_USE_INDEX{{System::Main, "NetPlay", "UseIndex"}, false};
const ConfigInfo<std::string> NETPLAY_INDEX_NAME{{System::Main, "NetPlay", "IndexName"}, ""};
const ConfigInfo<std::string> NETPLAY_INDEX_REGION{{System::Main, "NetPlay", "IndexRegion"}, ""};
const ConfigInfo<std::string> NETPLAY_INDEX_PASSWORD{{System::Main, "NetPlay", "IndexPassword"},
                                                     ""};

const ConfigInfo<std::string> NETPLAY_HOST_CODE{{System::Main, "NetPlay", "HostCode"}, "00000000"};

const ConfigInfo<u16> NETPLAY_HOST_PORT{{System::Main, "NetPlay", "HostPort"}, DEFAULT_LISTEN_PORT};
const ConfigInfo<std::string> NETPLAY_ADDRESS{{System::Main, "NetPlay", "Address"}, "127.0.0.1"};
const ConfigInfo<u16> NETPLAY_CONNECT_PORT{{System::Main, "NetPlay", "ConnectPort"},
                                           DEFAULT_LISTEN_PORT};
const ConfigInfo<u16> NETPLAY_LISTEN_PORT{{System::Main, "NetPlay", "ListenPort"},
                                          DEFAULT_LISTEN_PORT};

const ConfigInfo<std::string> NETPLAY_NICKNAME{{System::Main, "NetPlay", "Nickname"}, "Player"};
const ConfigInfo<bool> NETPLAY_USE_UPNP{{System::Main, "NetPlay", "UseUPNP"}, false};

const ConfigInfo<bool> NETPLAY_ENABLE_QOS{{System::Main, "NetPlay", "EnableQoS"}, true};

const ConfigInfo<bool> NETPLAY_ENABLE_CHUNKED_UPLOAD_LIMIT{
    {System::Main, "NetPlay", "EnableChunkedUploadLimit"}, false};
const ConfigInfo<u32> NETPLAY_CHUNKED_UPLOAD_LIMIT{{System::Main, "NetPlay", "ChunkedUploadLimit"},
                                                   3000};

const ConfigInfo<u32> NETPLAY_BUFFER_SIZE{{System::Main, "NetPlay", "BufferSize"}, 5};
const ConfigInfo<u32> NETPLAY_CLIENT_BUFFER_SIZE{{System::Main, "NetPlay", "BufferSizeClient"}, 1};

const ConfigInfo<bool> NETPLAY_WRITE_SAVE_SDCARD_DATA{
    {System::Main, "NetPlay", "WriteSaveSDCardData"}, false};
const ConfigInfo<bool> NETPLAY_LOAD_WII_SAVE{{System::Main, "NetPlay", "LoadWiiSave"}, false};
const ConfigInfo<bool> NETPLAY_SYNC_SAVES{{System::Main, "NetPlay", "SyncSaves"}, true};
const ConfigInfo<bool> NETPLAY_SYNC_CODES{{System::Main, "NetPlay", "SyncCodes"}, true};
const ConfigInfo<bool> NETPLAY_RECORD_INPUTS{{System::Main, "NetPlay", "RecordInputs"}, false};
const ConfigInfo<bool> NETPLAY_REDUCE_POLLING_RATE{{System::Main, "NetPlay", "ReducePollingRate"},
                                                   false};
const ConfigInfo<bool> NETPLAY_STRICT_SETTINGS_SYNC{{System::Main, "NetPlay", "StrictSettingsSync"},
                                                    false};
const ConfigInfo<std::string> NETPLAY_NETWORK_MODE{{System::Main, "NetPlay", "NetworkMode"},
                                                   "fixeddelay"};
const ConfigInfo<bool> NETPLAY_SYNC_ALL_WII_SAVES{{System::Main, "NetPlay", "SyncAllWiiSaves"},
                                                  false};
const ConfigInfo<bool> NETPLAY_GOLF_MODE_OVERLAY{{System::Main, "NetPlay", "GolfModeOverlay"},
                                                 true};

}  // namespace Config
