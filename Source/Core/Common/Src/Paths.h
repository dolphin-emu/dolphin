#ifndef PATHS_H
#define PATHS_H

#define PLUGINS_DIR "Plugins"
#define DEFAULT_DATA_DIR ""
#define USERDATA_DIR "User"
#define SYSDATA_DIR "Sys"

// Under User
#define CONFIG_DIR "Config"
#define GAMECONFIG_DIR "GameConfig"
#define CACHE_DIR "Cache"
#define SAVESTATES_DIR "SaveStates"
#define SCREENSHOTS_DIR "ScreenShots"
#define LOGS_DIR "Logs"

// Under Sys


// Files
#define DOLPHIN_CONFIG "Dolphin.ini"

#define DEFAULT_GFX_PLUGIN "Plugin_VideoOGL"
#define DEFAULT_DSP_PLUGIN "Plugin_DSP_HLE"
#define DEFAULT_PAD_PLUGIN "Plugin_PadSimple"
#define DEFAULT_WIIMOTE_PLUGIN "Plugin_Wiimote"

#ifdef _WIN32
#define PLUGIN_SUFFIX ".dll"
#define DIR_SEP "\\"
#else
#define PLUGIN_SUFFIX ".so"
#define DIR_SEP "/"
#endif

#endif // PATHS_H
