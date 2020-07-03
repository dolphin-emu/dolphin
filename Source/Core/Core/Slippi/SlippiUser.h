#pragma once

#include "Common/CommonTypes.h"
#include <atomic>
#include <string>
#include <thread>

class SlippiUser
{
public:
  struct UserInfo
  {
    std::string uid = "";
    std::string playKey = "";
    std::string displayName = "";
    std::string connectCode = "";
    std::string latestVersion = "";
    std::string fileContents = "";
  };

  ~SlippiUser();

  bool AttemptLogin();
  void OpenLogInPage();
  void UpdateFile();
  void UpdateApp();
  void ListenForLogIn();
  void LogOut();
  void OverwriteLatestVersion(std::string version);
  UserInfo GetUserInfo();
  bool IsLoggedIn();
  void FileListenThread();

protected:
  std::string getUserFilePath();
  UserInfo parseFile(std::string fileContents);
  void deleteFile();

  UserInfo userInfo;
  bool isLoggedIn = false;

  std::thread fileListenThread;
  std::atomic<bool> runThread;
};
