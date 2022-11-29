#include "Metadata.h"
#include <Common/FileUtil.h>
#include <Core/HW/Memmap.h>
#include <Core/HW/AddressSpace.h>
#include <../minizip/mz_compat.h>
#include <codecvt>
#include "zip.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include <Core/StateAuxillary.h>

struct ItemStruct
{
  u8 itemID;
  u8 itemAmount;
  float itemTime;
};

struct GoalStruct
{
  float goalTime;
  float ballX;
  float ballY;
};

struct MissedShots
{
  float goalTime;
  float ballX;
  float ballY;
};

// VARIABLES

static tm* matchDateTime;
static int homeTeamPossesionFrameCount;
static int awayTeamPossesionFrameCount;
static std::string playerName = "";
std::vector<const NetPlay::Player*> playerArray;
static NetPlay::PadMappingArray netplayGCMap;

static u16 controllerPort1;
static u16 controllerPort2;
static u16 controllerPort3;
static u16 controllerPort4;
static std::vector<int> controllerVector(4);
static std::vector<std::string> leftTeamPlayerNamesVector;
static std::vector<std::string> rightTeamPlayerNamesVector;

static u32 leftSideCaptainID;
static u32 rightSideCaptainID;
static u32 leftSideSidekickID;
static u32 rightSideSidekickID;
static u32 stadiumID;
static u32 overtimeNotReached;
static u32 timeElapsed;

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

// ruleset
static u32 matchDifficulty;
static u32 matchTimeAllotted;
static u8 matchItemsBool;
static u8 matchSuperStrikesBool;
static u8 matchBowserBool;
static std::array<u8, 16> md5Hash;

// items
static u32 leftTeamItemOffset;
static u32 leftTeamItemCount;
static u32 rightTeamItemOffset;
static u32 rightTeamItemCount;
static std::vector<ItemStruct> leftTeamItemVector;
static std::vector<ItemStruct> rightTeamItemVector;

// goals
static u32 leftTeamGoalOffset;
static u32 rightTeamGoalOffset;
static std::vector<GoalStruct> leftTeamGoalVector;
static std::vector<GoalStruct> rightTeamGoalVector;

// missed shots
static u16 leftTeamMissedShotsOffset;
static u16 rightTeamMissedShotsOffset;
static u16 leftTeamMissedShotsFlag;
static u16 rightTeamMissedShotsFlag;
static std::vector<MissedShots> leftTeamMissedShotsVector;
static std::vector<MissedShots> rightTeamMissedShotsVector;

// METHODS

