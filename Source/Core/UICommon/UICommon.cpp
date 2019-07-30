// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <clocale>
#include <cmath>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#ifdef _WIN32
#include <shlobj.h>  // for SHGetFolderPath
#endif

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/Wiimote.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"

#include "InputCommon/GCAdapter.h"

#include "UICommon/DiscordPresence.h"
#include "UICommon/UICommon.h"
#include "UICommon/USBUtils.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "UICommon/X11Utils.h"
#endif

#ifdef __APPLE__
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif

#include "VideoCommon/VideoBackendBase.h"

namespace UICommon
{
static void CreateDumpPath(const std::string& path)
{
  if (!path.empty())
    File::SetUserPath(D_DUMP_IDX, path + '/');
  File::CreateFullPath(File::GetUserPath(D_DUMPAUDIO_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPFRAMES_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPOBJECTS_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
}

static void InitCustomPaths()
{
  File::SetUserPath(D_WIIROOT_IDX, Config::Get(Config::MAIN_FS_PATH));
  CreateDumpPath(Config::Get(Config::MAIN_DUMP_PATH));
  const std::string sd_path = Config::Get(Config::MAIN_SD_PATH);
  if (!sd_path.empty())
    File::SetUserPath(F_WIISDCARD_IDX, sd_path);
}

void Init()
{
  Config::Init();
  Config::AddConfigChangedCallback(InitCustomPaths);
  Config::AddLayer(ConfigLoaders::GenerateBaseConfigLoader());
  SConfig::Init();
#ifdef USE_DISCORD_PRESENCE
  Discord::Init();
#endif
  LogManager::Init();
  VideoBackendBase::PopulateList();
  WiimoteReal::LoadSettings();
  GCAdapter::Init();
  VideoBackendBase::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

  Common::SetEnableAlert(SConfig::GetInstance().bUsePanicHandlers);
}

void Shutdown()
{
  GCAdapter::Shutdown();
  WiimoteReal::Shutdown();
  VideoBackendBase::ClearList();
  LogManager::Shutdown();
#ifdef USE_DISCORD_PRESENCE
  Discord::Shutdown();
#endif
  SConfig::Shutdown();
  Config::Shutdown();
}

void SetLocale(std::string locale_name)
{
  auto set_locale = [](const std::string& locale) {
#ifdef __linux__
    std::string adjusted_locale = locale;
    if (!locale.empty())
      adjusted_locale += ".UTF-8";
#else
    const std::string& adjusted_locale = locale;
#endif

    // setlocale sets the C locale, and global sets the C and C++ locales, so the call to setlocale
    // would be redundant if it wasn't for not having any other good way to check whether
    // the locale name is valid. (Constructing a std::locale object for an unsupported
    // locale name throws std::runtime_error, and exception handling is disabled in Dolphin.)
    if (!std::setlocale(LC_ALL, adjusted_locale.c_str()))
      return false;
    std::locale::global(std::locale(adjusted_locale));
    return true;
  };

#ifdef _WIN32
  constexpr char PREFERRED_SEPARATOR = '-';
  constexpr char OTHER_SEPARATOR = '_';
#else
  constexpr char PREFERRED_SEPARATOR = '_';
  constexpr char OTHER_SEPARATOR = '-';
#endif

  // Users who use a system language other than English are unlikely to prefer American date and
  // time formats, so let's explicitly request "en_GB" if Dolphin's language is set to "en".
  // (The settings window only allows setting "en", not anything like "en_US" or "en_GB".)
  // Users who prefer the American formats are likely to have their system language set to en_US,
  // and are thus likely to leave Dolphin's language as the default value "" (<System Language>).
  if (locale_name == "en")
    locale_name = "en_GB";

  std::replace(locale_name.begin(), locale_name.end(), OTHER_SEPARATOR, PREFERRED_SEPARATOR);

  // Use the specified locale if supported.
  if (set_locale(locale_name))
    return;

  // Remove subcodes until we get a supported locale. If that doesn't give us a supported locale,
  // "" is passed to set_locale in order to get the system default locale.
  while (!locale_name.empty())
  {
    const size_t separator_index = locale_name.rfind(PREFERRED_SEPARATOR);
    locale_name.erase(separator_index == std::string::npos ? 0 : separator_index);
    if (set_locale(locale_name))
      return;
  }

  // If none of the locales tried above are supported, we just keep using whatever locale is set
  // (which is the classic locale by default).
}

void CreateDirectories()
{
  File::CreateFullPath(File::GetUserPath(D_RESOURCEPACK_IDX));
  File::CreateFullPath(File::GetUserPath(D_USER_IDX));
  File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_COVERCACHE_IDX));
  File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPSSL_IDX));
  File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
  File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
  File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
  File::CreateFullPath(File::GetUserPath(D_MAPS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
  File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
  File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
#ifndef ANDROID
  File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));
  File::CreateFullPath(File::GetUserPath(D_STYLES_IDX));
