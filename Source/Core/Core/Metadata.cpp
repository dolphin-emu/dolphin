#include "Metadata.h"
#include <Common/FileUtil.h>
#include <Core/HW/Memmap.h>
#include <processenv.h>
#include <processthreadsapi.h>
#include <WinBase.h>
#include <WinUser.h>
#include <ShlObj_core.h>

// VARIABLES

static tm* matchDateTime;

static u16 controllerPort1;
static u16 controllerPort2;
static u16 controllerPort3;
static u16 controllerPort4;
static std::map<int, u16> controllerMap;

static u32 leftSideCaptainID;
static u32 rightSideCaptainID;
static u32 leftSideSidekickID;
static u32 rightSideSidekickID;
static u32 stadiumID;

// left team
static u16 leftSideScore;
static u32 leftSideShots;
static u16 leftSideHits;
static u16 leftSideSteals;
static u16 leftSideSuperStrikes;
static u16 leftSidePerfectPasses;

// right team
static u16 rightSideScore;
static u32 rightSideShots;
static u16 rightSideHits;
static u16 rightSideSteals;
static u16 rightSideSuperStrikes;
static u16 rightSidePerfectPasses;

// METHODS

std::string Metadata::getJSONString()
{
  char date_string[100];
  strftime(date_string, 50, "%B %d %Y %OH:%OM:%OS", matchDateTime);

  std::stringstream json_stream;
  json_stream << "{" << std::endl;
  json_stream << "  \"Date\": \"" << date_string << "\"," << std::endl;
  json_stream << "  \"Controller Port Info\": {" << std::endl;
  for (int i = 0; i < 4; i++)
  {
    json_stream << "    \"Controller Port " + std::to_string(i) + "\": " << std::to_string(controllerMap.at(i)) << "," << std::endl;
  }
  json_stream << "   }," << std::endl;
  json_stream << "  \"Left Side Captain ID\": \"" << leftSideCaptainID << "\"," << std::endl;
  json_stream << "  \"Left Side Sidekick ID\": \"" << leftSideSidekickID << "\"," << std::endl;
  json_stream << "  \"Right Side Captain ID\": \"" << rightSideCaptainID << "\"," << std::endl;
  json_stream << "  \"Right Side Sidekick ID\": \"" << rightSideSidekickID << "\"," << std::endl;
  json_stream << "  \"Score\": \"" << leftSideScore << "-" << rightSideScore << "\"," << std::endl;
  json_stream << "  \"Stadium ID\": \"" << stadiumID << "\"," << std::endl;

  json_stream << "  \"Left Side Match Stats\": {" << std::endl;
  json_stream << "    \"Goals\": \"" << leftSideScore << "," << std::endl;
  json_stream << "    \"Shots\": \"" << leftSideShots << "," << std::endl;
  json_stream << "    \"Hits\": \"" << leftSideHits << "," << std::endl;
  json_stream << "    \"Steals\": \"" << leftSideSteals << "," << std::endl;
  json_stream << "    \"Super Strikes\": \"" << leftSideSuperStrikes << "," << std::endl;
  json_stream << "    \"Perfect Passes\": \"" << leftSidePerfectPasses << "," << std::endl;
  json_stream << "   }," << std::endl;

  json_stream << "  \"Right Side Match Stats\": {" << std::endl;
  json_stream << "    \"Goals\": \"" << rightSideScore << "," << std::endl;
  json_stream << "    \"Shots\": \"" << rightSideShots << "," << std::endl;
  json_stream << "    \"Hits\": \"" << rightSideHits << "," << std::endl;
  json_stream << "    \"Steals\": \"" << rightSideSteals << "," << std::endl;
  json_stream << "    \"Super Strikes\": \"" << rightSideSuperStrikes << "," << std::endl;
  json_stream << "    \"Perfect Passes\": \"" << rightSidePerfectPasses << "," << std::endl;
  json_stream << "   }," << std::endl;

  json_stream << "}" << std::endl;


  return json_stream.str();
}

