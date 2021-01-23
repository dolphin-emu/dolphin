#pragma once

#include <atomic>
#include <curl/curl.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "Common/CommonTypes.h"

class SlippiUser
{
public:
  struct UserInfo
  {
    std::string uid = "";
    std::string play_key = "";
    std::string display_name = "";
    std::string connect_code = "";
    std::string latest_version = "";
    std::string file_contents = "";
  };

  SlippiUser();
  ~SlippiUser();

  bool AttemptLogin();
  void OpenLogInPage();
  void UpdateApp();
  void ListenForLogIn();
  void LogOut();
  void OverwriteLatestVersion(std::string version);
  UserInfo GetUserInfo();
  bool IsLoggedIn();
  void FileListenThread();

protected:
  std::string getUserFilePath();
  UserInfo parseFile(std::string file_contents);
  void deleteFile();
  void overwriteFromServer();

  UserInfo m_user_info;
  bool m_is_logged_in = false;

  const std::string URL_START = "https://users-rest-dot-slippi.uc.r.appspot.com/user";
  CURL* m_curl = nullptr;
  struct curl_slist* m_curl_header_list = nullptr;
  std::vector<char> m_receive_buf;

  std::thread m_file_listen_thread;
  std::atomic<bool> m_run_thread;
};