std::string Metadata::getJSONString()
{
  const AddressSpace::Accessors* accessors =
      AddressSpace::GetAccessors(AddressSpace::Type::Effective);
  char date_string[100];
  char file_name[100];
  strftime(date_string, 50, "%B %d %Y %OH:%OM:%OS", matchDateTime);
  strftime(file_name, 50, "%B_%d_%Y_%OH_%OM_%OS", matchDateTime);
  std::string convertedDate(file_name);
  std::string file_name_string = "Game_" + convertedDate + ".cit";

  std::stringstream json_stream;
  json_stream << "{" << std::endl;
  json_stream << "  \"File Name\": \"" << file_name_string << "\"," << std::endl;
  json_stream << "  \"Date\": \"" << date_string << "\"," << std::endl;
  std::string md5String = "";
  for (int i = 0; i < md5Hash.size(); i++)
  {
    md5String += std::format("{:x}", md5Hash[i]);
  }
  json_stream << "  \"Game Hash\": \"" << md5String << "\"," << std::endl;
  json_stream << "  \"Controller Port Info\": {" << std::endl;
  for (int i = 0; i < 4; i++)
  {
    if (i != 3)
    {
      json_stream << "    \"Controller Port " + std::to_string(i) + "\": " << std::to_string(controllerVector.at(i)) << "," << std::endl;
    }
    else
    {
      // no comma cuz end
      json_stream << "    \"Controller Port " + std::to_string(i) + "\": " << std::to_string(controllerVector.at(i)) << std::endl;
    }
  }
  json_stream << "   }," << std::endl;
  json_stream << "  \"Left Side Captain ID\": \"" << leftSideCaptainID << "\"," << std::endl;
  json_stream << "  \"Left Side Sidekick ID\": \"" << leftSideSidekickID << "\"," << std::endl;
  json_stream << "  \"Right Side Captain ID\": \"" << rightSideCaptainID << "\"," << std::endl;
  json_stream << "  \"Right Side Sidekick ID\": \"" << rightSideSidekickID << "\"," << std::endl;
  json_stream << "  \"Score\": \"" << leftSideScore << "-" << rightSideScore << "\"," << std::endl;
  json_stream << "  \"Stadium ID\": \"" << stadiumID << "\"," << std::endl;

  json_stream << "  \"Left Side Match Stats\": {" << std::endl;
  json_stream << "    \"Goals\": \"" << leftSideScore << "\"," << std::endl;
  json_stream << "    \"Shots\": \"" << leftSideShots << "\"," << std::endl;
  json_stream << "    \"Hits\": \"" << leftSideHits << "\"," << std::endl;
  json_stream << "    \"Steals\": \"" << leftSideSteals << "\"," << std::endl;
  json_stream << "    \"Super Strikes\": \"" << leftSideSuperStrikes << "\"," << std::endl;
  json_stream << "    \"Perfect Passes\": \"" << leftSidePerfectPasses << "\"" << std::endl;
  json_stream << "   }," << std::endl;

  json_stream << "  \"Right Side Match Stats\": {" << std::endl;
  json_stream << "    \"Goals\": \"" << rightSideScore << "\"," << std::endl;
  json_stream << "    \"Shots\": \"" << rightSideShots << "\"," << std::endl;
  json_stream << "    \"Hits\": \"" << rightSideHits << "\"," << std::endl;
  json_stream << "    \"Steals\": \"" << rightSideSteals << "\"," << std::endl;
  json_stream << "    \"Super Strikes\": \"" << rightSideSuperStrikes << "\"," << std::endl;
  json_stream << "    \"Perfect Passes\": \"" << rightSidePerfectPasses << "\"" << std::endl;
  json_stream << "   }," << std::endl;

  json_stream << "  \"Netplay Match\": \"" << NetPlay::IsNetPlayRunning() << "\"," << std::endl;

  json_stream << "  \"Left Team Player Info\": [" << std::endl;
  for (int i = 0; i < leftTeamPlayerNamesVector.size(); i++)
  {
    if (i != leftTeamPlayerNamesVector.size() - 1)
    {
      json_stream << "    [" << "\"" + leftTeamPlayerNamesVector.at(i) + "\"" << "]" << "," << std::endl;
    }
    else
    {
      json_stream << "    [" << "\"" + leftTeamPlayerNamesVector.at(i) + "\"" << "]" << std::endl;
    }
  }
  json_stream << "  ]," << std::endl;
  json_stream << "  \"Right Team Player Info\": [" << std::endl;
  for (int i = 0; i < rightTeamPlayerNamesVector.size(); i++)
  {
    if (i != rightTeamPlayerNamesVector.size() - 1)
    {
      json_stream << "    [" << "\"" + rightTeamPlayerNamesVector.at(i) + "\"" << "]" << "," << std::endl;
    }
    else
    {
      json_stream << "    [" << "\"" + rightTeamPlayerNamesVector.at(i) + "\"" << "]" << std::endl;
    }
  }
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Overtime Not Reached\": \"" << overtimeNotReached << "\"," << std::endl;
  json_stream << "  \"Match Time Allotted\": \"" << matchTimeAllotted << "\"," << std::endl;
  json_stream << "  \"Match Time Elapsed\": \"" << accessors->ReadF32(addressTimeElapsed) << "\","
              << std::endl;
  json_stream << "  \"Match Difficulty\": \"" << matchDifficulty << "\"," << std::endl;
  json_stream << "  \"Match Items\": \"" << std::to_string(matchItemsBool) << "\"," << std::endl;
  json_stream << "  \"Match Super Strikes\": \"" << std::to_string(matchSuperStrikesBool) << "\","
              << std::endl;
  json_stream << "  \"Match Bowser or FTX\": \"" << std::to_string(matchBowserBool) << "\","
              << std::endl;

  json_stream << "  \"Left Team Item Count\": \"" << leftTeamItemCount << "\"," << std::endl;
  json_stream << "  \"Left Team Item Info\": [" << std::endl;
  for (int i = 0; i < leftTeamItemVector.size(); i++)
  {
    if (i != leftTeamItemVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(leftTeamItemVector.at(i).itemID) << ","
                  << std::to_string(leftTeamItemVector.at(i).itemAmount) << ","
                  << leftTeamItemVector.at(i).itemTime << "]"
                  << "," << std::endl; 
    }
    else
    {
      json_stream << "    [" << std::to_string(leftTeamItemVector.at(i).itemID) << ","
                  << std::to_string(leftTeamItemVector.at(i).itemAmount) << ","
                  << leftTeamItemVector.at(i).itemTime << "]"
                  << std::endl; 
    }
  }
  // add a comma once we add right team items to line below
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Right Team Item Count\": \"" << rightTeamItemCount << "\"," << std::endl;
  json_stream << "  \"Right Team Item Info\": [" << std::endl;
  for (int i = 0; i < rightTeamItemVector.size(); i++)
  {
    if (i != rightTeamItemVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(rightTeamItemVector.at(i).itemID) << ","
                  << std::to_string(rightTeamItemVector.at(i).itemAmount) << ","
                  << rightTeamItemVector.at(i).itemTime << "]"
                  << "," << std::endl;
    }
    else
    {
      json_stream << "    [" << std::to_string(rightTeamItemVector.at(i).itemID) << ","
                  << std::to_string(rightTeamItemVector.at(i).itemAmount) << ","
                  << rightTeamItemVector.at(i).itemTime << "]" << std::endl;
    }
  }
  // add a comma below if we add more stats
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Left Team Goal Info\": [" << std::endl;
  for (int i = 0; i < leftTeamGoalVector.size(); i++)
  {
    if (i != leftTeamGoalVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(leftTeamGoalVector.at(i).goalTime) << ","
                  << std::to_string(leftTeamGoalVector.at(i).ballX) << ","
                  << std::to_string(leftTeamGoalVector.at(i).ballY)
                  << "],"
                  << std::endl;
    }
    else
    {
      json_stream << "    [" << std::to_string(leftTeamGoalVector.at(i).goalTime) << ","
                  << std::to_string(leftTeamGoalVector.at(i).ballX) << ","
                  << std::to_string(leftTeamGoalVector.at(i).ballY)
                  << "]"
                  << std::endl;
    }
  }
  // add a comma once we add right team goals to line below
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Right Team Goal Info\": [" << std::endl;
  for (int i = 0; i < rightTeamGoalVector.size(); i++)
  {
    if (i != rightTeamGoalVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(rightTeamGoalVector.at(i).goalTime) << ","
                  << std::to_string(rightTeamGoalVector.at(i).ballX) << ","
                  << std::to_string(rightTeamGoalVector.at(i).ballY)
                  << "],"
                  << std::endl;
    }
    else
    {
      json_stream << "    [" << std::to_string(rightTeamGoalVector.at(i).goalTime) << ","
                  << std::to_string(rightTeamGoalVector.at(i).ballX) << ","
                  << std::to_string(rightTeamGoalVector.at(i).ballY)
                  << "]"
                  << std::endl;
    }
  }
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Left Team Missed Shots Info\": [" << std::endl;
  for (int i = 0; i < leftTeamMissedShotsVector.size(); i++)
  {
    if (i != leftTeamMissedShotsVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(leftTeamMissedShotsVector.at(i).goalTime) << ","
                  << std::to_string(leftTeamMissedShotsVector.at(i).ballX) << ","
                  << std::to_string(leftTeamMissedShotsVector.at(i).ballY) << "]," << std::endl;
    }
    else
    {
      json_stream << "    [" << std::to_string(leftTeamMissedShotsVector.at(i).goalTime) << ","
                  << std::to_string(leftTeamMissedShotsVector.at(i).ballX) << ","
                  << std::to_string(leftTeamMissedShotsVector.at(i).ballY) << "]" << std::endl;
    }
  }
  // add a comma once we add right team goals to line below
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Right Team Missed Shots Info\": [" << std::endl;
  for (int i = 0; i < rightTeamMissedShotsVector.size(); i++)
  {
    if (i != rightTeamMissedShotsVector.size() - 1)
    {
      json_stream << "    [" << std::to_string(rightTeamMissedShotsVector.at(i).goalTime) << ","
                  << std::to_string(rightTeamMissedShotsVector.at(i).ballX) << ","
                  << std::to_string(rightTeamMissedShotsVector.at(i).ballY) << "]," << std::endl;
    }
    else
    {
      json_stream << "    [" << std::to_string(rightTeamMissedShotsVector.at(i).goalTime) << ","
                  << std::to_string(rightTeamMissedShotsVector.at(i).ballX) << ","
                  << std::to_string(rightTeamMissedShotsVector.at(i).ballY) << "]" << std::endl;
    }
  }
  // add a comma below if we add more stats
  json_stream << "  ]," << std::endl;

  json_stream << "  \"Left Team Frames Possessed Ball\": \"" << homeTeamPossesionFrameCount << "\","
              << std::endl;
  json_stream << "  \"Right Team Frames Possessed Ball\": \"" << awayTeamPossesionFrameCount << "\""
              << std::endl;

  json_stream << "}" << std::endl;


  return json_stream.str();
}