std::wstring GetEnvString()
{
  wchar_t* env = GetEnvironmentStrings();
  if (!env)
    abort();
  const wchar_t* var = env;
  size_t totallen = 0;
  size_t len;
  while ((len = wcslen(var)) > 0)
  {
    totallen += len + 1;
    var += len + 1;
  }
  std::wstring result(env, totallen);
  FreeEnvironmentStrings(env);
  return result;
}

void Metadata::writeJSON(std::string jsonString, bool callBatch)
{
  //std::string file_path = "C:\\Users\\Brian\\Desktop\\throw dtm here";
  //file_path += "\\output.json";
  //std::string file_path;

  PWSTR path;
  SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &path);
  std::wstring strpath(path);
  CoTaskMemFree(path);
  std::string documents_file_path(strpath.begin(), strpath.end());
  std::string replays_path = documents_file_path;
  replays_path += "\\Citrus_Replays";
  // C://Users//Brian//Documents//Citrus Replays
  std::string json_output_path = replays_path;
  json_output_path += "\\output.json";

  File::WriteStringToFile(json_output_path, jsonString);

  if (callBatch)
  {
    char date_string[100];
    strftime(date_string, 50, "%B_%d_%Y_%OH_%OM_%OS", matchDateTime);
    std::string someDate(date_string);
    std::string gameVar = " Game_";
    gameVar += someDate;
    // Game_May_05_2022_11_51_34
    gameVar += " ";
    gameVar += replays_path;
    // Game_May_05_2022_11_51_34 C://Users//Brian//Documents//Citrus Replays
    // we need to pass the path the replays are held in in order to CD into them in the batch file
    std::string batchPath("./createcit.bat");
    batchPath += gameVar;
    WinExec(batchPath.c_str(), SW_HIDE);
  }
}

void Metadata::setMatchMetadata(tm* matchDateTimeParam)
{
  // have consistent time across the output file and the in-json time
  matchDateTime = matchDateTimeParam;

  //set match info vars

  //set controllers
  controllerPort1 = Memory::Read_U16(addressControllerPort1);
  controllerPort2 = Memory::Read_U16(addressControllerPort2);
  controllerPort3 = Memory::Read_U16(addressControllerPort3);
  controllerPort4 = Memory::Read_U16(addressControllerPort4);
  controllerMap.insert(std::pair<int, u16>(0, controllerPort1));
  controllerMap.insert(std::pair<int, u16>(1, controllerPort2));
  controllerMap.insert(std::pair<int, u16>(2, controllerPort3));
  controllerMap.insert(std::pair<int, u16>(3, controllerPort4));

  //set captains, sidekicks, and stage
  leftSideCaptainID = Memory::Read_U32(addressLeftSideCaptainID);
  rightSideCaptainID = Memory::Read_U32(addressRightSideCaptainID);
  leftSideSidekickID = Memory::Read_U32(addressLeftSideSidekickID);
  rightSideSidekickID = Memory::Read_U32(addressRightSideSidekickID);
  stadiumID = Memory::Read_U32(addressStadiumID);

  //set left team stats
  leftSideScore = Memory::Read_U16(addressLeftSideScore);
  leftSideShots = Memory::Read_U32(addressLeftSideShots);
  leftSideHits = Memory::Read_U16(addressLeftSideHits);
  leftSideSteals = Memory::Read_U16(addressLeftSideSteals);
  leftSideSuperStrikes = Memory::Read_U16(addressLeftSideSuperStrikes);
  leftSidePerfectPasses = Memory::Read_U16(addressLeftSidePerfectPasses);

  //set right team stats
  rightSideScore = Memory::Read_U16(addressRightSideScore);
  rightSideShots = Memory::Read_U32(addressRightSideShots);
  rightSideHits = Memory::Read_U16(addressRightSideHits);
  rightSideSteals = Memory::Read_U16(addressRightSideSteals);
  rightSideSuperStrikes = Memory::Read_U16(addressRightSideSuperStrikes);
  rightSidePerfectPasses = Memory::Read_U16(addressRightSidePerfectPasses);
}