#else
  // Disable media scanning in directories with a .nomedia file
  File::CreateEmptyFile(File::GetUserPath(D_COVERCACHE_IDX) + DIR_SEP NOMEDIA_FILE);
#endif
}

void SetUserDirectory(const std::string& custom_path)
{
  if (!custom_path.empty())
  {
    File::CreateFullPath(custom_path + DIR_SEP);
    File::SetUserPath(D_USER_IDX, custom_path + DIR_SEP);
    return;
  }

  std::string user_path;
#ifdef _WIN32
  // Detect where the User directory is. There are five different cases
  // (on top of the command line flag, which overrides all this):
  // 1. GetExeDirectory()\portable.txt exists
  //    -> Use GetExeDirectory()\User
  // 2. HKCU\Software\Dolphin Emulator\LocalUserConfig exists and is true
  //    -> Use GetExeDirectory()\User
  // 3. HKCU\Software\Dolphin Emulator\UserConfigPath exists
  //    -> Use this as the user directory path
  // 4. My Documents exists
  //    -> Use My Documents\Dolphin Emulator as the User directory path
  // 5. Default
  //    -> Use GetExeDirectory()\User

  // Check our registry keys
  HKEY hkey;
  DWORD local = 0;
  TCHAR configPath[MAX_PATH] = {0};
  if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Dolphin Emulator"), 0, KEY_QUERY_VALUE,
                   &hkey) == ERROR_SUCCESS)
  {
    DWORD size = 4;
    if (RegQueryValueEx(hkey, TEXT("LocalUserConfig"), nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&local), &size) != ERROR_SUCCESS)
      local = 0;

    size = MAX_PATH;
    if (RegQueryValueEx(hkey, TEXT("UserConfigPath"), nullptr, nullptr, (LPBYTE)configPath,
                        &size) != ERROR_SUCCESS)
      configPath[0] = 0;
    RegCloseKey(hkey);
  }

  local = local || File::Exists(File::GetExeDirectory() + DIR_SEP "portable.txt");

  // Get Program Files path in case we need it.
  TCHAR my_documents[MAX_PATH];
  bool my_documents_found = SUCCEEDED(
      SHGetFolderPath(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, my_documents));

  if (local)  // Case 1-2
    user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;
  else if (configPath[0])  // Case 3
    user_path = TStrToUTF8(configPath);
  else if (my_documents_found)  // Case 4
    user_path = TStrToUTF8(my_documents) + DIR_SEP "Dolphin Emulator" DIR_SEP;
  else  // Case 5
    user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;

  // Prettify the path: it will be displayed in some places, we don't want a mix
  // of \ and /.
  user_path = ReplaceAll(user_path, "\\", DIR_SEP);

  // Make sure it ends in DIR_SEP.
  if (*user_path.rbegin() != DIR_SEP_CHR)
    user_path += DIR_SEP;

#else
  if (File::IsDirectory(ROOT_DIR DIR_SEP USERDATA_DIR))
  {
    user_path = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
  }
  else
  {
    const char* env_path = getenv("DOLPHIN_EMU_USERPATH");
    const char* home = getenv("HOME");
    if (!home)
      home = getenv("PWD");
    if (!home)
      home = "";
    std::string home_path = std::string(home) + DIR_SEP;

#if defined(__APPLE__) || defined(ANDROID)
    if (env_path)
    {
      user_path = env_path;
    }
    else
    {
      user_path = home_path + DOLPHIN_DATA_DIR DIR_SEP;
    }
#else
    // We are on a non-Apple and non-Android POSIX system, there are 4 cases:
    // 1. GetExeDirectory()/portable.txt exists
    //    -> Use GetExeDirectory()/User
    // 2. $DOLPHIN_EMU_USERPATH is set
    //    -> Use $DOLPHIN_EMU_USERPATH
    // 3. ~/.dolphin-emu directory exists
    //    -> Use ~/.dolphin-emu
    // 4. Default
    //    -> Use XDG basedir, see
    //    http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
    user_path = home_path + "." DOLPHIN_DATA_DIR DIR_SEP;
    std::string exe_path = File::GetExeDirectory();
    if (File::Exists(exe_path + DIR_SEP "portable.txt"))
    {
      user_path = exe_path + DIR_SEP "User" DIR_SEP;
    }
    else if (env_path)
    {
      user_path = env_path;
    }
    else if (!File::Exists(user_path))
    {
      const char* data_home = getenv("XDG_DATA_HOME");
      std::string data_path =
          std::string(data_home && data_home[0] == '/' ? data_home :
                                                         (home_path + ".local" DIR_SEP "share")) +
          DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;

      const char* config_home = getenv("XDG_CONFIG_HOME");
      std::string config_path =
          std::string(config_home && config_home[0] == '/' ? config_home :
                                                             (home_path + ".config")) +
          DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;

      const char* cache_home = getenv("XDG_CACHE_HOME");
      std::string cache_path =
          std::string(cache_home && cache_home[0] == '/' ? cache_home : (home_path + ".cache")) +
          DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;

      File::SetUserPath(D_USER_IDX, data_path);
      File::SetUserPath(D_CONFIG_IDX, config_path);
      File::SetUserPath(D_CACHE_IDX, cache_path);
      return;
    }
#endif
  }
