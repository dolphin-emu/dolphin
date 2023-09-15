#pragma once
#include <string>
#include <Common/CitrusRequest.h>

class CitrusUser
{
public:
  struct UserInfo
  {
    std::string userId = "";
    std::string discordGlobalName = "";
    std::string discordAvatar = "";
    std::string jwt = "";
    std::string fileContents = "";
  };

  CitrusRequest::LoginError AttemptLogin();
  static bool IsValidUserId(std::string userId);
  void OpenLogInPage();
  UserInfo GetUserInfo();
  bool IsLoggedIn();


protected:
  UserInfo parseFile(std::string fileContents);

  UserInfo userInfo;
  bool isLoggedIn = false;
};