void Metadata::writeJSON(std::string jsonString, bool callBatch)
{
  //std::string file_path = "C:\\Users\\Brian\\Desktop\\throw dtm here";
  //file_path += "\\output.json";
  //std::string file_path;
  /*
  PWSTR path;
  SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &path);
  std::wstring strpath(path);
  CoTaskMemFree(path);
  std::string documents_file_path(strpath.begin(), strpath.end());
  std::string replays_path = "\"";
  replays_path += documents_file_path;                   
  replays_path += "\\Citrus Replays";
  // "C://Users//Brian//Documents//Citrus Replays
  std::string json_output_path = documents_file_path + "\\Citrus Replays";
  json_output_path += "\\output.json";
  replays_path += "\"";
  // "C://Users//Brian//Documents//Citrus Replays"
  */
  std::string improvedReplayPath = File::GetUserPath(D_CITRUSREPLAYS_IDX) + "output.json";
  File::WriteStringToFile(improvedReplayPath, jsonString);

  if (callBatch)
  {
    char date_string[100];
    strftime(date_string, 50, "%B_%d_%Y_%OH_%OM_%OS", matchDateTime);
    std::string someDate(date_string);
    std::string gameTime = "Game_" + someDate;
    /*
    std::string gameVar = " Game_";
    gameVar += someDate;
    // Game_May_05_2022_11_51_34
    gameVar += " ";
    gameVar += replays_path;
    // Game_May_05_2022_11_51_34 C://Users//Brian//Documents//Citrus Replays
    // we need to pass the path the replays are held in in order to CD into them in the batch file
    std::filesystem::path cwd = std::filesystem::current_path() / "createcit.bat";
    std::string pathToBatch = cwd.string();
    std::string batchPath = "\"\"" + pathToBatch + "\"";
    //std::string batchPath("./createcit.bat");
    batchPath += gameVar + "\"";
    */
    /*
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    //si.wShowWindow = SW_HIDE;
    // CREATE_NO_WINDOW
    if (!CreateProcessA(pathToBatch.c_str(), &batchPath[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                   NULL, (LPSTARTUPINFOA)&si, &pi))
    {
      // would handle error in here
    }
    // the task has ended so close the handle
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    */
    //WinExec(batchPath.c_str(), SW_HIDE);
    // https://stackoverflow.com/questions/11370908/how-do-i-use-minizip-on-zlib
    std::vector<std::wstring> paths;
    std::string exampleFile1 = File::GetUserPath(D_CITRUSREPLAYS_IDX) + "output.dtm.sav";
    for (char& c : exampleFile1)
    {
      if (c == '/')
        c = '\\';
    }
    std::string exampleFile2 = File::GetUserPath(D_CITRUSREPLAYS_IDX) + "output.dtm";
    for (char& c : exampleFile2)
    {
      if (c == '/')
        c = '\\';
    }
    std::string exampleFile3 = File::GetUserPath(D_CITRUSREPLAYS_IDX) + "output.json";
    for (char& c : exampleFile3)
    {
      if (c == '/')
        c = '\\';
    }
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile1));
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile2));
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile3));
    std::string zipName = File::GetUserPath(D_CITRUSREPLAYS_IDX) + gameTime + ".cit ";

    zipFile zf = zipOpen(zipName.c_str(), APPEND_STATUS_CREATE);
    if (zf == NULL)
      return;
    bool _return = true;
    for (size_t i = 0; i < paths.size(); i++)
    {
      std::fstream file(paths[i].c_str(), std::ios::binary | std::ios::in);
      if (file.is_open())
      {
        file.seekg(0, std::ios::end);
        long size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (size == 0 || file.read(&buffer[0], size))
        {
          zip_fileinfo zfi = {0};
          std::wstring fileName = paths[i].substr(paths[i].rfind('\\') + 1);

          if (ZIP_OK == zipOpenNewFileInZip(zf, std::string(fileName.begin(), fileName.end()).c_str(),
                                          &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED,
                                          -1))
          {
            if (zipWriteInFileInZip(zf, size == 0 ? "" : &buffer[0], size))
              _return = false;

            if (zipCloseFileInZip(zf))
              _return = false;

            file.close();
            continue;
          }
        }
        file.close();
      }
      _return = false;
    }

    // common function to delete residual files
    StateAuxillary::endPlayback();
    if (zipClose(zf, NULL))
      return;

    if (!_return)
      return;
    return;
  }
}