#endif
  File::SetUserPath(D_USER_IDX, user_path);
}

void SaveWiimoteSources()
{
  std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + WIIMOTE_INI_NAME ".ini";

  IniFile inifile;
  inifile.Load(ini_filename);

  for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
  {
    std::string secname("Wiimote");
    secname += (char)('1' + i);
    IniFile::Section& sec = *inifile.GetOrCreateSection(secname);

    sec.Set("Source", (int)g_wiimote_sources[i]);
  }

  std::string secname("BalanceBoard");
  IniFile::Section& sec = *inifile.GetOrCreateSection(secname);
  sec.Set("Source", (int)g_wiimote_sources[WIIMOTE_BALANCE_BOARD]);

  inifile.Save(ini_filename);
}

bool TriggerSTMPowerEvent()
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return false;

  const auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (!stm || !std::static_pointer_cast<IOS::HLE::Device::STMEventHook>(stm)->HasHookInstalled())
    return false;

  Core::DisplayMessage("Shutting down", 30000);
  ProcessorInterface::PowerButton_Tap();

  return true;
}

#if defined(HAVE_XRANDR) && HAVE_X11
void EnableScreenSaver(Window win, bool enable)
#else
void EnableScreenSaver(bool enable)
#endif
{
  // Inhibit the screensaver. Depending on the operating system this may also
  // disable low-power states and/or screen dimming.

#if defined(HAVE_X11) && HAVE_X11
  if (Config::Get(Config::MAIN_DISABLE_SCREENSAVER))
  {
    X11Utils::InhibitScreensaver(win, !enable);
  }
#endif

#ifdef _WIN32
  // Prevents Windows from sleeping, turning off the display, or idling
  if (enable)
  {
    SetThreadExecutionState(ES_CONTINUOUS);
  }
  else
  {
    EXECUTION_STATE should_screen_save =
        Config::Get(Config::MAIN_DISABLE_SCREENSAVER) ? ES_DISPLAY_REQUIRED : 0;
    SetThreadExecutionState(ES_CONTINUOUS | should_screen_save | ES_SYSTEM_REQUIRED);
  }
#endif

#ifdef __APPLE__
  static IOPMAssertionID s_power_assertion = kIOPMNullAssertionID;

  if (Config::Get(Config::MAIN_DISABLE_SCREENSAVER))
  {
    if (enable)
    {
      if (s_power_assertion != kIOPMNullAssertionID)
      {
        IOPMAssertionRelease(s_power_assertion);
        s_power_assertion = kIOPMNullAssertionID;
      }
    }
    else
    {
      CFStringRef reason_for_activity = CFSTR("Emulation Running");
      if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep,
                                      kIOPMAssertionLevelOn, reason_for_activity,
                                      &s_power_assertion) != kIOReturnSuccess)
      {
        s_power_assertion = kIOPMNullAssertionID;
      }
    }
  }
#endif
}

std::string FormatSize(u64 bytes)
{
  // i18n: The symbol for the unit "bytes"
  const char* const unit_symbols[] = {_trans("B"),   _trans("KiB"), _trans("MiB"), _trans("GiB"),
                                      _trans("TiB"), _trans("PiB"), _trans("EiB")};

  // Find largest power of 2 less than size.
  // div 10 to get largest named unit less than size
  // 10 == log2(1024) (number of B in a KiB, KiB in a MiB, etc)
  // Max value is 63 / 10 = 6
  const int unit = IntLog2(std::max<u64>(bytes, 1)) / 10;

  // Don't need exact values, only 5 most significant digits
  const double unit_size = std::pow(2, unit * 10);
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2);
  ss << bytes / unit_size << ' ' << Common::GetStringT(unit_symbols[unit]);
  return ss.str();
}

}  // namespace UICommon
