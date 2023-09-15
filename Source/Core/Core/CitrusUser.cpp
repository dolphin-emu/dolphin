#include "CitrusUser.h"
#include "Common/CitrusRequest.h"

#include "json.hpp"
#include <Common/FileUtil.h>
#include <regex>
using json = nlohmann::json;

CitrusRequest::LoginError CitrusUser::AttemptLogin()
{
  // check for user config file
  std::string userConfigPath = File::GetExeDirectory() + "/" + "user.json";

  // make function to validate user login and return different errors

  if (!File::Exists(userConfigPath))
  {
    return CitrusRequest::LoginError::NoUserFile;
  }

  std::string userFileContents;
  File::ReadFileToString(userConfigPath, userFileContents);
  userInfo = parseFile(userFileContents);

  // validate user now by calling a validateuser function that does multiple checks
  // we will still need to validate server side every time we make a call to the api

  if (!IsValidUserId(userInfo.userId))
  {
    return CitrusRequest::LoginError::InvalidUserId;
  }

  auto res = CitrusRequest::LogInUser(userInfo.userId, userInfo.jwt);
  if (res != CitrusRequest::LoginError::NoError)
  {
    return res;
  }

  return CitrusRequest::LoginError::NoError;
  //INFO_LOG_FMT(CORE, "User discord Id is {}", readString(res, "discordId"));
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


CitrusUser::UserInfo CitrusUser::parseFile(std::string fileContents)
{
  UserInfo info;
  info.fileContents = fileContents;

  auto res = json::parse(fileContents, nullptr, false);
  if (res.is_discarded() || !res.is_object())
  {
    return info;
  }

  info.userId = readString(res, "discordId");
  info.discordGlobalName = readString(res, "discordGlobalName");
  info.discordAvatar = readString(res, "discordAvatar");
  info.jwt = readString(res, "jwt");

  return info;

}

bool CitrusUser::IsValidUserId(std::string userId)
{
  // userIds are just Discord/Twitter Snowflake IDs
  // They must be between 17-20 digits and completely numerical
  if (!std::regex_match(userId, std::regex("\\d{17,20}")))
  {
    return false;
  }

  return true;
}
