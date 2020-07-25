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

#include "Common/Common.h"
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

static size_t receive(char* ptr, size_t size, size_t nmemb, void* rcvBuf)
{
  size_t len = size * nmemb;
  INFO_LOG(SLIPPI_ONLINE, "[User] Received data: %d", len);

  std::string* buf = (std::string*)rcvBuf;

  buf->insert(buf->end(), ptr, ptr + len);

  return len;
}

SlippiUser::SlippiUser()
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);

    // Set up HTTP Headers
    m_curlHeaderList = curl_slist_append(m_curlHeaderList, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_curlHeaderList);

#ifdef _WIN32
    // ALPN support is enabled by default but requires Windows >= 8.1.
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

    m_curl = curl;
  }
}

SlippiUser::~SlippiUser()
{
  // Wait for thread to terminate
  runThread = false;
  if (fileListenThread.joinable())
    fileListenThread.join();

  if (m_curl)
  {
    curl_slist_free_all(m_curlHeaderList);
    curl_easy_cleanup(m_curl);
  }
}

bool SlippiUser::AttemptLogin()
{
  std::string userFilePath = getUserFilePath();

  INFO_LOG(SLIPPI_ONLINE, "Looking for file at: %s", userFilePath.c_str());

  {
    std::string userFilePathTxt =
      userFilePath + ".txt"; // Put the filename here in its own scope because we don't really need it elsewhere
    if (File::Exists(userFilePathTxt))
    {
      // If both files exist we just log they exist and take no further action
      if (File::Exists(userFilePath))
      {
        INFO_LOG(SLIPPI_ONLINE,
          "Found both .json.txt and .json file for user data. Using .json and ignoring the .json.txt");
      }
      // If only the .txt file exists move the contents to a json file and log if it fails
      else if (!File::Rename(userFilePathTxt, userFilePath))
      {
        WARN_LOG(SLIPPI_ONLINE, "Could not move file %s to %s", userFilePathTxt.c_str(), userFilePath.c_str());
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
    overwriteFromServer();
    WARN_LOG(SLIPPI_ONLINE, "Found user %s (%s)", userInfo.displayName.c_str(), userInfo.uid.c_str());
  }

  return isLoggedIn;
}

void SlippiUser::OpenLogInPage()
{
  std::string url = "https://slippi.gg/online/enable";
  std::string path = getUserFilePath();

#ifdef _WIN32
  // On windows, sometimes the path can have backslashes and slashes mixed, convert all to backslashes
  path = ReplaceAll(path, "\\", "\\");
  path = ReplaceAll(path, "/", "\\");
#endif

#ifndef __APPLE__
  char* escapedPath = curl_easy_escape(nullptr, path.c_str(), (int)path.length());
  path = std::string(escapedPath);
  curl_free(escapedPath);
#endif

  std::string fullUrl = url + "?path=" + path;

  INFO_LOG(SLIPPI_ONLINE, "[User] Login at path: %s", fullUrl.c_str());

#ifdef _WIN32
  std::string command = "explorer \"" + fullUrl + "\"";
#elif defined(__APPLE__)
  std::string command = "open \"" + fullUrl + "\"";
#else
  std::string command = "xdg-open \"" + fullUrl + "\""; // Linux
#endif

  RunSystemCommand(command);
}

void SlippiUser::UpdateApp()
{
#ifdef _WIN32
  auto isoPath = SConfig::GetInstance().m_strFilename;

  std::string path = File::GetExeDirectory() + "/dolphin-slippi-tools.exe";
  std::string echoMsg = "echo Starting update process. If nothing happen after a few "
    "minutes, you may need to update manually from https://slippi.gg/netplay ...";
  // std::string command =
  //    "start /b cmd /c " + echoMsg + " && \"" + path + "\" app-update -launch -iso \"" + isoPath + "\"";
  std::string command = "start /b cmd /c " + echoMsg + " && \"" + path + "\" app-update -launch -iso \"" + isoPath +
    "\" -version \"" + scm_slippi_semver_str + "\"";
  WARN_LOG(SLIPPI, "Executing app update command: %s", command);
  RunSystemCommand(command);
#elif defined(__APPLE__)
#else
  const char* appimage_path = getenv("APPIMAGE");
  const char* appmount_path = getenv("APPDIR");
  if (!appimage_path)
  {
    CriticalAlertT("Automatic updates are not available for non-AppImage Linux builds.");
    return;
  }
  std::string path(appimage_path);
  std::string mount_path(appmount_path);
  std::string command = mount_path + "/usr/bin/appimageupdatetool " + path;
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

// On Linux platforms, the user.json file lives in the XDG_CONFIG_HOME/SlippiOnline
// directory in order to deal with the fact that we want the configuration for AppImage
// builds to be mutable.
std::string SlippiUser::getUserFilePath()
{
#if defined(__APPLE__)
  std::string userFilePath = File::GetBundleDirectory() + "/Contents/Resources" + DIR_SEP + "user.json";
#elif defined(_WIN32)
  std::string userFilePath = File::GetExeDirectory() + DIR_SEP + "user.json";
#else
  std::string userFilePath = File::GetUserPath(F_USERJSON_IDX);
#endif
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

void SlippiUser::overwriteFromServer()
{
  if (!m_curl)
    return;

  // Perform curl request
  std::string resp;
  curl_easy_setopt(m_curl, CURLOPT_URL, (URL_START + "/" + userInfo.uid).c_str());
  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &resp);
  CURLcode res = curl_easy_perform(m_curl);

  if (res != 0)
  {
    ERROR_LOG(SLIPPI, "[User] Error fetching user info from server, code: %d", res);
    return;
  }

  long responseCode;
  curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
  if (responseCode != 200)
  {
    ERROR_LOG(SLIPPI, "[User] Server responded with non-success status: %d", responseCode);
    return;
  }

  // Overwrite userInfo with data from server
  auto r = json::parse(resp);
  userInfo.connectCode = r.value("connectCode", userInfo.connectCode);
  userInfo.latestVersion = r.value("latestVersion", userInfo.latestVersion);

  // TODO: Once it's possible to change Display name from website, uncomment below
  // userInfo.displayName = r.value("displayName", userInfo.displayName);
}
