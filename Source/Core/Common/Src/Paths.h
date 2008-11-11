#ifndef PATHS_H
#define PATHS_H

#ifdef _WIN32
#define PLUGIN_PREFIX ""
#define PLUGIN_SUFFIX ".dll"
#define DIR_SEP "\\"
#else
#define PLUGIN_PREFIX "lib"
#define PLUGIN_SUFFIX ".so"
#define DIR_SEP "/"
#endif

#define PLUGINS_DIR "Plugins"
#define DATA_DIR "."
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

#define DEFAULT_GFX_PLUGIN PLUGIN_PREFIX "Plugin_VideoOGL" PLUGIN_SUFFIX
#define DEFAULT_DSP_PLUGIN PLUGIN_PREFIX "Plugin_DSP_HLE" PLUGIN_SUFFIX
#define DEFAULT_PAD_PLUGIN PLUGIN_PREFIX "Plugin_PadSimple" PLUGIN_SUFFIX
#define DEFAULT_WIIMOTE_PLUGIN PLUGIN_PREFIX "Plugin_Wiimote" PLUGIN_SUFFIX

// shorts
#ifndef _WIN32
#define CONFIG_FILE DOLPHIN_CONFIG
#else
#define CONFIG_FILE DATA_DIR DIR_SEP USERDATA_DIR DIR_SEP CONFIG_DIR DIR_SEP DOLPHIN_CONFIG
#endif
#endif // PATHS_H
