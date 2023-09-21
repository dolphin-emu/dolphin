#include "SlippiUser.h"

#include "Common/Logging/Log.h"

#include "SlippiRustExtensions.h"

// Takes a RustChatMessages pointer and extracts messages from them, then
// frees the underlying memory safely.
std::vector<std::string> ConvertChatMessagesFromRust(RustChatMessages* rsMessages)
{
  std::vector<std::string> chatMessages;

  for (int i = 0; i < rsMessages->len; i++)
  {
    std::string message = std::string(rsMessages->data[i]);
    chatMessages.push_back(message);
  }

  slprs_user_free_messages(rsMessages);

  return chatMessages;
}

SlippiUser::SlippiUser(uintptr_t rs_exi_device_ptr)
{
  slprs_exi_device_ptr = rs_exi_device_ptr;
}

SlippiUser::~SlippiUser()
{
  // Do nothing, the exi ptr is cleaned up by the exi device
}

bool SlippiUser::AttemptLogin()
{
  return slprs_user_attempt_login(slprs_exi_device_ptr);
}

void SlippiUser::OpenLogInPage()
{
  slprs_user_open_login_page(slprs_exi_device_ptr);
}

void SlippiUser::ListenForLogIn()
{
  slprs_user_listen_for_login(slprs_exi_device_ptr);
}

bool SlippiUser::UpdateApp()
{
  return slprs_user_update_app(slprs_exi_device_ptr);
}

void SlippiUser::LogOut()
{
  slprs_user_logout(slprs_exi_device_ptr);
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
  slprs_user_overwrite_latest_version(slprs_exi_device_ptr, version.c_str());
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
  SlippiUser::UserInfo userInfo;

  RustUserInfo* info = slprs_user_get_info(slprs_exi_device_ptr);
  userInfo.uid = std::string(info->uid);
  userInfo.play_key = std::string(info->play_key);
  userInfo.display_name = std::string(info->display_name);
  userInfo.connect_code = std::string(info->connect_code);
  userInfo.latest_version = std::string(info->latest_version);
  slprs_user_free_info(info);

  return userInfo;
}

std::vector<std::string> SlippiUser::GetDefaultChatMessages()
{
  RustChatMessages* chatMessages = slprs_user_get_default_messages(slprs_exi_device_ptr);
  return ConvertChatMessagesFromRust(chatMessages);
}

std::vector<std::string> SlippiUser::GetUserChatMessages()
{
  RustChatMessages* chatMessages = slprs_user_get_messages(slprs_exi_device_ptr);
  return ConvertChatMessagesFromRust(chatMessages);
}

bool SlippiUser::IsLoggedIn()
{
  return slprs_user_get_is_logged_in(slprs_exi_device_ptr);
}
