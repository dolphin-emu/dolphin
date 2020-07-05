#include "SlippiUser.h"

#ifdef _WIN32
#include "AtlBase.h"
#include "AtlConv.h"
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

#ifdef _WIN32
#define MAX_SYSTEM_PROGRAM (4096)
static void system_hidden(const char* cmd)
{
  PROCESS_INFORMATION p_info;
  STARTUPINFO s_info;

  memset(&s_info, 0, sizeof(s_info));
  memset(&p_info, 0, sizeof(p_info));
  s_info.cb = sizeof(s_info);

  wchar_t utf16cmd[MAX_SYSTEM_PROGRAM] = { 0 };
  MultiByteToWideChar(CP_UTF8, 0, cmd, -1, utf16cmd, MAX_SYSTEM_PROGRAM);
  if (CreateProcessW(NULL, utf16cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &s_info, &p_info))
  {
    DWORD ExitCode;
    WaitForSingleObject(p_info.hProcess, INFINITE);
    GetExitCodeProcess(p_info.hProcess, &ExitCode);
    CloseHandle(p_info.hProcess);
    CloseHandle(p_info.hThread);
  }
}
#endif

static void RunSystemCommand(const std::string& command)
{
#ifdef _WIN32
  _wsystem(UTF8ToUTF16(command).c_str());
#else
  system(command.c_str());
#endif
}

SlippiUser::~SlippiUser()
{
  // Wait for thread to terminate
  runThread = false;
  if (fileListenThread.joinable())
    fileListenThread.join();
}

bool SlippiUser::AttemptLogin()
{
  std::string userFilePath = getUserFilePath();

  INFO_LOG(SLIPPI_ONLINE, "Looking for file at: %s", userFilePath.c_str());

  {
    std::string userFilePathTxt =
      userFilePath + ".txt"; // Put the filename here in its own scope because we don't really need it elsewhere
  // If both files exist we just log they exist and take no further action
    if (File::Exists(userFilePathTxt) && File::Exists(userFilePath))
    {
      INFO_LOG(SLIPPI_ONLINE,
        "Found both .json.txt and .json file for user data. Using .json and ignoring the .json.txt");
    }
    // If only the .txt file exists copy the contents to a json file and delete the text file
    else if (File::Exists(userFilePathTxt))
    {
      // Attempt to copy the txt file to the json file path. If it fails log a warning
      if (!File::Copy(userFilePathTxt, userFilePath))
      {
        WARN_LOG(SLIPPI_ONLINE, "Could not copy file %s to %s", userFilePathTxt.c_str(), userFilePath.c_str());
      }
      // Attempt to delete the txt file. If it fails log an info because this isn't as critical
      if (!File::Delete(userFilePathTxt))
      {
        INFO_LOG(SLIPPI_ONLINE, "Failed to delete %s", userFilePathTxt.c_str());
      }
    }
  }

  // Get user file
  std::string userFileContents;
  File::ReadFileToString(userFilePath, userFileContents);

  userInfo = parseFile(userFileContents);

  isLoggedIn = !userInfo.uid.empty();
  if (isLoggedIn)
  {
    WARN_LOG(SLIPPI_ONLINE, "Found user %s (%s)", userInfo.displayName.c_str(), userInfo.uid.c_str());
  }

  return isLoggedIn;
}

void SlippiUser::OpenLogInPage()
{
#ifdef _WIN32
  std::string folderSep = "%5C";
#else
  std::string folderSep = "%2F";
#endif

  std::string url = "https://slippi.gg/online/enable";
  std::string path = getUserFilePath();
  path = ReplaceAll(path, "\\", folderSep);
  path = ReplaceAll(path, "/", folderSep);
  std::string fullUrl = url + "?path=" + path;

#ifdef _WIN32
  std::string command = "explorer \"" + fullUrl + "\"";
#elif defined(__APPLE__)
  std::string command = "open \"" + fullUrl + "\"";
#else
  std::string command = "xdg-open \"" + fullUrl + "\""; // Linux
#endif

  RunSystemCommand(command);
}

void SlippiUser::UpdateFile()
{
#ifdef _WIN32
  std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
  std::string command = path + " user-update";
  system_hidden(command.c_str());
#elif defined(__APPLE__)
#else
  std::string path = "dolphin-slippi-tools";
  std::string command = path + " user-update";
  system(command.c_str());
#endif
}

void SlippiUser::UpdateApp()
{
#ifdef _WIN32
  auto isoPath = SConfig::GetInstance().m_strBootROM;

  std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
  std::string echoMsg = "echo Starting update process. If nothing happen after a few "
    "minutes, you may need to update manually from https://slippi.gg/netplay ...";
  std::string command = "start /b cmd /c " + echoMsg + " && \"" + path + "\" app-update -launch -iso \"" + isoPath + "\"";
  WARN_LOG(SLIPPI, "Executing app update command: %s", command.c_str());
  RunSystemCommand(command);
#elif defined(__APPLE__)
#else
  const char* appimage_path = getenv("APPIMAGE");
  if (!appimage_path)
  {
    CriticalAlertT("Automatic updates are not available for non-AppImage Linux builds.");
    return;
  }
  std::string path(appimage_path);
  std::string command = "appimageupdatetool " + path;
  WARN_LOG(SLIPPI, "Executing app update command: %s", command.c_str());
  RunSystemCommand(command);
#endif
}

void SlippiUser::ListenForLogIn()
{
  if (runThread)
    return;

  if (fileListenThread.joinable())
    fileListenThread.join();

  runThread = true;
  fileListenThread = std::thread(&SlippiUser::FileListenThread, this);
}

void SlippiUser::LogOut()
{
  runThread = false;
  deleteFile();

  UserInfo emptyUser;
  isLoggedIn = false;
  userInfo = emptyUser;
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
  userInfo.latestVersion = version;
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
  return userInfo;
}

bool SlippiUser::IsLoggedIn()
{
  return isLoggedIn;
}

void SlippiUser::FileListenThread()
{
  while (runThread)
  {
    if (AttemptLogin())
    {
      runThread = false;
      break;
    }

    Common::SleepCurrentThread(500);
  }
}

// On Linux platforms, the user.json file lives in the Sys/ directory in
// order to deal with the fact that we want the configuration for AppImage
// builds to be mutable.
std::string SlippiUser::getUserFilePath()
{
#if defined(__APPLE__)
  std::string dirPath = File::GetBundleDirectory() + "/Contents/Resources";
#elif defined(_WIN32)
  std::string dirPath = File::GetExeDirectory();
#else
  std::string dirPath = File::GetSysDirectory();
#endif
  std::string userFilePath = dirPath + DIR_SEP + "user.json";
  return userFilePath;
}

inline std::string readString(json obj, std::string key)
{
  auto item = obj.find(key);
  if (item == obj.end() || item.value().is_null())
  {
    return "";
  }

  return obj[key];
}

SlippiUser::UserInfo SlippiUser::parseFile(std::string fileContents)
{
  UserInfo info;
  info.fileContents = fileContents;

  auto res = json::parse(fileContents, nullptr, false);
  if (res.is_discarded() || !res.is_object())
  {
    return info;
  }

  info.uid = readString(res, "uid");
  info.displayName = readString(res, "displayName");
  info.playKey = readString(res, "playKey");
  info.connectCode = readString(res, "connectCode");
  info.latestVersion = readString(res, "latestVersion");

  return info;
}

void SlippiUser::deleteFile()
{
  std::string userFilePath = getUserFilePath();
  File::Delete(userFilePath);
}
