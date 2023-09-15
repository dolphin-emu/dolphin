#include "CitrusRequest.h"
#include "HttpRequest.h"
#include <picojson.h>
#include "Logging/Log.h"

static bool PROD_ENABLED = false;

static Common::HttpRequest::Headers headers = {
    {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                   "Gecko) Chrome/97.0.4692.71 Safari/537.36"}};

std::map<CitrusRequest::LoginError, std::string> CitrusRequest::loginErrorMap = {
    {CitrusRequest::LoginError::NoUserFile, "No user file found"},
    {CitrusRequest::LoginError::InvalidUserId, "The user id in your user.json file is not formatted correctly."},
    {CitrusRequest::LoginError::InvalidLogin, "Your user.json file failed to authenticate you."},
    {CitrusRequest::LoginError::ServerError, "We encounterd an error with the server while trying to log you in."},
};

std::string CitrusRequest::GetCurrentBaseURL()
{
  if (PROD_ENABLED)
  {
    return "https://api.mariostrikers.gg";
  }

  return "https://localhost:3000";
  
}

CitrusRequest::LoginError CitrusRequest::LogInUser(std::string userId, std::string jwt)
{
  // The timeout is only for trying to establish the connection to the server
  // If we don't establish a connection with 3 seconds, we error.
  // If we do establish a connection, we are synchronously waiting for the response.
  // We can avoid waiting for a response for other requests by simply not storing the result to a response variable

  Common::HttpRequest req{std::chrono::seconds{3}};

  picojson::object jsonRequestObj;
  jsonRequestObj["userId"] = picojson::value(userId);
  jsonRequestObj["jwt"] = picojson::value(jwt);

  std::string jsonRequestString = picojson::value(jsonRequestObj).serialize();

  std::string loginURL = GetCurrentBaseURL() + "/citrus/" + "verifyLogIn";
  auto resp = req.Post(loginURL, jsonRequestString, headers, Common::HttpRequest::AllowedReturnCodes::All, true);
  if (!resp)
  {
    INFO_LOG_FMT(COMMON, "Login request could not communicate with server");
    return LoginError::ServerError;
  }

  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  INFO_LOG_FMT(COMMON, "Login request JSON response: {}", contents);

  picojson::value json;
  const std::string err = picojson::parse(json, contents); 
  if (!err.empty())
  {
    INFO_LOG_FMT(COMMON, "Invalid JSON received from login request: {}", err);
    return LoginError::ServerError;
  }
  picojson::object obj = json.get<picojson::object>();

  if (obj["valid"].get<std::string>() == "true")
  {
    INFO_LOG_FMT(COMMON, "Login successful");
    return LoginError::NoError;
  }

  return LoginError::ServerError;
}