void Metadata::setMatchMetadata(tm* matchDateTimeParam)
{
  // have consistent time across the output file and the in-json time
  matchDateTime = matchDateTimeParam;

  // set match info vars

  homeTeamPossesionFrameCount = Memory::Read_U32(addressLeftTeamBallOwnedFrames);
  awayTeamPossesionFrameCount = Memory::Read_U32(addressRightTeamBallOwnedFrames);

  const AddressSpace::Accessors* accessors =
      AddressSpace::GetAccessors(AddressSpace::Type::Effective);

  // set controllers
  controllerPort1 = Memory::Read_U16(addressControllerPort1);
  controllerPort2 = Memory::Read_U16(addressControllerPort2);
  controllerPort3 = Memory::Read_U16(addressControllerPort3);
  controllerPort4 = Memory::Read_U16(addressControllerPort4);
  controllerVector.at(0) = controllerPort1;
  controllerVector.at(1) = controllerPort2;
  controllerVector.at(2) = controllerPort3;
  controllerVector.at(3) = controllerPort4;

  // set captains, sidekicks, stage, stadium, and overtime
  leftSideCaptainID = Memory::Read_U32(addressLeftSideCaptainID);
  rightSideCaptainID = Memory::Read_U32(addressRightSideCaptainID);
  leftSideSidekickID = Memory::Read_U32(addressLeftSideSidekickID);
  rightSideSidekickID = Memory::Read_U32(addressRightSideSidekickID);
  stadiumID = Memory::Read_U32(addressStadiumID);
  overtimeNotReached = Memory::Read_U8(addressOvertimeNotReachedBool);

  // set left team stats
  leftSideScore = Memory::Read_U16(addressLeftSideScore);
  leftSideShots = Memory::Read_U32(addressLeftSideShots);
  leftSideHits = Memory::Read_U16(addressLeftSideHits);
  leftSideSteals = Memory::Read_U16(addressLeftSideSteals);
  leftSideSuperStrikes = Memory::Read_U16(addressLeftSideSuperStrikes);
  leftSidePerfectPasses = Memory::Read_U16(addressLeftSidePerfectPasses);

  // set right team stats
  rightSideScore = Memory::Read_U16(addressRightSideScore);
  rightSideShots = Memory::Read_U32(addressRightSideShots);
  rightSideHits = Memory::Read_U16(addressRightSideHits);
  rightSideSteals = Memory::Read_U16(addressRightSideSteals);
  rightSideSuperStrikes = Memory::Read_U16(addressRightSideSuperStrikes);
  rightSidePerfectPasses = Memory::Read_U16(addressRightSidePerfectPasses);

  // set ruleset
  matchDifficulty = Memory::Read_U32(addressMatchDifficulty);
  matchTimeAllotted = Memory::Read_U32(addressMatchTimeAllotted);
  matchItemsBool = Memory::Read_U8(addressMatchItemsBool);
  matchBowserBool = Memory::Read_U8(addressMatchBowserBool);
  matchSuperStrikesBool = Memory::Read_U8(addressMatchSuperStrikesBool);

  // set item metadata
  leftTeamItemOffset = Memory::Read_U32(addressLeftTeamItemOffset);
  leftTeamItemCount = Memory::Read_U32(addressLeftTeamItemCount);
  rightTeamItemOffset = Memory::Read_U32(addressRightTeamItemOffset);
  rightTeamItemCount = Memory::Read_U32(addressRightTeamItemCount);

  // set goal metadata
  leftTeamGoalOffset = Memory::Read_U32(addressLeftTeamGoalOffset);
  rightTeamGoalOffset = Memory::Read_U32(addressRightTeamGoalOffset);

  // set missed shots metadata
  leftTeamMissedShotsOffset = Memory::Read_U16(addressLeftTeamMissedShotsOffset);
  leftTeamMissedShotsFlag = Memory::Read_U16(addressLeftTeamMissedShotsFlag);
  rightTeamMissedShotsOffset = Memory::Read_U16(addressRightTeamMissedShotsOffset);
  rightTeamMissedShotsFlag = Memory::Read_U16(addressRightTeamMissedShotsFlag);

  if (!leftTeamItemVector.empty())
  {
    leftTeamItemVector.clear();
  }

  for (int i = 0; i < (int)leftTeamItemOffset; i += 8)
  {
    // we add i to each one as that is the incremented offset
    u8 leftTeamItemID = Memory::Read_U8(addressLeftTeamItemStart + i);
    u8 leftTeamItemAmount = Memory::Read_U8(addressLeftTeamItemStart + i + 1);
    float leftTeamItemTime = accessors->ReadF32(addressLeftTeamItemStart + i + 4);
    ItemStruct leftTeamItemStruct = {leftTeamItemID, leftTeamItemAmount, leftTeamItemTime};
    leftTeamItemVector.push_back(leftTeamItemStruct);
  }

  if (!rightTeamItemVector.empty())
  {
    rightTeamItemVector.clear();
  }

  for (int i = 0; i < (int)rightTeamItemOffset; i += 8)
  {
    // we add i to each one as that is the incremented offset
    u8 rightTeamItemID = Memory::Read_U8(addressRightTeamItemStart + i);
    u8 rightTeamItemAmount = Memory::Read_U8(addressRightTeamItemStart + i + 1);
    float rightTeamItemTime = accessors->ReadF32(addressRightTeamItemStart + i + 4);
    ItemStruct rightTeamItemStruct = {rightTeamItemID, rightTeamItemAmount, rightTeamItemTime};
    rightTeamItemVector.push_back(rightTeamItemStruct);
  }

  if (!leftTeamGoalVector.empty())
  {
    leftTeamGoalVector.clear();
  }

  for (int i = 0; i < (int)leftTeamGoalOffset; i += 12)
  {
    // we add i to each one as that is the incremented offset
    float leftTeamGoalTime = accessors->ReadF32(addressLeftTeamGoalStart + i);
    float leftTeamBallX = accessors->ReadF32(addressLeftTeamGoalStart + i + 4);
    float leftTeamBallY = accessors->ReadF32(addressLeftTeamGoalStart + i + 8);
    GoalStruct leftTeamGoalStruct = {leftTeamGoalTime, leftTeamBallX, leftTeamBallY};
    leftTeamGoalVector.push_back(leftTeamGoalStruct);
  }

  if (!rightTeamGoalVector.empty())
  {
    rightTeamGoalVector.clear();
  }

  for (int i = 0; i < (int)rightTeamGoalOffset; i += 12)
  {
    // we add i to each one as that is the incremented offset
    float rightTeamGoalTime = accessors->ReadF32(addressRightTeamGoalStart + i);
    float rightTeamBallX = accessors->ReadF32(addressRightTeamGoalStart + i + 4);
    float rightTeamBallY = accessors->ReadF32(addressRightTeamGoalStart + i + 8);
    GoalStruct rightTeamGoalStruct = {rightTeamGoalTime, rightTeamBallX, rightTeamBallY};
    rightTeamGoalVector.push_back(rightTeamGoalStruct);
  }

  leftTeamMissedShotsVector.clear();

  for (int i = 0; i < (int)leftTeamMissedShotsOffset; i += 12)
  {
    float leftTeamMissedShotTime = accessors->ReadF32(addressLeftTeamMissedShotsStart + i);
    float leftTeamMissedShotBallX = accessors->ReadF32(addressLeftTeamMissedShotsStart + i + 4);
    float leftTeamMissedShotBallY = accessors->ReadF32(addressLeftTeamMissedShotsStart + i + 8);
    MissedShots leftTeamMissedShotsStruct = {leftTeamMissedShotTime, leftTeamMissedShotBallX,
                                             leftTeamMissedShotBallY};
    leftTeamMissedShotsVector.push_back(leftTeamMissedShotsStruct);
  }
  if (leftTeamMissedShotsFlag == 1)
  {
    // this means we left off on a missed shot that we weren't able to record so we need to record it now
    float leftTeamMissedShotTime = accessors->ReadF32(addressLeftTeamMissedShotsTimestamp);
    float leftTeamMissedShotBallX = accessors->ReadF32(addressLeftTeamMissedShotsBallXPos);
    float leftTeamMissedShotBallY = accessors->ReadF32(addressLeftTeamMissedShotsBallYPos);
    MissedShots leftTeamMissedShotsStruct = {leftTeamMissedShotTime, leftTeamMissedShotBallX,
                                             leftTeamMissedShotBallY};
    leftTeamMissedShotsVector.push_back(leftTeamMissedShotsStruct);
  }

  rightTeamMissedShotsVector.clear();

  for (int i = 0; i < (int)rightTeamMissedShotsOffset; i += 12)
  {
    float rightTeamMissedShotTime = accessors->ReadF32(addressRightTeamMissedShotsStart + i );
    float rightTeamMissedShotBallX = accessors->ReadF32(addressRightTeamMissedShotsStart + i + 4);
    float rightTeamMissedShotBallY = accessors->ReadF32(addressRightTeamMissedShotsStart + i + 8);
    MissedShots rightTeamMissedShotsStruct = {rightTeamMissedShotTime, rightTeamMissedShotBallX,
                                             rightTeamMissedShotBallY};
    rightTeamMissedShotsVector.push_back(rightTeamMissedShotsStruct);
  }
  if (rightTeamMissedShotsFlag == 1)
  {
    // this means we left off on a missed shot that we weren't able to record so we need to record
    // it now
    float rightTeamMissedShotTime = accessors->ReadF32(addressRightTeamMissedShotsTimestamp);
    float rightTeamMissedShotBallX = accessors->ReadF32(addressRightTeamMissedShotsBallXPos);
    float rightTeamMissedShotBallY = accessors->ReadF32(addressRightTeamMissedShotsBallYPos);
    MissedShots rightTeamMissedShotsStruct = {rightTeamMissedShotTime, rightTeamMissedShotBallX,
                                             rightTeamMissedShotBallY};
    rightTeamMissedShotsVector.push_back(rightTeamMissedShotsStruct);
  }

  if (NetPlay::IsNetPlayRunning())
  {
    // even though dolhpin ports can't change during netplay, they might change what side they take
    leftTeamPlayerNamesVector.clear();
    rightTeamPlayerNamesVector.clear();
    for (int i = 0; i < controllerVector.size(); i++)
    {
      // example will be me vs me
      /*
      "Controller Port Info": {
      "Controller Port 0": 0,
      "Controller Port 1": 1,
      "Controller Port 2": 65535,
      "Controller Port 3": 65535
     },
        / we need to go find who is at port x and add their name to the array
        // but we need the json to know ports and names
        // we know ports from controller map
        // one structure. a left/right team map with port # - name
      */
      if (controllerVector.at(i) == 0)
      {
        // left team
        // in this scenario, we first match at index 0
        // so we're going to go to m_pad_map and find what player id is at port 0
        // then we're going to search for that player id in our player array and return their name
        NetPlay::PlayerId pad_map_player_id = netplayGCMap[i];
        // now get the player name that matches the PID we just stored
        std::string foundPlayerName = "";
        for (int j = 0; j < playerArray.size(); j++)
        {
          if (playerArray.at(j)->pid == pad_map_player_id)
          {
            foundPlayerName = playerArray.at(j)->name;
            break;
          }
        }
        std::string portAndPlayerName = "P" + std::to_string(i + 1) + " - " + foundPlayerName;
        leftTeamPlayerNamesVector.push_back(portAndPlayerName);
      }
      else if (controllerVector.at(i) == 1)
      {
        // right team
        NetPlay::PlayerId pad_map_player_id = netplayGCMap[i];
        // now get the player name that matches the PID we just stored
        std::string foundPlayerName = "";
        for (int j = 0; j < playerArray.size(); j++)
        {
          if (playerArray.at(j)->pid == pad_map_player_id)
          {
            foundPlayerName = playerArray.at(j)->name;
            break;
          }
        }
        std::string portAndPlayerName = "P" + std::to_string(i + 1) + " - " + foundPlayerName;
        rightTeamPlayerNamesVector.push_back(portAndPlayerName);
      }
    }
  }
  else
  {
    leftTeamPlayerNamesVector.clear();
    rightTeamPlayerNamesVector.clear();
    for (int i = 0; i < controllerVector.size(); i++)
    {
      if (controllerVector.at(i) == 0)
      {
        leftTeamPlayerNamesVector.push_back("P" + std::to_string(i + 1) + " - " + "Local Player");
      }
      else if (controllerVector.at(i) == 1)
      {
        rightTeamPlayerNamesVector.push_back("P" + std::to_string(i + 1) + " - " + "Local Player");
      }
    }
  }
  if (leftTeamPlayerNamesVector.empty())
  {
    leftTeamPlayerNamesVector.push_back("CPU");
  }
  if (rightTeamPlayerNamesVector.empty())
  {
    rightTeamPlayerNamesVector.push_back("CPU");
  }
}

void Metadata::setPlayerName(std::string playerNameParam)
{
  playerName = playerNameParam;
}

void Metadata::setPlayerArray(std::vector<const NetPlay::Player*> playerArrayParam)
{
  playerArray = playerArrayParam;
  //statViewerPlayers = playerArrayParam;
}

void Metadata::setNetPlayControllers(NetPlay::PadMappingArray m_pad_map)
{
  netplayGCMap = m_pad_map;
  //statViewerControllers = m_pad_map;
}

void Metadata::setMD5(std::array<u8, 16> md5Param)
{
  md5Hash = md5Param;
}

std::vector<const NetPlay::Player*> Metadata::getPlayerArray()
{
  return playerArray;
}

NetPlay::PadMappingArray Metadata::getControllers()
{
  return netplayGCMap;
}
