// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <semver/include/semver200.h>
#include <utility>  // std::move

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemoryUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Version.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/GeckoCode.h"
#include "Core/HW/EXI/EXI_DeviceSlippi.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Host.h"
#include "Core/NetPlayClient.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/Slippi/SlippiMatchmaking.h"

// SlippiTODO: should we remove this import for netplay build?? ifdef?
#include "Core/Slippi/SlippiPlayback.h"

#include "Core/Slippi/SlippiPremadeText.h"
#include "Core/Slippi/SlippiReplayComm.h"
#include "Core/State.h"
#include "Core/System.h"
#include "VideoCommon/OnScreenDisplay.h"

#define FRAME_INTERVAL 900
#define SLEEP_TIME_MS 8
#define WRITE_FILE_SLEEP_TIME_MS 85

// #define LOCAL_TESTING
// #define CREATE_DIFF_FILES
extern std::unique_ptr<SlippiPlaybackStatus> g_playbackStatus;
extern std::unique_ptr<SlippiReplayComm> g_replayComm;
extern bool g_needInputForFrame;

#ifdef LOCAL_TESTING
bool isLocalConnected = false;
int localChatMessageId = 0;
#endif

namespace ExpansionInterface
{
static std::unordered_map<u8, std::string> slippi_names;
static std::unordered_map<u8, std::string> slippi_connect_codes;

template <typename T>
bool isFutureReady(std::future<T>& t)
{
  return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

std::vector<u8> uint16ToVector(u16 num)
{
  u8 byte0 = num >> 8;
  u8 byte1 = num & 0xFF;

  return std::vector<u8>({byte0, byte1});
}

std::vector<u8> uint32ToVector(u32 num)
{
  u8 byte0 = num >> 24;
  u8 byte1 = (num & 0xFF0000) >> 16;
  u8 byte2 = (num & 0xFF00) >> 8;
  u8 byte3 = num & 0xFF;

  return std::vector<u8>({byte0, byte1, byte2, byte3});
}

std::vector<u8> int32ToVector(int32_t num)
{
  u8 byte0 = num >> 24;
  u8 byte1 = (num & 0xFF0000) >> 16;
  u8 byte2 = (num & 0xFF00) >> 8;
  u8 byte3 = num & 0xFF;

  return std::vector<u8>({byte0, byte1, byte2, byte3});
}

void appendWordToBuffer(std::vector<u8>* buf, u32 word)
{
  auto wordVector = uint32ToVector(word);
  buf->insert(buf->end(), wordVector.begin(), wordVector.end());
}

void appendHalfToBuffer(std::vector<u8>* buf, u16 word)
{
  auto halfVector = uint16ToVector(word);
  buf->insert(buf->end(), halfVector.begin(), halfVector.end());
}

std::string ConvertConnectCodeForGame(const std::string& input)
{
  // Shift-Jis '#' symbol is two bytes (0x8194), followed by 0x00 null terminator
  char fullWidthShiftJisHashtag[] = {-127, -108, 0};  // 0x81, 0x94, 0x00
  std::string connectCode(input);
  // SLIPPITODO：Not the best use of ReplaceAll. potential bug if more than one '#' found.
  connectCode = ReplaceAll(connectCode, "#", std::string(fullWidthShiftJisHashtag));
  // fixed length + full width (two byte) hashtag +1, null terminator +1
  connectCode.resize(CONNECT_CODE_LENGTH + 2);
  return connectCode;
}

CEXISlippi::CEXISlippi(Core::System& system) : IEXIDevice(system)
{
  INFO_LOG_FMT(SLIPPI, "EXI SLIPPI Constructor called.");

  user = std::make_unique<SlippiUser>();
  g_playbackStatus = std::make_unique<SlippiPlaybackStatus>();
  matchmaking = std::make_unique<SlippiMatchmaking>(user.get());
  gameFileLoader = std::make_unique<SlippiGameFileLoader>();
  game_reporter = std::make_unique<SlippiGameReporter>(user.get());
  g_replayComm = std::make_unique<SlippiReplayComm>();
  directCodes = std::make_unique<SlippiDirectCodes>("direct-codes.json");
  teamsCodes = std::make_unique<SlippiDirectCodes>("teams-codes.json");

  generator = std::default_random_engine(Common::Timer::NowMs());

  // Loggers will check 5 bytes, make sure we own that memory
  m_read_queue.reserve(5);

  // Initialize local selections to empty
  localSelections.Reset();

  // Forces savestate to re-init regions when a new ISO is loaded
  SlippiSavestate::shouldForceInit = true;

  // Update user file and then listen for User
#ifndef IS_PLAYBACK
  user->ListenForLogIn();
#endif

  // Use sane stage defaults (should get overwritten)
  allowedStages = {
      0x2,   // FoD
      0x3,   // Pokemon
      0x8,   // Yoshi's Story
      0x1C,  // Dream Land
      0x1F,  // Battlefield
      0x20,  // Final Destination
  };

#ifdef CREATE_DIFF_FILES
  // MnMaAll.usd
  std::string origStr;
  std::string modifiedStr;
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.usd", origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll-new.usd", modifiedStr);
  std::vector<u8> orig(origStr.begin(), origStr.end());
  std::vector<u8> modified(modifiedStr.begin(), modifiedStr.end());
  auto diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.usd.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnMaAll.usd.diff");

  // MnExtAll.usd
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.usd",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll-new.usd", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.usd.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnExtAll.usd.diff");

  // SdMenu.usd
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.usd",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu-new.usd", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.usd.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\SdMenu.usd.diff");

  // Japanese Files
  // MnMaAll.dat
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.dat", origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll-new.dat", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.dat.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnMaAll.dat.diff");

  // MnExtAll.dat
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.dat",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll-new.dat", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.dat.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnExtAll.dat.diff");

  // SdMenu.dat
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.dat",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu-new.dat", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      diff, "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.dat.diff");
  File::WriteStringToFile(diff, "C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\SdMenu.dat.diff");

  // TEMP - Restore orig
  // std::string stateString;
  // decoder.Decode((char *)orig.data(), orig.size(), diff, &stateString);
  // File::WriteStringToFile(stateString,
  //                        "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll-restored.usd");
#endif
}

CEXISlippi::~CEXISlippi()
{
  u8 empty[1];

  // Closes file gracefully to prevent file corruption when emulation
  // suddenly stops. This would happen often on netplay when the opponent
  // would close the emulation before the file successfully finished writing
  writeToFileAsync(&empty[0], 0, "close");
  writeThreadRunning = false;
  if (m_fileWriteThread.joinable())
  {
    m_fileWriteThread.join();
  }

  SlippiSpectateServer::getInstance().endGame(true);

  // Try to determine whether we were playing an in-progress ranked match, if so
  // indicate to server that this client has abandoned. Anyone trying to modify
  // this behavior to game their rating is subject to get banned.
  auto active_match_id = matchmaking->GetMatchmakeResult().id;
  if (activeMatchId.find("mode.ranked") != std::string::npos)
  {
    ERROR_LOG(SLIPPI_ONLINE, "Exit during in-progress ranked game: %s", activeMatchId.c_str());
    gameReporter->ReportAbandonment(activeMatchId);
  }
  handleConnectionCleanup();

  localSelections.Reset();

  g_playbackStatus->resetPlayback();

  // TODO: ENET shutdown should maybe be done at app shutdown instead.
  // Right now this might be problematic in the case where someone starts a netplay client
  // and then queues into online matchmaking, and then stops the game. That might deinit
  // the ENET libraries so that they can't be used anymore for the netplay lobby? Course
  // you'd have to be kinda dumb to do that sequence of stuff anyway so maybe it's nbd
  if (isEnetInitialized)
    enet_deinitialize();
}

void CEXISlippi::configureCommands(u8* payload, u8 length)
{
  for (int i = 1; i < length; i += 3)
  {
    // Go through the receive commands payload and set up other commands
    u8 commandByte = payload[i];
    u32 commandPayloadSize = payload[i + 1] << 8 | payload[i + 2];
    payloadSizes[commandByte] = commandPayloadSize;
  }
}

void CEXISlippi::updateMetadataFields(u8* payload, u32 length)
{
  if (length <= 0 || payload[0] != CMD_RECEIVE_POST_FRAME_UPDATE)
  {
    // Only need to update if this is a post frame update
    return;
  }

  // Keep track of last frame
  lastFrame = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];

  // Keep track of character usage
  u8 playerIndex = payload[5];
  u8 internalCharacterId = payload[7];
  if (!characterUsage.count(playerIndex) || !characterUsage[playerIndex].count(internalCharacterId))
  {
    characterUsage[playerIndex][internalCharacterId] = 0;
  }
  characterUsage[playerIndex][internalCharacterId] += 1;
}

// SLIPPITODO: Maybe support names from regular netplay again
std::unordered_map<u8, std::string> CEXISlippi::getNetplayNames()
{
  std::unordered_map<u8, std::string> names;

  if (slippi_names.size())
  {
    names = slippi_names;
  }

  return names;
}

std::vector<u8> CEXISlippi::generateMetadata()
{
  std::vector<u8> metadata({'U', 8, 'm', 'e', 't', 'a', 'd', 'a', 't', 'a', '{'});

  // TODO: Abstract out UBJSON functions to make this cleaner

  // Add game start time
  u8 dateTimeStrLength = sizeof "2011-10-08T07:07:09Z";
  std::vector<char> dateTimeBuf(dateTimeStrLength);
  strftime(&dateTimeBuf[0], dateTimeStrLength, "%FT%TZ", gmtime(&gameStartTime));
  dateTimeBuf.pop_back();  // Removes the \0 from the back of string
  metadata.insert(metadata.end(), {'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U',
                                   static_cast<u8>(dateTimeBuf.size())});
  metadata.insert(metadata.end(), dateTimeBuf.begin(), dateTimeBuf.end());

  // Add game duration
  std::vector<u8> lastFrameToWrite = int32ToVector(lastFrame);
  metadata.insert(metadata.end(), {'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'});
  metadata.insert(metadata.end(), lastFrameToWrite.begin(), lastFrameToWrite.end());

  // Add players elements to metadata, one per player index
  metadata.insert(metadata.end(), {'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{'});

  auto playerNames = getNetplayNames();

  for (auto it = characterUsage.begin(); it != characterUsage.end(); ++it)
  {
    auto playerIndex = it->first;
    auto playerCharacterUsage = it->second;

    metadata.push_back('U');
    std::string playerIndexStr = std::to_string(playerIndex);
    metadata.push_back(static_cast<u8>(playerIndexStr.length()));
    metadata.insert(metadata.end(), playerIndexStr.begin(), playerIndexStr.end());
    metadata.push_back('{');

    // Add names element for this player
    metadata.insert(metadata.end(), {'U', 5, 'n', 'a', 'm', 'e', 's', '{'});

    if (playerNames.count(playerIndex))
    {
      auto playerName = playerNames[playerIndex];
      // Add netplay element for this player name
      metadata.insert(metadata.end(), {'U', 7, 'n', 'e', 't', 'p', 'l', 'a', 'y', 'S', 'U'});
      metadata.push_back(static_cast<u8>(playerName.length()));
      metadata.insert(metadata.end(), playerName.begin(), playerName.end());
    }

    if (slippi_connect_codes.count(playerIndex))
    {
      auto connectCode = slippi_connect_codes[playerIndex];
      // Add connection code element for this player name
      metadata.insert(metadata.end(), {'U', 4, 'c', 'o', 'd', 'e', 'S', 'U'});
      metadata.push_back(static_cast<u8>(connectCode.length()));
      metadata.insert(metadata.end(), connectCode.begin(), connectCode.end());
    }

    metadata.push_back('}');  // close names

    // Add character element for this player
    metadata.insert(metadata.end(),
                    {'U', 10, 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', '{'});
    for (auto it2 = playerCharacterUsage.begin(); it2 != playerCharacterUsage.end(); ++it2)
    {
      metadata.push_back('U');
      std::string internalCharIdStr = std::to_string(it2->first);
      metadata.push_back(static_cast<u8>(internalCharIdStr.length()));
      metadata.insert(metadata.end(), internalCharIdStr.begin(), internalCharIdStr.end());

      metadata.push_back('l');
      std::vector<u8> frameCount = uint32ToVector(it2->second);
      metadata.insert(metadata.end(), frameCount.begin(), frameCount.end());
    }
    metadata.push_back('}');  // close characters

    metadata.push_back('}');  // close player
  }
  metadata.push_back('}');

  // Indicate this was played on dolphin
  metadata.insert(metadata.end(), {'U', 8,   'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n',
                                   'S', 'U', 7,   'd', 'o', 'l', 'p', 'h', 'i', 'n'});

  metadata.push_back('}');
  return metadata;
}

void CEXISlippi::writeToFileAsync(u8* payload, u32 length, std::string fileOption)
{
  if (!Config::Get(Config::SLIPPI_SAVE_REPLAYS))
  {
    return;
  }

  if (fileOption == "create" && !writeThreadRunning)
  {
    WARN_LOG_FMT(SLIPPI, "Creating file write thread...");
    writeThreadRunning = true;
    m_fileWriteThread = std::thread(&CEXISlippi::FileWriteThread, this);
  }

  if (!writeThreadRunning)
  {
    return;
  }

  std::vector<u8> payloadData;
  payloadData.insert(payloadData.end(), payload, payload + length);

  auto writeMsg = std::make_unique<WriteMessage>();
  writeMsg->data = payloadData;
  writeMsg->operation = fileOption;

  fileWriteQueue.push(std::move(writeMsg));
}

void CEXISlippi::FileWriteThread(void)
{
  while (writeThreadRunning || !fileWriteQueue.empty())
  {
    // Process all messages
    while (!fileWriteQueue.empty())
    {
      writeToFile(std::move(fileWriteQueue.front()));
      fileWriteQueue.pop();

      Common::SleepCurrentThread(0);
    }

    Common::SleepCurrentThread(WRITE_FILE_SLEEP_TIME_MS);
  }
}

void CEXISlippi::writeToFile(std::unique_ptr<WriteMessage> msg)
{
  if (!msg)
  {
    ERROR_LOG_FMT(SLIPPI, "Unexpected error: write message is falsy.");
    return;
  }

  u8* payload = &msg->data[0];
  u32 length = static_cast<u32>(msg->data.size());
  std::string fileOption = msg->operation;

  std::vector<u8> dataToWrite;
  if (fileOption == "create")
  {
    // If the game sends over option 1 that means a file should be created
    createNewFile();

    // Start ubjson file and prepare the "raw" element that game
    // data output will be dumped into. The size of the raw output will
    // be initialized to 0 until all of the data has been received
    std::vector<u8> headerBytes({'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0});
    dataToWrite.insert(dataToWrite.end(), headerBytes.begin(), headerBytes.end());

    // Used to keep track of how many bytes have been written to the file
    writtenByteCount = 0;

    // Used to track character usage (sheik/zelda)
    characterUsage.clear();

    // Reset lastFrame
    lastFrame = Slippi::GAME_FIRST_FRAME;

    // Get display names and connection codes from slippi netplay client
    if (slippi_netplay)
    {
      auto playerInfo = matchmaking->GetPlayerInfo();

      for (int i = 0; i < playerInfo.size(); i++)
      {
        slippi_names[i] = playerInfo[i].display_name;
        slippi_connect_codes[i] = playerInfo[i].connect_code;
      }
    }
  }

  // If no file, do nothing
  if (!m_file)
  {
    return;
  }

  // Update fields relevant to generating metadata at the end
  updateMetadataFields(payload, length);

  // Add the payload to data to write
  dataToWrite.insert(dataToWrite.end(), payload, payload + length);
  writtenByteCount += length;

  // If we are going to close the file, generate data to complete the UBJSON file
  if (fileOption == "close")
  {
    // This option indicates we are done sending over body
    std::vector<u8> closingBytes = generateMetadata();
    closingBytes.push_back('}');
    dataToWrite.insert(dataToWrite.end(), closingBytes.begin(), closingBytes.end());

    // Reset display names and connect codes retrieved from netplay client
    slippi_names.clear();
    slippi_connect_codes.clear();
  }

  // Write data to file
  bool result = m_file.WriteBytes(&dataToWrite[0], dataToWrite.size());
  if (!result)
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to write data to file.");
  }

  // If file should be closed, close it
  if (fileOption == "close")
  {
    // Write the number of bytes for the raw output
    std::vector<u8> sizeBytes = uint32ToVector(writtenByteCount);
    m_file.Seek(11, File::SeekOrigin::Begin);
    m_file.WriteBytes(&sizeBytes[0], sizeBytes.size());

    // Close file
    closeFile();
  }
}

void CEXISlippi::createNewFile()
{
  if (m_file)
  {
    // If there's already a file open, close that one
    closeFile();
  }

  std::string dirpath = Config::Get(Config::SLIPPI_REPLAY_DIR);
  // in case the config value just gets lost somehow
  if (dirpath.empty())
  {
    Config::Get(Config::SLIPPI_REPLAY_DIR) = File::GetHomeDirectory() + DIR_SEP + "Slippi";
    dirpath = Config::Get(Config::SLIPPI_REPLAY_DIR);
  }

  // Remove a trailing / or \\ if the user managed to have that in their config
  char dirpathEnd = dirpath.back();
  if (dirpathEnd == '/' || dirpathEnd == '\\')
  {
    dirpath.pop_back();
  }

  // First, ensure that the root Slippi replay directory is created
  File::CreateFullPath(dirpath + DIR_SEP);

  // Now we have a dir such as /home/Replays but we need to make one such
  // as /home/Replays/2020-06 if month categorization is enabled
  if (Config::Get(Config::SLIPPI_REPLAY_MONTH_FOLDERS))
  {
    dirpath.push_back('/');

    // Append YYYY-MM to the directory path
    uint8_t yearMonthStrLength = sizeof "2020-06-Mainline";
    std::vector<char> yearMonthBuf(yearMonthStrLength);
    strftime(&yearMonthBuf[0], yearMonthStrLength, "%Y-%m-Mainline", localtime(&gameStartTime));

    std::string yearMonth(&yearMonthBuf[0]);
    dirpath.append(yearMonth);

    // Ensure that the subfolder directory is created
    File::CreateDir(dirpath);
  }

  std::string filepath = dirpath + DIR_SEP + generateFileName();
  INFO_LOG_FMT(SLIPPI, "EXI_DeviceSlippi.cpp: Creating new replay file {}", filepath.c_str());

#ifdef _WIN32
  m_file = File::IOFile(filepath, "wb", File::SharedAccess::Read);
#else
  m_file = File::IOFile(filepath, "wb");
#endif

  if (!m_file)
  {
    PanicAlertFmtT("Could not create .slp replay file [{0}].\n\n"
                   "The replay folder's path might be invalid, or you might "
                   "not have permission to write to it.\n\n"
                   "You can change the replay folder in Config > Slippi > "
                   "Slippi Replay Settings.",
                   filepath);
  }
}

std::string CEXISlippi::generateFileName()
{
  // Add game start time
  u8 dateTimeStrLength = sizeof "20171015T095717";
  std::vector<char> dateTimeBuf(dateTimeStrLength);
  strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&gameStartTime));

  std::string str(&dateTimeBuf[0]);
  return StringFromFormat("Game_%s.slp", str.c_str());
}

void CEXISlippi::closeFile()
{
  if (!m_file)
  {
    // If we have no file or payload is not game end, do nothing
    return;
  }

  // If this is the end of the game end payload, reset the file so that we create a new one
  m_file.Close();
  m_file = nullptr;
}

void CEXISlippi::prepareGameInfo(u8* payload)
{
  // Since we are prepping new data, clear any existing data
  m_read_queue.clear();

  if (!m_current_game)
  {
    // Do nothing if we don't have a game loaded
    return;
  }

  if (!m_current_game->AreSettingsLoaded())
  {
    m_read_queue.push_back(0);
    return;
  }

  // Return success code
  m_read_queue.push_back(1);

  // Prepare playback savestate payload
  playbackSavestatePayload.clear();
  appendWordToBuffer(&playbackSavestatePayload, 0);  // This space will be used to set frame index
  int bkpPos = 0;
  while ((*(u32*)(&payload[bkpPos * 8])) != 0)
  {
    bkpPos += 1;
  }
  playbackSavestatePayload.insert(playbackSavestatePayload.end(), payload,
                                  payload + (bkpPos * 8 + 4));

  Slippi::GameSettings* settings = m_current_game->GetSettings();

  // Unlikely but reset the overclocking in case we quit during a hard ffw in a previous play
  SConfig::GetSlippiConfig().oc_enable = g_playbackStatus->origOCEnable;
  SConfig::GetSlippiConfig().oc_factor = g_playbackStatus->origOCFactor;

  // Start in Fast Forward if this is mirrored
  auto replayCommSettings = g_replayComm->getSettings();
  if (!g_playbackStatus->isHardFFW)
    g_playbackStatus->isHardFFW = replayCommSettings.mode == "mirror";
  g_playbackStatus->lastFFWFrame = INT_MIN;

  // Build a word containing the stage and the presence of the characters
  u32 randomSeed = settings->randomSeed;
  appendWordToBuffer(&m_read_queue, randomSeed);

  // This is kinda dumb but we need to handle the case where a player transforms
  // into sheik/zelda immediately. This info is not stored in the game info header
  // and so let's overwrite those values
  int player1Pos = 24;  // This is the index of the first players character info
  std::array<u32, Slippi::GAME_INFO_HEADER_SIZE> gameInfoHeader = settings->header;
  for (int i = 0; i < 4; i++)
  {
    // check if this player is actually in the game
    bool playerExists = m_current_game->DoesPlayerExist(i);
    if (!playerExists)
    {
      continue;
    }

    // check if the player is playing sheik or zelda
    u8 externalCharId = settings->players[i].characterId;
    if (externalCharId != 0x12 && externalCharId != 0x13)
    {
      continue;
    }

    // this is the position in the array that this player's character info is stored
    int pos = player1Pos + (9 * i);

    // here we have determined the player is playing sheik or zelda...
    // at this point let's overwrite the player's character with the one
    // that they are playing
    gameInfoHeader[pos] &= 0x00FFFFFF;
    gameInfoHeader[pos] |= externalCharId << 24;
  }

  // Write entire header to game
  for (int i = 0; i < Slippi::GAME_INFO_HEADER_SIZE; i++)
  {
    appendWordToBuffer(&m_read_queue, gameInfoHeader[i]);
  }

  // Write UCF toggles
  std::array<u32, Slippi::UCF_TOGGLE_SIZE> ucfToggles = settings->ucfToggles;
  for (int i = 0; i < Slippi::UCF_TOGGLE_SIZE; i++)
  {
    appendWordToBuffer(&m_read_queue, ucfToggles[i]);
  }

  // Write nametags
  for (int i = 0; i < 4; i++)
  {
    auto player = settings->players[i];
    for (int j = 0; j < Slippi::NAMETAG_SIZE; j++)
    {
      appendHalfToBuffer(&m_read_queue, player.nametag[j]);
    }
  }

  // Write PAL byte
  m_read_queue.push_back(settings->isPAL);

  // Get replay version numbers
  auto replayVersion = m_current_game->GetVersion();
  auto majorVersion = replayVersion[0];
  auto minorVersion = replayVersion[1];

  // Write PS pre-load byte
  auto shouldPreloadPs = majorVersion > 1 || (majorVersion == 1 && minorVersion > 2);
  m_read_queue.push_back(shouldPreloadPs);

  // Write PS Frozen byte
  m_read_queue.push_back(settings->isFrozenPS);

  // Write should resync setting
  m_read_queue.push_back(replayCommSettings.shouldResync ? 1 : 0);

  // Write display names
  for (int i = 0; i < 4; i++)
  {
    auto displayName = settings->players[i].displayName;
    m_read_queue.insert(m_read_queue.end(), displayName.begin(), displayName.end());
  }

  // Return the size of the gecko code list
  prepareGeckoList();
  appendWordToBuffer(&m_read_queue, static_cast<u32>(geckoList.size()));

  // Initialize frame sequence index value for reading rollbacks
  frameSeqIdx = 0;

  if (replayCommSettings.rollbackDisplayMethod != "off")
  {
    // Prepare savestates
    availableSavestates.clear();
    activeSavestates.clear();

    // Prepare savestates for online play
    for (int i = 0; i < ROLLBACK_MAX_FRAMES; i++)
    {
      availableSavestates.push_back(std::make_unique<SlippiSavestate>());
    }
  }
  else
  {
    // Prepare savestates
    availableSavestates.clear();
    activeSavestates.clear();

    // Add savestate for testing
    availableSavestates.push_back(std::make_unique<SlippiSavestate>());
  }

  // Reset playback frame to begining
  g_playbackStatus->currentPlaybackFrame = Slippi::GAME_FIRST_FRAME;

  // Initialize replay related threads if not viewing rollback versions of relays
  if (replayCommSettings.rollbackDisplayMethod == "off" &&
      (replayCommSettings.mode == "normal" || replayCommSettings.mode == "queue"))
  {
    // g_playbackStatus->startThreads();
  }
}

void CEXISlippi::prepareGeckoList()
{
  // TODO: How do I move this somewhere else?
  // This contains all of the codes required to play legacy replays (UCF, PAL, Frz Stadium)
  static std::vector<u8> defaultCodeList = {
      0xC2, 0x0C, 0x9A, 0x44, 0x00, 0x00, 0x00, 0x2F,  // #External/UCF + Arduino Toggle UI/UCF/UCF
                                                       // 0.74 Dashback - Check for Toggle.asm
      0xD0, 0x1F, 0x00, 0x2C, 0x88, 0x9F, 0x06, 0x18, 0x38, 0x62, 0xF2, 0x28, 0x7C, 0x63, 0x20,
      0xAE, 0x2C, 0x03, 0x00, 0x01, 0x41, 0x82, 0x00, 0x14, 0x38, 0x62, 0xF2, 0x2C, 0x7C, 0x63,
      0x20, 0xAE, 0x2C, 0x03, 0x00, 0x01, 0x40, 0x82, 0x01, 0x50, 0x7C, 0x08, 0x02, 0xA6, 0x90,
      0x01, 0x00, 0x04, 0x94, 0x21, 0xFF, 0x50, 0xBE, 0x81, 0x00, 0x08, 0x48, 0x00, 0x01, 0x21,
      0x7F, 0xC8, 0x02, 0xA6, 0xC0, 0x3F, 0x08, 0x94, 0xC0, 0x5E, 0x00, 0x00, 0xFC, 0x01, 0x10,
      0x40, 0x40, 0x82, 0x01, 0x18, 0x80, 0x8D, 0xAE, 0xB4, 0xC0, 0x3F, 0x06, 0x20, 0xFC, 0x20,
      0x0A, 0x10, 0xC0, 0x44, 0x00, 0x3C, 0xFC, 0x01, 0x10, 0x40, 0x41, 0x80, 0x01, 0x00, 0x88,
      0x7F, 0x06, 0x70, 0x2C, 0x03, 0x00, 0x02, 0x40, 0x80, 0x00, 0xF4, 0x88, 0x7F, 0x22, 0x1F,
      0x54, 0x60, 0x07, 0x39, 0x40, 0x82, 0x00, 0xE8, 0x3C, 0x60, 0x80, 0x4C, 0x60, 0x63, 0x1F,
      0x78, 0x8B, 0xA3, 0x00, 0x01, 0x38, 0x7D, 0xFF, 0xFE, 0x88, 0x9F, 0x06, 0x18, 0x48, 0x00,
      0x00, 0x8D, 0x7C, 0x7C, 0x1B, 0x78, 0x7F, 0xA3, 0xEB, 0x78, 0x88, 0x9F, 0x06, 0x18, 0x48,
      0x00, 0x00, 0x7D, 0x7C, 0x7C, 0x18, 0x50, 0x7C, 0x63, 0x19, 0xD6, 0x2C, 0x03, 0x15, 0xF9,
      0x40, 0x81, 0x00, 0xB0, 0x38, 0x00, 0x00, 0x01, 0x90, 0x1F, 0x23, 0x58, 0x90, 0x1F, 0x23,
      0x40, 0x80, 0x9F, 0x00, 0x04, 0x2C, 0x04, 0x00, 0x0A, 0x40, 0xA2, 0x00, 0x98, 0x88, 0x7F,
      0x00, 0x0C, 0x38, 0x80, 0x00, 0x01, 0x3D, 0x80, 0x80, 0x03, 0x61, 0x8C, 0x41, 0x8C, 0x7D,
      0x89, 0x03, 0xA6, 0x4E, 0x80, 0x04, 0x21, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x78,
      0x80, 0x83, 0x00, 0x2C, 0x80, 0x84, 0x1E, 0xCC, 0xC0, 0x3F, 0x00, 0x2C, 0xD0, 0x24, 0x00,
      0x18, 0xC0, 0x5E, 0x00, 0x04, 0xFC, 0x01, 0x10, 0x40, 0x41, 0x81, 0x00, 0x0C, 0x38, 0x60,
      0x00, 0x80, 0x48, 0x00, 0x00, 0x08, 0x38, 0x60, 0x00, 0x7F, 0x98, 0x64, 0x00, 0x06, 0x48,
      0x00, 0x00, 0x48, 0x7C, 0x85, 0x23, 0x78, 0x38, 0x63, 0xFF, 0xFF, 0x2C, 0x03, 0x00, 0x00,
      0x40, 0x80, 0x00, 0x08, 0x38, 0x63, 0x00, 0x05, 0x3C, 0x80, 0x80, 0x46, 0x60, 0x84, 0xB1,
      0x08, 0x1C, 0x63, 0x00, 0x30, 0x7C, 0x84, 0x1A, 0x14, 0x1C, 0x65, 0x00, 0x0C, 0x7C, 0x84,
      0x1A, 0x14, 0x88, 0x64, 0x00, 0x02, 0x7C, 0x63, 0x07, 0x74, 0x4E, 0x80, 0x00, 0x20, 0x4E,
      0x80, 0x00, 0x21, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBA, 0x81, 0x00, 0x08,
      0x80, 0x01, 0x00, 0xB4, 0x38, 0x21, 0x00, 0xB0, 0x7C, 0x08, 0x03, 0xA6, 0x00, 0x00, 0x00,
      0x00, 0xC2, 0x09, 0x98, 0xA4, 0x00, 0x00, 0x00,
      0x2B,  // #External/UCF + Arduino Toggle UI/UCF/UCF
             // 0.74 Shield Drop - Check for Toggle.asm
      0x7C, 0x08, 0x02, 0xA6, 0x90, 0x01, 0x00, 0x04, 0x94, 0x21, 0xFF, 0x50, 0xBE, 0x81, 0x00,
      0x08, 0x7C, 0x7E, 0x1B, 0x78, 0x83, 0xFE, 0x00, 0x2C, 0x48, 0x00, 0x01, 0x01, 0x7F, 0xA8,
      0x02, 0xA6, 0x88, 0x9F, 0x06, 0x18, 0x38, 0x62, 0xF2, 0x28, 0x7C, 0x63, 0x20, 0xAE, 0x2C,
      0x03, 0x00, 0x01, 0x41, 0x82, 0x00, 0x14, 0x38, 0x62, 0xF2, 0x30, 0x7C, 0x63, 0x20, 0xAE,
      0x2C, 0x03, 0x00, 0x01, 0x40, 0x82, 0x00, 0xF8, 0xC0, 0x3F, 0x06, 0x3C, 0x80, 0x6D, 0xAE,
      0xB4, 0xC0, 0x03, 0x03, 0x14, 0xFC, 0x01, 0x00, 0x40, 0x40, 0x81, 0x00, 0xE4, 0xC0, 0x3F,
      0x06, 0x20, 0x48, 0x00, 0x00, 0x71, 0xD0, 0x21, 0x00, 0x90, 0xC0, 0x3F, 0x06, 0x24, 0x48,
      0x00, 0x00, 0x65, 0xC0, 0x41, 0x00, 0x90, 0xEC, 0x42, 0x00, 0xB2, 0xEC, 0x21, 0x00, 0x72,
      0xEC, 0x21, 0x10, 0x2A, 0xC0, 0x5D, 0x00, 0x0C, 0xFC, 0x01, 0x10, 0x40, 0x41, 0x80, 0x00,
      0xB4, 0x88, 0x9F, 0x06, 0x70, 0x2C, 0x04, 0x00, 0x03, 0x40, 0x81, 0x00, 0xA8, 0xC0, 0x1D,
      0x00, 0x10, 0xC0, 0x3F, 0x06, 0x24, 0xFC, 0x00, 0x08, 0x40, 0x40, 0x80, 0x00, 0x98, 0xBA,
      0x81, 0x00, 0x08, 0x80, 0x01, 0x00, 0xB4, 0x38, 0x21, 0x00, 0xB0, 0x7C, 0x08, 0x03, 0xA6,
      0x80, 0x61, 0x00, 0x1C, 0x83, 0xE1, 0x00, 0x14, 0x38, 0x21, 0x00, 0x18, 0x38, 0x63, 0x00,
      0x08, 0x7C, 0x68, 0x03, 0xA6, 0x4E, 0x80, 0x00, 0x20, 0xFC, 0x00, 0x0A, 0x10, 0xC0, 0x3D,
      0x00, 0x00, 0xEC, 0x00, 0x00, 0x72, 0xC0, 0x3D, 0x00, 0x04, 0xEC, 0x00, 0x08, 0x28, 0xFC,
      0x00, 0x00, 0x1E, 0xD8, 0x01, 0x00, 0x80, 0x80, 0x61, 0x00, 0x84, 0x38, 0x63, 0x00, 0x02,
      0x3C, 0x00, 0x43, 0x30, 0xC8, 0x5D, 0x00, 0x14, 0x6C, 0x63, 0x80, 0x00, 0x90, 0x01, 0x00,
      0x80, 0x90, 0x61, 0x00, 0x84, 0xC8, 0x21, 0x00, 0x80, 0xEC, 0x01, 0x10, 0x28, 0xC0, 0x3D,
      0x00, 0x00, 0xEC, 0x20, 0x08, 0x24, 0x4E, 0x80, 0x00, 0x20, 0x4E, 0x80, 0x00, 0x21, 0x42,
      0xA0, 0x00, 0x00, 0x37, 0x27, 0x00, 0x00, 0x43, 0x30, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00,
      0xBF, 0x4C, 0xCC, 0xCD, 0x43, 0x30, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x7F, 0xC3, 0xF3,
      0x78, 0x7F, 0xE4, 0xFB, 0x78, 0xBA, 0x81, 0x00, 0x08, 0x80, 0x01, 0x00, 0xB4, 0x38, 0x21,
      0x00, 0xB0, 0x7C, 0x08, 0x03, 0xA6, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC2,
      0x16, 0xE7, 0x50, 0x00, 0x00, 0x00,
      0x33,  // #Common/StaticPatches/ToggledStaticOverwrites.asm
      0x88, 0x62, 0xF2, 0x34, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x14, 0x48, 0x00, 0x00,
      0x75, 0x7C, 0x68, 0x02, 0xA6, 0x48, 0x00, 0x01, 0x3D, 0x48, 0x00, 0x00, 0x14, 0x48, 0x00,
      0x00, 0x95, 0x7C, 0x68, 0x02, 0xA6, 0x48, 0x00, 0x01, 0x2D, 0x48, 0x00, 0x00, 0x04, 0x88,
      0x62, 0xF2, 0x38, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x14, 0x48, 0x00, 0x00, 0xB9,
      0x7C, 0x68, 0x02, 0xA6, 0x48, 0x00, 0x01, 0x11, 0x48, 0x00, 0x00, 0x10, 0x48, 0x00, 0x00,
      0xC9, 0x7C, 0x68, 0x02, 0xA6, 0x48, 0x00, 0x01, 0x01, 0x88, 0x62, 0xF2, 0x3C, 0x2C, 0x03,
      0x00, 0x00, 0x41, 0x82, 0x00, 0x14, 0x48, 0x00, 0x00, 0xD1, 0x7C, 0x68, 0x02, 0xA6, 0x48,
      0x00, 0x00, 0xE9, 0x48, 0x00, 0x01, 0x04, 0x48, 0x00, 0x00, 0xD1, 0x7C, 0x68, 0x02, 0xA6,
      0x48, 0x00, 0x00, 0xD9, 0x48, 0x00, 0x00, 0xF4, 0x4E, 0x80, 0x00, 0x21, 0x80, 0x3C, 0xE4,
      0xD4, 0x00, 0x24, 0x04, 0x64, 0x80, 0x07, 0x96, 0xE0, 0x60, 0x00, 0x00, 0x00, 0x80, 0x2B,
      0x7E, 0x54, 0x48, 0x00, 0x00, 0x88, 0x80, 0x2B, 0x80, 0x8C, 0x48, 0x00, 0x00, 0x84, 0x80,
      0x12, 0x39, 0xA8, 0x60, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x4E, 0x80, 0x00, 0x21,
      0x80, 0x3C, 0xE4, 0xD4, 0x00, 0x20, 0x00, 0x00, 0x80, 0x07, 0x96, 0xE0, 0x3A, 0x40, 0x00,
      0x01, 0x80, 0x2B, 0x7E, 0x54, 0x88, 0x7F, 0x22, 0x40, 0x80, 0x2B, 0x80, 0x8C, 0x2C, 0x03,
      0x00, 0x02, 0x80, 0x10, 0xFC, 0x48, 0x90, 0x05, 0x21, 0xDC, 0x80, 0x10, 0xFB, 0x68, 0x90,
      0x05, 0x21, 0xDC, 0x80, 0x12, 0x39, 0xA8, 0x90, 0x1F, 0x1A, 0x5C, 0xFF, 0xFF, 0xFF, 0xFF,
      0x4E, 0x80, 0x00, 0x21, 0x80, 0x1D, 0x46, 0x10, 0x48, 0x00, 0x00, 0x4C, 0x80, 0x1D, 0x47,
      0x24, 0x48, 0x00, 0x00, 0x3C, 0x80, 0x1D, 0x46, 0x0C, 0x80, 0x9F, 0x00, 0xEC, 0xFF, 0xFF,
      0xFF, 0xFF, 0x4E, 0x80, 0x00, 0x21, 0x80, 0x1D, 0x46, 0x10, 0x38, 0x83, 0x7F, 0x9C, 0x80,
      0x1D, 0x47, 0x24, 0x88, 0x1B, 0x00, 0xC4, 0x80, 0x1D, 0x46, 0x0C, 0x3C, 0x60, 0x80, 0x3B,
      0xFF, 0xFF, 0xFF, 0xFF, 0x4E, 0x80, 0x00, 0x21, 0x80, 0x1D, 0x45, 0xFC, 0x48, 0x00, 0x09,
      0xDC, 0xFF, 0xFF, 0xFF, 0xFF, 0x4E, 0x80, 0x00, 0x21, 0x80, 0x1D, 0x45, 0xFC, 0x40, 0x80,
      0x09, 0xDC, 0xFF, 0xFF, 0xFF, 0xFF, 0x38, 0xA3, 0xFF, 0xFC, 0x84, 0x65, 0x00, 0x04, 0x2C,
      0x03, 0xFF, 0xFF, 0x41, 0x82, 0x00, 0x10, 0x84, 0x85, 0x00, 0x04, 0x90, 0x83, 0x00, 0x00,
      0x4B, 0xFF, 0xFF, 0xEC, 0x4E, 0x80, 0x00, 0x20, 0x3C, 0x60, 0x80, 0x00, 0x3C, 0x80, 0x00,
      0x3B, 0x60, 0x84, 0x72, 0x2C, 0x3D, 0x80, 0x80, 0x32, 0x61, 0x8C, 0x8F, 0x50, 0x7D, 0x89,
      0x03, 0xA6, 0x4E, 0x80, 0x04, 0x21, 0x3C, 0x60, 0x80, 0x17, 0x3C, 0x80, 0x80, 0x17, 0x00,
      0x00, 0x00, 0x00, 0xC2, 0x1D, 0x14, 0xC8, 0x00, 0x00, 0x00,
      0x04,  // #Common/Preload Stadium
             // Transformations/Handlers/Init
             // isLoaded Bool.asm
      0x88, 0x62, 0xF2, 0x38, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x0C, 0x38, 0x60, 0x00,
      0x00, 0x98, 0x7F, 0x00, 0xF0, 0x3B, 0xA0, 0x00, 0x01, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xC2, 0x1D, 0x45, 0xEC, 0x00, 0x00, 0x00, 0x1B,  // #Common/Preload Stadium
                                                                   // Transformations/Handlers/Load
                                                                   // Transformation.asm
      0x88, 0x62, 0xF2, 0x38, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0xC4, 0x88, 0x7F, 0x00,
      0xF0, 0x2C, 0x03, 0x00, 0x00, 0x40, 0x82, 0x00, 0xB8, 0x38, 0x60, 0x00, 0x04, 0x3D, 0x80,
      0x80, 0x38, 0x61, 0x8C, 0x05, 0x80, 0x7D, 0x89, 0x03, 0xA6, 0x4E, 0x80, 0x04, 0x21, 0x54,
      0x60, 0x10, 0x3A, 0xA8, 0x7F, 0x00, 0xE2, 0x3C, 0x80, 0x80, 0x3B, 0x60, 0x84, 0x7F, 0x9C,
      0x7C, 0x84, 0x00, 0x2E, 0x7C, 0x03, 0x20, 0x00, 0x41, 0x82, 0xFF, 0xD4, 0x90, 0x9F, 0x00,
      0xEC, 0x2C, 0x04, 0x00, 0x03, 0x40, 0x82, 0x00, 0x0C, 0x38, 0x80, 0x00, 0x00, 0x48, 0x00,
      0x00, 0x34, 0x2C, 0x04, 0x00, 0x04, 0x40, 0x82, 0x00, 0x0C, 0x38, 0x80, 0x00, 0x01, 0x48,
      0x00, 0x00, 0x24, 0x2C, 0x04, 0x00, 0x09, 0x40, 0x82, 0x00, 0x0C, 0x38, 0x80, 0x00, 0x02,
      0x48, 0x00, 0x00, 0x14, 0x2C, 0x04, 0x00, 0x06, 0x40, 0x82, 0x00, 0x00, 0x38, 0x80, 0x00,
      0x03, 0x48, 0x00, 0x00, 0x04, 0x3C, 0x60, 0x80, 0x3E, 0x60, 0x63, 0x12, 0x48, 0x54, 0x80,
      0x10, 0x3A, 0x7C, 0x63, 0x02, 0x14, 0x80, 0x63, 0x03, 0xD8, 0x80, 0x9F, 0x00, 0xCC, 0x38,
      0xBF, 0x00, 0xC8, 0x3C, 0xC0, 0x80, 0x1D, 0x60, 0xC6, 0x42, 0x20, 0x38, 0xE0, 0x00, 0x00,
      0x3D, 0x80, 0x80, 0x01, 0x61, 0x8C, 0x65, 0x80, 0x7D, 0x89, 0x03, 0xA6, 0x4E, 0x80, 0x04,
      0x21, 0x38, 0x60, 0x00, 0x01, 0x98, 0x7F, 0x00, 0xF0, 0x80, 0x7F, 0x00, 0xD8, 0x60, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC2, 0x1D, 0x4F, 0x14, 0x00, 0x00, 0x00,
      0x04,  // #Common/Preload
             // Stadium
             // Transformations/Handlers/Reset
             // isLoaded.asm
      0x88, 0x62, 0xF2, 0x38, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x0C, 0x38, 0x60, 0x00,
      0x00, 0x98, 0x7F, 0x00, 0xF0, 0x80, 0x6D, 0xB2, 0xD8, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xC2, 0x06, 0x8F, 0x30, 0x00, 0x00, 0x00, 0x9D,  // #Common/PAL/Handlers/Character
                                                                   // DAT Patcher.asm
      0x88, 0x62, 0xF2, 0x34, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x04, 0xD4, 0x7C, 0x08, 0x02,
      0xA6, 0x90, 0x01, 0x00, 0x04, 0x94, 0x21, 0xFF, 0x50, 0xBE, 0x81, 0x00, 0x08, 0x83, 0xFE,
      0x01, 0x0C, 0x83, 0xFF, 0x00, 0x08, 0x3B, 0xFF, 0xFF, 0xE0, 0x80, 0x7D, 0x00, 0x00, 0x2C,
      0x03, 0x00, 0x1B, 0x40, 0x80, 0x04, 0x9C, 0x48, 0x00, 0x00, 0x71, 0x48, 0x00, 0x00, 0xA9,
      0x48, 0x00, 0x00, 0xB9, 0x48, 0x00, 0x01, 0x51, 0x48, 0x00, 0x01, 0x79, 0x48, 0x00, 0x01,
      0x79, 0x48, 0x00, 0x02, 0x29, 0x48, 0x00, 0x02, 0x39, 0x48, 0x00, 0x02, 0x81, 0x48, 0x00,
      0x02, 0xF9, 0x48, 0x00, 0x03, 0x11, 0x48, 0x00, 0x03, 0x11, 0x48, 0x00, 0x03, 0x11, 0x48,
      0x00, 0x03, 0x11, 0x48, 0x00, 0x03, 0x21, 0x48, 0x00, 0x03, 0x21, 0x48, 0x00, 0x03, 0x89,
      0x48, 0x00, 0x03, 0x89, 0x48, 0x00, 0x03, 0x91, 0x48, 0x00, 0x03, 0x91, 0x48, 0x00, 0x03,
      0xA9, 0x48, 0x00, 0x03, 0xA9, 0x48, 0x00, 0x03, 0xB9, 0x48, 0x00, 0x03, 0xB9, 0x48, 0x00,
      0x03, 0xC9, 0x48, 0x00, 0x03, 0xC9, 0x48, 0x00, 0x03, 0xC9, 0x48, 0x00, 0x04, 0x29, 0x7C,
      0x88, 0x02, 0xA6, 0x1C, 0x63, 0x00, 0x04, 0x7C, 0x84, 0x1A, 0x14, 0x80, 0xA4, 0x00, 0x00,
      0x54, 0xA5, 0x01, 0xBA, 0x7C, 0xA4, 0x2A, 0x14, 0x80, 0x65, 0x00, 0x00, 0x80, 0x85, 0x00,
      0x04, 0x2C, 0x03, 0x00, 0xFF, 0x41, 0x82, 0x00, 0x14, 0x7C, 0x63, 0xFA, 0x14, 0x90, 0x83,
      0x00, 0x00, 0x38, 0xA5, 0x00, 0x08, 0x4B, 0xFF, 0xFF, 0xE4, 0x48, 0x00, 0x03, 0xF0, 0x00,
      0x00, 0x33, 0x44, 0x3F, 0x54, 0x7A, 0xE1, 0x00, 0x00, 0x33, 0x60, 0x42, 0xC4, 0x00, 0x00,
      0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x37, 0x9C, 0x42, 0x92, 0x00, 0x00, 0x00, 0x00, 0x39,
      0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x39, 0x0C, 0x40, 0x86, 0x66, 0x66, 0x00, 0x00,
      0x39, 0x10, 0x3D, 0xEA, 0x0E, 0xA1, 0x00, 0x00, 0x39, 0x28, 0x41, 0xA0, 0x00, 0x00, 0x00,
      0x00, 0x3C, 0x04, 0x2C, 0x01, 0x48, 0x0C, 0x00, 0x00, 0x47, 0x20, 0x1B, 0x96, 0x80, 0x13,
      0x00, 0x00, 0x47, 0x34, 0x1B, 0x96, 0x80, 0x13, 0x00, 0x00, 0x47, 0x3C, 0x04, 0x00, 0x00,
      0x09, 0x00, 0x00, 0x4A, 0x40, 0x2C, 0x00, 0x68, 0x11, 0x00, 0x00, 0x4A, 0x4C, 0x28, 0x1B,
      0x00, 0x13, 0x00, 0x00, 0x4A, 0x50, 0x0D, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x4A, 0x54, 0x2C,
      0x80, 0x68, 0x11, 0x00, 0x00, 0x4A, 0x60, 0x28, 0x1B, 0x00, 0x13, 0x00, 0x00, 0x4A, 0x64,
      0x0D, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x4B, 0x24, 0x2C, 0x00, 0x68, 0x0D, 0x00, 0x00, 0x4B,
      0x30, 0x0F, 0x10, 0x40, 0x13, 0x00, 0x00, 0x4B, 0x38, 0x2C, 0x80, 0x38, 0x0D, 0x00, 0x00,
      0x4B, 0x44, 0x0F, 0x10, 0x40, 0x13, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x38, 0x0C, 0x00,
      0x00, 0x00, 0x07, 0x00, 0x00, 0x4E, 0xF8, 0x2C, 0x00, 0x38, 0x03, 0x00, 0x00, 0x4F, 0x08,
      0x0F, 0x80, 0x00, 0x0B, 0x00, 0x00, 0x4F, 0x0C, 0x2C, 0x80, 0x20, 0x03, 0x00, 0x00, 0x4F,
      0x1C, 0x0F, 0x80, 0x00, 0x0B, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
      0x4D, 0x10, 0x3F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x70, 0x42, 0x94, 0x00, 0x00, 0x00,
      0x00, 0x4D, 0xD4, 0x41, 0x90, 0x00, 0x00, 0x00, 0x00, 0x4D, 0xE0, 0x41, 0x90, 0x00, 0x00,
      0x00, 0x00, 0x83, 0xAC, 0x2C, 0x00, 0x00, 0x09, 0x00, 0x00, 0x83, 0xB8, 0x34, 0x8C, 0x80,
      0x11, 0x00, 0x00, 0x84, 0x00, 0x34, 0x8C, 0x80, 0x11, 0x00, 0x00, 0x84, 0x30, 0x05, 0x00,
      0x00, 0x8B, 0x00, 0x00, 0x84, 0x38, 0x04, 0x1A, 0x05, 0x00, 0x00, 0x00, 0x84, 0x44, 0x05,
      0x00, 0x00, 0x8B, 0x00, 0x00, 0x84, 0xDC, 0x05, 0x78, 0x05, 0x78, 0x00, 0x00, 0x85, 0xB8,
      0x10, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x85, 0xC0, 0x03, 0xE8, 0x01, 0xF4, 0x00, 0x00, 0x85,
      0xCC, 0x10, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x85, 0xD4, 0x03, 0x84, 0x03, 0xE8, 0x00, 0x00,
      0x85, 0xE0, 0x10, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x88, 0x18, 0x0B, 0x00, 0x01, 0x0B, 0x00,
      0x00, 0x88, 0x2C, 0x0B, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x88, 0xF8, 0x04, 0x1A, 0x0B, 0xB8,
      0x00, 0x00, 0x89, 0x3C, 0x04, 0x1A, 0x0B, 0xB8, 0x00, 0x00, 0x89, 0x80, 0x04, 0x1A, 0x0B,
      0xB8, 0x00, 0x00, 0x89, 0xE0, 0x04, 0xFE, 0xF7, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
      0x36, 0xCC, 0x42, 0xEC, 0x00, 0x00, 0x00, 0x00, 0x37, 0xC4, 0x0C, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0x00, 0x34, 0x68, 0x3F, 0x66, 0x66, 0x66, 0x00, 0x00, 0x39, 0xD8,
      0x44, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x3A, 0x44, 0xB4, 0x99, 0x00, 0x11, 0x00, 0x00, 0x3A,
      0x48, 0x1B, 0x8C, 0x00, 0x8F, 0x00, 0x00, 0x3A, 0x58, 0xB4, 0x99, 0x00, 0x11, 0x00, 0x00,
      0x3A, 0x5C, 0x1B, 0x8C, 0x00, 0x8F, 0x00, 0x00, 0x3A, 0x6C, 0xB4, 0x99, 0x00, 0x11, 0x00,
      0x00, 0x3A, 0x70, 0x1B, 0x8C, 0x00, 0x8F, 0x00, 0x00, 0x3B, 0x30, 0x44, 0x0C, 0x00, 0x00,
      0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x45, 0xC8, 0x2C, 0x01, 0x50, 0x10, 0x00, 0x00, 0x45,
      0xD4, 0x2D, 0x19, 0x80, 0x13, 0x00, 0x00, 0x45, 0xDC, 0x2C, 0x80, 0xB0, 0x10, 0x00, 0x00,
      0x45, 0xE8, 0x2D, 0x19, 0x80, 0x13, 0x00, 0x00, 0x49, 0xC4, 0x2C, 0x00, 0x68, 0x0A, 0x00,
      0x00, 0x49, 0xD0, 0x28, 0x1B, 0x80, 0x13, 0x00, 0x00, 0x49, 0xD8, 0x2C, 0x80, 0x78, 0x0A,
      0x00, 0x00, 0x49, 0xE4, 0x28, 0x1B, 0x80, 0x13, 0x00, 0x00, 0x49, 0xF0, 0x2C, 0x00, 0x68,
      0x08, 0x00, 0x00, 0x49, 0xFC, 0x23, 0x1B, 0x80, 0x13, 0x00, 0x00, 0x4A, 0x04, 0x2C, 0x80,
      0x78, 0x08, 0x00, 0x00, 0x4A, 0x10, 0x23, 0x1B, 0x80, 0x13, 0x00, 0x00, 0x5C, 0x98, 0x1E,
      0x0C, 0x80, 0x80, 0x00, 0x00, 0x5C, 0xF4, 0xB4, 0x80, 0x0C, 0x90, 0x00, 0x00, 0x5D, 0x08,
      0xB4, 0x80, 0x0C, 0x90, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x3A, 0x1C, 0xB4, 0x94, 0x00,
      0x13, 0x00, 0x00, 0x3A, 0x64, 0x2C, 0x00, 0x00, 0x15, 0x00, 0x00, 0x3A, 0x70, 0xB4, 0x92,
      0x80, 0x13, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0x00, 0x64, 0x7C, 0xB4, 0x9A, 0x40, 0x17, 0x00, 0x00, 0x64, 0x80,
      0x64, 0x00, 0x10, 0x97, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x33,
      0xE4, 0x42, 0xDE, 0x00, 0x00, 0x00, 0x00, 0x45, 0x28, 0x2C, 0x01, 0x30, 0x11, 0x00, 0x00,
      0x45, 0x34, 0xB4, 0x98, 0x80, 0x13, 0x00, 0x00, 0x45, 0x3C, 0x2C, 0x81, 0x30, 0x11, 0x00,
      0x00, 0x45, 0x48, 0xB4, 0x98, 0x80, 0x13, 0x00, 0x00, 0x45, 0x50, 0x2D, 0x00, 0x20, 0x11,
      0x00, 0x00, 0x45, 0x5C, 0xB4, 0x98, 0x80, 0x13, 0x00, 0x00, 0x45, 0xF8, 0x2C, 0x01, 0x30,
      0x0F, 0x00, 0x00, 0x46, 0x08, 0x0F, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x46, 0x0C, 0x2C, 0x81,
      0x28, 0x0F, 0x00, 0x00, 0x46, 0x1C, 0x0F, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x4A, 0xEC, 0x2C,
      0x00, 0x70, 0x03, 0x00, 0x00, 0x4B, 0x00, 0x2C, 0x80, 0x38, 0x03, 0x00, 0x00, 0x00, 0xFF,
      0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x48, 0x5C, 0x2C, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
      0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x37, 0xB0, 0x3F, 0x59, 0x99, 0x9A, 0x00, 0x00,
      0x37, 0xCC, 0x42, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x55, 0x20, 0x87, 0x11, 0x80, 0x13, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x3B, 0x8C, 0x44, 0x0C, 0x00, 0x00,
      0x00, 0x00, 0x3D, 0x0C, 0x44, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
      0xFF, 0x00, 0x00, 0x50, 0xE4, 0xB4, 0x99, 0x00, 0x13, 0x00, 0x00, 0x50, 0xF8, 0xB4, 0x99,
      0x00, 0x13, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
      0x00, 0x4E, 0xB0, 0x02, 0xBC, 0xFF, 0x38, 0x00, 0x00, 0x4E, 0xBC, 0x14, 0x00, 0x01, 0x23,
      0x00, 0x00, 0x4E, 0xC4, 0x03, 0x84, 0x01, 0xF4, 0x00, 0x00, 0x4E, 0xD0, 0x14, 0x00, 0x01,
      0x23, 0x00, 0x00, 0x4E, 0xD8, 0x04, 0x4C, 0x04, 0xB0, 0x00, 0x00, 0x4E, 0xE4, 0x14, 0x00,
      0x01, 0x23, 0x00, 0x00, 0x50, 0x5C, 0x2C, 0x00, 0x68, 0x15, 0x00, 0x00, 0x50, 0x6C, 0x14,
      0x08, 0x01, 0x23, 0x00, 0x00, 0x50, 0x70, 0x2C, 0x80, 0x60, 0x15, 0x00, 0x00, 0x50, 0x80,
      0x14, 0x08, 0x01, 0x23, 0x00, 0x00, 0x50, 0x84, 0x2D, 0x00, 0x20, 0x15, 0x00, 0x00, 0x50,
      0x94, 0x14, 0x08, 0x01, 0x23, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xBA, 0x81,
      0x00, 0x08, 0x80, 0x01, 0x00, 0xB4, 0x38, 0x21, 0x00, 0xB0, 0x7C, 0x08, 0x03, 0xA6, 0x3C,
      0x60, 0x80, 0x3C, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC2, 0x2F, 0x9A, 0x3C,
      0x00, 0x00, 0x00, 0x08,  // #Common/PAL/Handlers/PAL Stock Icons.asm
      0x88, 0x62, 0xF2, 0x34, 0x2C, 0x03, 0x00, 0x00, 0x41, 0x82, 0x00, 0x30, 0x48, 0x00, 0x00,
      0x21, 0x7C, 0x88, 0x02, 0xA6, 0x80, 0x64, 0x00, 0x00, 0x90, 0x7D, 0x00, 0x2C, 0x90, 0x7D,
      0x00, 0x30, 0x80, 0x64, 0x00, 0x04, 0x90, 0x7D, 0x00, 0x3C, 0x48, 0x00, 0x00, 0x10, 0x4E,
      0x80, 0x00, 0x21, 0x3F, 0x59, 0x99, 0x9A, 0xC1, 0xA8, 0x00, 0x00, 0x80, 0x1D, 0x00, 0x14,
      0x00, 0x00, 0x00, 0x00, 0xC2, 0x10, 0xFC, 0x44, 0x00, 0x00, 0x00,
      0x04,  // #Common/PAL/Handlers/DK
             // Up B/Aerial Up B.asm
      0x88, 0x82, 0xF2, 0x34, 0x2C, 0x04, 0x00, 0x00, 0x41, 0x82, 0x00, 0x10, 0x3C, 0x00, 0x80,
      0x11, 0x60, 0x00, 0x00, 0x74, 0x48, 0x00, 0x00, 0x08, 0x38, 0x03, 0xD7, 0x74, 0x00, 0x00,
      0x00, 0x00, 0xC2, 0x10, 0xFB, 0x64, 0x00, 0x00, 0x00, 0x04,  // #Common/PAL/Handlers/DK Up
                                                                   // B/Grounded Up B.asm
      0x88, 0x82, 0xF2, 0x34, 0x2C, 0x04, 0x00, 0x00, 0x41, 0x82, 0x00, 0x10, 0x3C, 0x00, 0x80,
      0x11, 0x60, 0x00, 0x00, 0x74, 0x48, 0x00, 0x00, 0x08, 0x38, 0x03, 0xD7, 0x74, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Termination sequence
  };

  static std::unordered_map<u32, bool> static_deny_list = {
      {0x8008d698, true},  // Recording/GetLCancelStatus/GetLCancelStatus.asm
      {0x8006c324, true},  // Recording/GetLCancelStatus/ResetLCancelStatus.asm
      {0x800679bc, true},  // Recording/ExtendPlayerBlock.asm
      {0x802fef88, true},  // Recording/FlushFrameBuffer.asm
      {0x80005604, true},  // Recording/IsVSMode.asm
      {0x8016d30c, true},  // Recording/SendGameEnd.asm
      {0x8016e74c, true},  // Recording/SendGameInfo.asm
      {0x8006c5d8, true},  // Recording/SendGamePostFrame.asm
      {0x8006b0dc, true},  // Recording/SendGamePreFrame.asm
      {0x803219ec, true},  // 3.4.0: Recording/FlushFrameBuffer.asm (Have to keep old ones for
                           // backward compatibility)
      {0x8006da34, true},  // 3.4.0: Recording/SendGamePostFrame.asm
      {0x8016d884, true},  // 3.7.0: Recording/SendGameEnd.asm

      {0x8021aae4, true},  // Binary/FasterMeleeSettings/DisableFdTransitions.bin
      {0x801cbb90, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CC8AC, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CBE9C, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CBEF0, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CBF54, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x80390838, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CD250, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801CCDCC, true},  // Binary/FasterMeleeSettings/LaglessFod.bin
      {0x801C26B0, true},  // Binary/FasterMeleeSettings/RandomStageMusic.bin
      {0x803761ec, true},  // Binary/NormalLagReduction.bin
      {0x800198a4, true},  // Binary/PerformanceLagReduction.bin
      {0x80019620, true},  // Binary/PerformanceLagReduction.bin
      {0x801A5054, true},  // Binary/PerformanceLagReduction.bin
      {0x80397878, true},  // Binary/OsReportPrintOnCrash.bin
      {0x801A4DA0, true},  // Binary/LagReduction/PD.bin
      {0x801A4DB4, true},  // Binary/LagReduction/PD.bin
      {0x80019860, true},  // Binary/LagReduction/PD.bin
      {0x801A4C24, true},  // Binary/LagReduction/PD+VB.bin
      {0x8001985C, true},  // Binary/LagReduction/PD+VB.bin
      {0x80019860, true},  // Binary/LagReduction/PD+VB.bin
      {0x80376200, true},  // Binary/LagReduction/PD+VB.bin
      {0x801A5018, true},  // Binary/LagReduction/PD+VB.bin
      {0x80218D68, true},  // Binary/LagReduction/PD+VB.bin
      {0x8016E9AC, true},  // Binary/Force2PCenterHud.bin
      {0x80030E44, true},  // Binary/DisableScreenShake.bin
      {0x803761EC, true},  // Binary/NormalLagReduction.bin
      {0x80376238, true},  // Binary/NormalLagReduction.bin

      {0x800055f0, true},  // Common/EXITransferBuffer.asm
      {0x800055f8, true},  // Common/GetIsFollower.asm
      {0x800055fc, true},  // Common/Gecko/ProcessCodeList.asm
      {0x8016d294, true},  // Common/IncrementFrameIndex.asm
      {0x80376a24, true},  // Common/UseInGameDelay/ApplyInGameDelay.asm
      {0x8016e9b0, true},  // Common/UseInGameDelay/InitializeInGameDelay.asm
      {0x8000561c, true},  // Common/GetCommonMinorID/GetCommonMinorID.asm
      {0x802f666c, true},  // Common/UseInGameDelay/InitializeInGameDelay.asm v2
      {0x8000569c, true},  // Common/CompatibilityHooks/GetFighterNum.asm
      {0x800056a0, true},  // Common/CompatibilityHooks/GetSSMIndex.asm
      {0x800056a8, true},  // Common/CompatibilityHooks/RequestSSMLoad.asm

      {0x801a5b14, true},  // External/Salty Runback/Salty Runback.asm
      {0x801a4570, true},  // External/LagReduction/ForceHD/480pDeflickerOff.asm
      {0x802fccd8, true},  // External/Hide Nametag When Invisible/Hide Nametag When Invisible.asm

      {0x804ddb30, true},  // External/Widescreen/Adjust Offscreen Scissor/Fix Bubble
                           // Positions/Adjust Corner Value 1.asm
      {0x804ddb34, true},  // External/Widescreen/Adjust Offscreen Scissor/Fix Bubble
                           // Positions/Adjust Corner Value 2.asm
      {0x804ddb2c, true},  // External/Widescreen/Adjust Offscreen Scissor/Fix Bubble
                           // Positions/Extend Negative Vertical Bound.asm
      {0x804ddb28, true},  // External/Widescreen/Adjust Offscreen Scissor/Fix Bubble
                           // Positions/Extend Positive Vertical Bound.asm
      {0x804ddb4c, true},  // External/Widescreen/Adjust Offscreen Scissor/Fix Bubble
                           // Positions/Widen Bubble Region.asm
      {0x804ddb58, true},  // External/Widescreen/Adjust Offscreen Scissor/Adjust Bubble Zoom.asm
      {0x80086b24, true},  // External/Widescreen/Adjust Offscreen Scissor/Draw High Poly Models.asm
      {0x80030C7C, true},  // External/Widescreen/Adjust Offscreen Scissor/Left Camera Bound.asm
      {0x80030C88, true},  // External/Widescreen/Adjust Offscreen Scissor/Right Camera Bound.asm
      {0x802fcfc4,
       true},  // External/Widescreen/Nametag Fixes/Adjust Nametag Background X Scale.asm
      {0x804ddb84, true},  // External/Widescreen/Nametag Fixes/Adjust Nametag Text X Scale.asm
      {0x803BB05C, true},  // External/Widescreen/Fix Screen Flash.asm
      {0x8036A4A8, true},  // External/Widescreen/Overwrite CObj Values.asm
      {0x80302784, true},  // External/Monitor4-3/Add Shutters.asm
      {0x800C0148, true},  // External/FlashRedFailedLCancel/ChangeColor.asm
      {0x8008D690, true},  // External/FlashRedFailedLCancel/TriggerColor.asm

      {0x801A4DB4, true},  // Online/Core/ForceEngineOnRollback.asm
      {0x8016D310, true},  // Online/Core/HandleLRAS.asm
      {0x8034DED8, true},  // Online/Core/HandleRumble.asm
      {0x8016E748, true},  // Online/Core/InitOnlinePlay.asm
      {0x8016e904, true},  // Online/Core/InitPause.asm
      {0x801a5014, true},  // Online/Core/LoopEngineForRollback.asm
      {0x801a4de4, true},  // Online/Core/StartEngineLoop.asm
      {0x80376A28, true},  // Online/Core/TriggerSendInput.asm
      {0x801a4cb4, true},  // Online/Core/EXIFileLoad/AllocBuffer.asm
      {0x800163fc, true},  // Online/Core/EXIFileLoad/GetFileSize.asm
      {0x800166b8, true},  // Online/Core/EXIFileLoad/TransferFile.asm
      {0x80019260, true},  // Online/Core/Hacks/ForceNoDiskCrash.asm
      {0x80376304, true},  // Online/Core/Hacks/ForceNoVideoAssert.asm
      {0x80321d70, true},  // Online/Core/Hacks/PreventCharacterCrowdChants.asm
      {0x80019608, true},  // Online/Core/Hacks/PreventPadAlarmDuringRollback.asm
      {0x8038D224, true},  // Online/Core/Sound/AssignSoundInstanceId.asm
      {0x80088224, true},  // Online/Core/Sound/NoDestroyVoice.asm
      {0x800882B0, true},  // Online/Core/Sound/NoDestroyVoice2.asm
      {0x8038D0B0, true},  // Online/Core/Sound/PreventDuplicateSounds.asm
      {0x803775b8, true},  // Online/Logging/LogInputOnCopy.asm
      {0x8016e9b4, true},  // Online/Menus/InGame/InitInGame.asm
      {0x80185050, true},  // Online/Menus/VSScreen/HideStageDisplay/PreventEarlyR3Overwrite.asm
      {0x80184b1c, true},  // Online/Menus/VSScreen/HideStageText/SkipStageNumberShow.asm
      {0x801A45BC, true},  // Online/Slippi Online Scene/main.asm
      {0x801a45b8, true},  // Online/Slippi Online Scene/main.asm (https://bit.ly/3kxohf4)
      {0x801BFA20, true},  // Online/Slippi Online Scene/boot.asm
      {0x800cc818, true},  // External/GreenDuringWait/fall.asm
      {0x8008a478, true},  // External/GreenDuringWait/wait.asm

      {0x802f6690, true},  // HUD Transparency v1.1
                           // (https://smashboards.com/threads/transparent-hud-v1-1.508509/)
      {0x802F71E0,
       true},  // Smaller "Ready, GO!" (https://smashboards.com/threads/smaller-ready-go.509740/)
      {0x80071960,
       true},  // Yellow During IASA
               // (https://smashboards.com/threads/color-overlays-for-iasa-frames.401474/post-19120928)
  };

  std::unordered_map<u32, bool> deny_list;
  deny_list.insert(static_deny_list.begin(), static_deny_list.end());

  auto replayCommSettings = g_replayComm->getSettings();
  if (replayCommSettings.rollbackDisplayMethod == "off")
  {
    // Some codes should only be blacklisted when not displaying rollbacks, these are codes
    // that are required for things to not break when using Slippi savestates. Perhaps this
    // should be handled by actually applying these codes in the playback ASM instead? not sure
    deny_list[0x8038add0] = true;  // Online/Core/PreventFileAlarms/PreventMusicAlarm.asm
    deny_list[0x80023FFC] = true;  // Online/Core/PreventFileAlarms/MuteMusic.asm
  }

  geckoList.clear();

  Slippi::GameSettings* settings = m_current_game->GetSettings();
  if (settings->geckoCodes.empty())
  {
    geckoList = defaultCodeList;
    return;
  }

  std::vector<u8> source = settings->geckoCodes;
  INFO_LOG_FMT(SLIPPI, "Booting codes with source size: {}", source.size());

  int idx = 0;
  while (idx < source.size())
  {
    u8 codeType = source[idx] & 0xFE;
    u32 address =
        source[idx] << 24 | source[idx + 1] << 16 | source[idx + 2] << 8 | source[idx + 3];
    address = (address & 0x01FFFFFF) | 0x80000000;

    u32 codeOffset = 8;  // Default code offset. Most codes are this length
    switch (codeType)
    {
    case 0xC0:
    case 0xC2:
    {
      u32 lineCount =
          source[idx + 4] << 24 | source[idx + 5] << 16 | source[idx + 6] << 8 | source[idx + 7];
      codeOffset = 8 + (lineCount * 8);
      break;
    }
    case 0x08:
      codeOffset = 16;
      break;
    case 0x06:
    {
      u32 byteLen =
          source[idx + 4] << 24 | source[idx + 5] << 16 | source[idx + 6] << 8 | source[idx + 7];
      // Round up to next 8 bytes and add the first 8 bytes
      codeOffset = 8 + ((byteLen + 7) & 0xFFFFFFF8);
      break;
    }
    }

    idx += codeOffset;

    // If this address is blacklisted, we don't add it to what we will send to game
    if (deny_list.count(address))
      continue;

    INFO_LOG_FMT(SLIPPI, "Codetype [{:x}] Inserting section: {} - {} ({:x}, {})", codeType,
                 idx - codeOffset, idx, address, codeOffset);

    // If not blacklisted, add code to return vector
    geckoList.insert(geckoList.end(), source.begin() + (idx - codeOffset), source.begin() + idx);
  }

  // Add the termination sequence
  geckoList.insert(geckoList.end(), {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

void CEXISlippi::prepareCharacterFrameData(Slippi::FrameData* frame, u8 port, u8 isFollower)
{
  std::unordered_map<uint8_t, Slippi::PlayerFrameData> source;
  source = isFollower ? frame->followers : frame->players;

  // This must be updated if new data is added
  int characterDataLen = 49;

  // Check if player exists
  if (!source.count(port))
  {
    // If player does not exist, insert blank section
    m_read_queue.insert(m_read_queue.end(), characterDataLen, 0);
    return;
  }

  // Get data for this player
  Slippi::PlayerFrameData data = source[port];

  // log << frameIndex << "\t" << port << "\t" << data.locationX << "\t" << data.locationY << "\t"
  // << data.animation
  // << "\n";

  // WARN_LOG_FMT(EXPANSIONINTERFACE, "[Frame {}] [Player {}] Positions: %f | %f", frameIndex, port,
  // data.locationX, data.locationY);

  // Add all of the inputs in order
  appendWordToBuffer(&m_read_queue, data.randomSeed);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.joystickX);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.joystickY);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.cstickX);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.cstickY);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.trigger);
  appendWordToBuffer(&m_read_queue, data.buttons);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.locationX);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.locationY);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.facingDirection);
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.animation));
  m_read_queue.push_back(data.joystickXRaw);
  appendWordToBuffer(&m_read_queue, *(u32*)&data.percent);
  // NOTE TO DEV: If you add data here, make sure to increase the size above
}

bool CEXISlippi::checkFrameFullyFetched(s32 frameIndex)
{
  auto doesFrameExist = m_current_game->DoesFrameExist(frameIndex);
  if (!doesFrameExist)
    return false;

  Slippi::FrameData* frame = m_current_game->GetFrame(frameIndex);

  version::Semver200_version lastFinalizedVersion("3.7.0");
  version::Semver200_version currentVersion(m_current_game->GetVersionString());

  bool frameIsFinalized = true;
  if (currentVersion >= lastFinalizedVersion)
  {
    // If latest finalized frame should exist, check it as well. This will prevent us
    // from loading a non-committed frame when mirroring a rollback game
    frameIsFinalized = m_current_game->GetLastFinalizedFrame() >= frameIndex;
  }

  // This flag is set to true after a post frame update has been received. At that point
  // we know we have received all of the input data for the frame
  return frame->inputsFullyFetched && frameIsFinalized;
}

void CEXISlippi::prepareFrameData(u8* payload)
{
  // Since we are prepping new data, clear any existing data
  m_read_queue.clear();

  if (!m_current_game)
  {
    // Do nothing if we don't have a game loaded
    return;
  }

  // Parse input
  s32 frameIndex = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];

  // If loading from queue, move on to the next replay if we have past endFrame
  auto watchSettings = g_replayComm->current;
  if (frameIndex > watchSettings.endFrame)
  {
    INFO_LOG_FMT(SLIPPI, "Killing game because we are past endFrame");
    m_read_queue.push_back(FRAME_RESP_TERMINATE);
    return;
  }

  // If a new replay should be played, terminate the current game
  auto isNewReplay = g_replayComm->isNewReplay();
  if (isNewReplay)
  {
    m_read_queue.push_back(FRAME_RESP_TERMINATE);
    return;
  }

  auto isProcessingComplete = m_current_game->IsProcessingComplete();
  // Wait until frame exists in our data before reading it. We also wait until
  // next frame has been found to ensure we have actually received all of the
  // data from this frame. Don't wait until next frame is processing is complete
  // (this is the last frame, in that case)
  auto isFrameFound = m_current_game->DoesFrameExist(frameIndex);
  g_playbackStatus->lastFrame = m_current_game->GetLatestIndex();
  auto isFrameComplete = checkFrameFullyFetched(frameIndex);
  auto isFrameReady = isFrameFound && (isProcessingComplete || isFrameComplete);

  // If there is a startFrame configured, manage the fast-forward flag
  if (watchSettings.startFrame > Slippi::GAME_FIRST_FRAME)
  {
    if (frameIndex < watchSettings.startFrame)
    {
      g_playbackStatus->setHardFFW(true);
    }
    else if (frameIndex == watchSettings.startFrame)
    {
      // TODO: This might disable fast forward on first frame when we dont want to?
      g_playbackStatus->setHardFFW(false);
    }
  }

  auto commSettings = g_replayComm->getSettings();
  if (commSettings.rollbackDisplayMethod == "normal")
  {
    auto nextFrame = m_current_game->GetFrameAt(frameSeqIdx);
    bool shouldHardFFW = nextFrame && nextFrame->frame <= g_playbackStatus->currentPlaybackFrame;
    g_playbackStatus->setHardFFW(shouldHardFFW);

    if (nextFrame)
    {
      // This feels jank but without this g_playbackStatus ends up getting updated to
      // a value beyond the frame that actually gets played causes too much FFW
      frameIndex = nextFrame->frame;
    }
  }

  // If RealTimeMode is enabled, let's trigger fast forwarding under certain conditions
  auto isFarBehind = g_playbackStatus->lastFrame - frameIndex > 2;
  auto isVeryFarBehind = g_playbackStatus->lastFrame - frameIndex > 25;
  if (isFarBehind && commSettings.mode == "mirror" && commSettings.isRealTimeMode)
  {
    g_playbackStatus->isSoftFFW = true;

    // Once isHardFFW has been turned on, do not turn it off with this condition, should
    // hard FFW to the latest point
    if (!g_playbackStatus->isHardFFW)
      g_playbackStatus->isHardFFW = isVeryFarBehind;
  }

  if (g_playbackStatus->lastFrame == frameIndex)
  {
    // The reason to disable fast forwarding here is in hopes
    // of disabling it on the last frame that we have actually received.
    // Doing this will allow the rendering logic to run to display the
    // last frame instead of the frame previous to fast forwarding.
    // Not sure if this fully works with partial frames
    g_playbackStatus->isSoftFFW = false;
    g_playbackStatus->setHardFFW(false);
  }

  bool shouldFFW = g_playbackStatus->shouldFFWFrame(frameIndex);
  u8 requestResultCode = shouldFFW ? FRAME_RESP_FASTFORWARD : FRAME_RESP_CONTINUE;
  if (!isFrameReady)
  {
    // If processing is complete, the game has terminated early. Tell our playback
    // to end the game as well.
    auto shouldTerminateGame = isProcessingComplete;
    requestResultCode = shouldTerminateGame ? FRAME_RESP_TERMINATE : FRAME_RESP_WAIT;
    m_read_queue.push_back(requestResultCode);

    // Disable fast forward here too... this shouldn't be necessary but better
    // safe than sorry I guess
    g_playbackStatus->isSoftFFW = false;
    g_playbackStatus->setHardFFW(false);

    if (requestResultCode == FRAME_RESP_TERMINATE)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Game should terminate on frame {} [{}]", frameIndex,
                    frameIndex);
    }

    return;
  }

  u8 rollbackCode = 0;  // 0 = not rollback, 1 = rollback, perhaps other options in the future?

  // Increment frame index if greater
  if (frameIndex > g_playbackStatus->currentPlaybackFrame)
  {
    g_playbackStatus->currentPlaybackFrame = frameIndex;
  }
  else if (commSettings.rollbackDisplayMethod != "off")
  {
    rollbackCode = 1;
  }

  // Keep track of last FFW frame, used for soft FFW's
  if (shouldFFW)
  {
    WARN_LOG_FMT(SLIPPI, "[Frame {}] FFW frame, behind by: {} frames.", frameIndex,
                 g_playbackStatus->lastFrame - frameIndex);
    g_playbackStatus->lastFFWFrame = frameIndex;
  }

  // Return success code
  m_read_queue.push_back(requestResultCode);

  // Get frame
  Slippi::FrameData* frame = m_current_game->GetFrame(frameIndex);
  if (commSettings.rollbackDisplayMethod != "off")
  {
    auto previousFrame = m_current_game->GetFrameAt(frameSeqIdx - 1);
    frame = m_current_game->GetFrameAt(frameSeqIdx);

    *(s32*)(&playbackSavestatePayload[0]) = Common::swap32(frame->frame);

    if (previousFrame && frame->frame <= previousFrame->frame)
    {
      // Here we should load a savestate
      handleLoadSavestate(&playbackSavestatePayload[0]);
    }

    // Here we should save a savestate
    handleCaptureSavestate(&playbackSavestatePayload[0]);

    frameSeqIdx += 1;
  }

  // For normal replays, modify slippi seek/playback data as needed
  // TODO: maybe handle other modes too?
  if (commSettings.mode == "normal" || commSettings.mode == "queue")
  {
    g_playbackStatus->prepareSlippiPlayback(frame->frame);
  }

  // Push RB code
  m_read_queue.push_back(rollbackCode);

  // Add frame rng seed to be restored at priority 0
  u8 rngResult = frame->randomSeedExists ? 1 : 0;
  m_read_queue.push_back(rngResult);
  appendWordToBuffer(&m_read_queue, *(u32*)&frame->randomSeed);

  // Add frame data for every character
  for (u8 port = 0; port < 4; port++)
  {
    prepareCharacterFrameData(frame, port, 0);
    prepareCharacterFrameData(frame, port, 1);
  }
}

void CEXISlippi::prepareIsStockSteal(u8* payload)
{
  // Since we are prepping new data, clear any existing data
  m_read_queue.clear();

  if (!m_current_game)
  {
    // Do nothing if we don't have a game loaded
    return;
  }

  // Parse args
  s32 frameIndex = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
  u8 playerIndex = payload[4];

  // I'm not sure checking for the frame should be necessary. Theoretically this
  // should get called after the frame request so the frame should already exist
  auto isFrameFound = m_current_game->DoesFrameExist(frameIndex);
  if (!isFrameFound)
  {
    m_read_queue.push_back(0);
    return;
  }

  // Load the data from this frame into the read buffer
  Slippi::FrameData* frame = m_current_game->GetFrame(frameIndex);
  auto players = frame->players;

  u8 playerIsBack = players.count(playerIndex) ? 1 : 0;
  m_read_queue.push_back(playerIsBack);
}

void CEXISlippi::prepareIsFileReady()
{
  m_read_queue.clear();

  auto isNewReplay = g_replayComm->isNewReplay();
  if (!isNewReplay)
  {
    g_replayComm->nextReplay();
    m_read_queue.push_back(0);
    return;
  }

  // Attempt to load game if there is a new replay file
  // this can come pack falsy if the replay file does not exist
  m_current_game = g_replayComm->loadGame();
  if (!m_current_game)
  {
    // Do not start if replay file doesn't exist
    // TODO: maybe display error message?
    INFO_LOG_FMT(SLIPPI, "EXI_DeviceSlippi.cpp: Replay file does not exist?");
    m_read_queue.push_back(0);
    return;
  }

  INFO_LOG_FMT(SLIPPI, "EXI_DeviceSlippi.cpp: Replay file loaded successfully!?");

  // Clear playback control related vars
  g_playbackStatus->resetPlayback();

  // Start the playback!
  m_read_queue.push_back(1);
}

bool CEXISlippi::isDisconnected()
{
  if (!slippi_netplay)
    return true;

  auto status = slippi_netplay->GetSlippiConnectStatus();
  return status != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
}

void CEXISlippi::handleOnlineInputs(u8* payload)
{
  m_read_queue.clear();

  s32 frame = Common::swap32(&payload[0]);
  s32 finalized_frame = Common::swap32(&payload[4]);
  u32 finalized_frame_checksum = Common::swap32(&payload[8]);
  u8 delay = payload[12];
  u8* inputs = &payload[13];

  if (frame == 1)
  {
    availableSavestates.clear();
    activeSavestates.clear();

    // Prepare savestates for online play
    for (int i = 0; i < ROLLBACK_MAX_FRAMES; i++)
    {
      availableSavestates.push_back(std::make_unique<SlippiSavestate>());
    }

    // Reset stall counter
    isConnectionStalled = false;
    stallFrameCount = 0;

    // Reset skip variables
    framesToSkip = 0;
    isCurrentlySkipping = false;

    // Reset advance stuff
    framesToAdvance = 0;
    isCurrentlyAdvancing = false;
    fallBehindCounter = 0;
    fallFarBehindCounter = 0;

    // Reset character selections such that they are cleared for next game
    localSelections.Reset();
    if (slippi_netplay)
      slippi_netplay->StartSlippiGame();
  }

  if (isDisconnected())
  {
    m_read_queue.push_back(3);  // Indicate we disconnected
    return;
  }

  // Drop inputs that we no longer need (inputs older than the finalized frame passed in)
  slippi_netplay->DropOldRemoteInputs(finalized_frame);

  bool shouldSkip = shouldSkipOnlineFrame(frame, finalized_frame);
  if (shouldSkip)
  {
    // Send inputs that have not yet been acked
    slippi_netplay->SendSlippiPad(nullptr);
  }
  else
  {
    // Send the input for this frame along with everything that has yet to be acked
    handleSendInputs(frame, delay, finalized_frame, finalized_frame_checksum, inputs);
  }

  prepareOpponentInputs(frame, shouldSkip);
}

bool CEXISlippi::shouldSkipOnlineFrame(s32 frame, s32 finalizedFrame)
{
  auto status = slippi_netplay->GetSlippiConnectStatus();
  bool connectionFailed =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
  bool connectionDisconnected =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (connectionFailed || connectionDisconnected)
  {
    // If connection failed just continue the game
    return false;
  }

  if (isConnectionStalled)
  {
    return false;
  }

  // Return true if we are too far ahead for rollback. ROLLBACK_MAX_FRAMES is the number of frames
  // we can receive for the opponent at one time and is our "look-ahead" limit
  // Example: finalizedFrame = 100 means the last savestate we need is 101. We can then store
  // states 101 to 107 before running out of savestates. So 107 - 100 = 7. We need to make sure
  // we have enough inputs to finalize to not overflow the available states, so if our latest frame
  // is 101, we can't let frame 109 be created. 101 - 100 >= 109 - 100 - 7 : 1 >= 2 (false).
  // It has to work this way because we only have room to move our states forward by one for frame
  // 108
  s32 latestRemoteFrame = slippi_netplay->GetSlippiLatestRemoteFrame(ROLLBACK_MAX_FRAMES);
  auto hasEnoughNewInputs =
      latestRemoteFrame - finalizedFrame >= (frame - finalizedFrame - ROLLBACK_MAX_FRAMES);
  if (!hasEnoughNewInputs)
  {
    stallFrameCount++;
    if (stallFrameCount > 60 * 7)
    {
      // 7 second stall will disconnect game
      isConnectionStalled = true;
    }

    WARN_LOG_FMT(
        SLIPPI_ONLINE,
        "Halting for one frame due to rollback limit (frame: {} | latest: {} | finalized: {})...",
        frame, latestRemoteFrame, finalizedFrame);
    return true;
  }

  stallFrameCount = 0;

  s32 frameTime = 16683;
  s32 t1 = 10000;
  s32 t2 = (2 * frameTime) + t1;

  // Only skip once for a given frame because our time detection method doesn't take into
  // consideration waiting for a frame. Also it's less jarring and it happens often enough that it
  // will smoothly get to the right place
  auto isTimeSyncFrame = frame % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;  // Only time sync every 30 frames
  if (isTimeSyncFrame == 0 && !isCurrentlySkipping)
  {
    auto offsetUs = slippi_netplay->CalcTimeOffsetUs();
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Frame {}] Offset for skip is: {} us", frame, offsetUs);

    // At the start of the game, let's make sure to sync perfectly, but after that let the slow
    // instance try to do more work before we stall

    // The decision to skip a frame only happens when we are already pretty far off ahead. The hope
    // is that this won't really be used much because the frame advance of the slow client along
    // with dynamic emulation speed will pick up the difference most of the time. But at some point
    // it's probably better to slow down...
    if (offsetUs > (frame <= 120 ? t1 : t2))
    {
      isCurrentlySkipping = true;

      int maxSkipFrames = frame <= 120 ? 5 : 1;  // On early frames, support skipping more frames
      framesToSkip = ((offsetUs - t1) / frameTime) + 1;
      framesToSkip =
          framesToSkip > maxSkipFrames ? maxSkipFrames : framesToSkip;  // Only skip 5 frames max

      WARN_LOG_FMT(SLIPPI_ONLINE,
                   "Halting on frame {} due to time sync. Offset: {} us. Frames: {}...", frame,
                   offsetUs, framesToSkip);
    }
  }

  // Handle the skipped frames
  if (framesToSkip > 0)
  {
    // If ahead by 60% of a frame, stall. I opted to use 60% instead of half a frame
    // because I was worried about two systems continuously stalling for each other
    framesToSkip = framesToSkip - 1;
    return true;
  }

  isCurrentlySkipping = false;

  return false;
}

bool CEXISlippi::shouldAdvanceOnlineFrame(s32 frame)
{
  // Logic below is used to test frame advance by forcing it more often
  // SConfig::GetInstance().m_EmulationSpeed = 0.5f;
  // if (frame > 120 && frame % 10 < 3)
  //{
  //	Common::SleepCurrentThread(1); // Sleep to try to let inputs come in to make late rollbacks
  // more likely 	return true;
  //}

  // return false;
  // return frame % 2 == 0;

  // Return true if we are over 60% of a frame behind our opponent. We limit how often this happens
  // to get a reliable average to act on. We will allow advancing up to 5 frames (spread out) over
  // the 30 frame period. This makes the game feel relatively smooth still
  auto isTimeSyncFrame =
      (frame % SLIPPI_ONLINE_LOCKSTEP_INTERVAL) == 0;  // Only time sync every 30 frames
  if (isTimeSyncFrame)
  {
    auto offsetUs = slippi_netplay->CalcTimeOffsetUs();

    // Dynamically adjust emulation speed in order to fine-tune time sync to reduce one sided
    // rollbacks even more
    // Modify emulation speed up to a max of 1% at 3 frames offset or more. Don't slow down the
    // front instance as much because we want to prioritize performance for the fast PC
    float deviation = 0;
    float maxSlowDownAmount = 0.005f;
    float maxSpeedUpAmount = 0.01f;
    int frameWindow = 3;
    if (offsetUs > -250 && offsetUs < 8000)
    {
      // Do nothing, leave deviation at 0 for 100% emulation speed when ahead by 8 ms or less
    }
    else if (offsetUs < 0)
    {
      // Here we are behind, so let's speed up our instance
      float frameWindowMultiplier = std::min(-offsetUs / (frameWindow * 16683.0f), 1.0f);
      deviation = frameWindowMultiplier * maxSpeedUpAmount;
    }
    else
    {
      // Here we are ahead, so let's slow down our instance
      float frameWindowMultiplier = std::min(offsetUs / (frameWindow * 16683.0f), 1.0f);
      deviation = frameWindowMultiplier * -maxSlowDownAmount;
    }

    auto dynamicEmulationSpeed = 1.0f + deviation;
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED, dynamicEmulationSpeed);
    // SConfig::GetInstance().m_EmulationSpeed = dynamicEmulationSpeed;
    //  SConfig::GetInstance().m_EmulationSpeed = 0.97f; // used for testing

    INFO_LOG_FMT(SLIPPI_ONLINE, "[Frame {}] Offset for advance is: {} us. New speed: {.4}%", frame,
                 offsetUs, dynamicEmulationSpeed * 100.0f);

    s32 frameTime = 16683;
    s32 t1 = 10000;
    s32 t2 = frameTime + t1;

    // Count the number of times we're below a threshold we should easily be able to clear. This is
    // checked twice per second.
    fallBehindCounter += offsetUs < -t1 ? 1 : 0;
    fallFarBehindCounter += offsetUs < -t2 ? 1 : 0;

    bool isSlow =
        (offsetUs < -t1 && fallBehindCounter > 50) || (offsetUs < -t2 && fallFarBehindCounter > 15);
    if (isSlow && lastSearch.mode != SlippiMatchmaking::OnlinePlayMode::TEAMS)
    {
      // We don't show this message for teams because it seems to false positive a lot there, maybe
      // because the min offset is always selected? Idk I feel like doubles has some perf issues I
      // don't understand atm.
      OSD::AddTypedMessage(
          OSD::MessageType::PerformanceWarning,
          "Possible poor match performance detected.\nIf this message appears with most opponents, "
          "your computer or network is likely impacting match performance for the other players.",
          10000, OSD::Color::RED);
    }

    if (offsetUs < -t2 && !isCurrentlyAdvancing)
    {
      isCurrentlyAdvancing = true;

      // On early frames, don't advance any frames. Let the stalling logic handle the initial sync
      int maxAdvFrames = frame > 120 ? 3 : 0;
      framesToAdvance = ((-offsetUs - t1) / frameTime) + 1;
      framesToAdvance = framesToAdvance > maxAdvFrames ? maxAdvFrames : framesToAdvance;

      WARN_LOG_FMT(SLIPPI_ONLINE,
                   "Advancing on frame {} due to time sync. Offset: {} us. Frames: {}...", frame,
                   offsetUs, framesToAdvance);
    }
  }

  // Handle the skipped frames
  if (framesToAdvance > 0)
  {
    // Only advance once every 5 frames in an attempt to make the speed up feel smoother
    if (frame % 5 != 0)
    {
      return false;
    }

    framesToAdvance = framesToAdvance - 1;
    return true;
  }

  isCurrentlyAdvancing = false;
  return false;
}

void CEXISlippi::handleSendInputs(s32 frame, u8 delay, s32 checksum_frame, u32 checksum, u8* inputs)
{
  if (isConnectionStalled)
    return;

  // On the first frame sent, we need to queue up empty dummy pads for as many
  // frames as we have delay
  if (frame == 1)
  {
    for (int i = 1; i <= delay; i++)
    {
      auto empty = std::make_unique<SlippiPad>(i);
      slippi_netplay->SendSlippiPad(std::move(empty));
    }
  }

  auto pad = std::make_unique<SlippiPad>(frame + delay, checksum_frame, checksum, inputs);

  slippi_netplay->SendSlippiPad(std::move(pad));
}

void CEXISlippi::prepareOpponentInputs(s32 frame, bool shouldSkip)
{
  m_read_queue.clear();

  u8 frameResult = 1;  // Indicates to continue frame

  auto state = slippi_netplay->GetSlippiConnectStatus();
  if (shouldSkip)
  {
    // Event though we are skipping an input, we still want to prepare the opponent inputs because
    // in the case where we get a stall on an advance frame, we need to keep the RXB inputs
    // populated for when the frame inputs are requested on a rollback
    frameResult = 2;
  }
  else if (state != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED ||
           isConnectionStalled)
  {
    frameResult = 3;  // Indicates we have disconnected
  }
  else if (shouldAdvanceOnlineFrame(frame))
  {
    frameResult = 4;
  }

  m_read_queue.push_back(frameResult);  // Write out the control message value

  u8 remotePlayerCount = matchmaking->RemotePlayerCount();
  m_read_queue.push_back(remotePlayerCount);  // Indicate the number of remote players

  std::unique_ptr<SlippiRemotePadOutput> results[SLIPPI_REMOTE_PLAYER_MAX];

  for (int i = 0; i < remotePlayerCount; i++)
  {
    results[i] = slippi_netplay->GetSlippiRemotePad(i, ROLLBACK_MAX_FRAMES);
    // results[i] = slippi_netplay->GetFakePadOutput(frame);

    // INFO_LOG(SLIPPI_ONLINE, "Sending checksum values: [%d] %08x", results[i]->checksumFrame,
    // results[i]->checksum);
    appendWordToBuffer(&m_read_queue, static_cast<u32>(results[i]->checksumFrame));
    appendWordToBuffer(&m_read_queue, results[i]->checksum);
  }
  for (int i = remotePlayerCount; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    // Send dummy data for unused players
    appendWordToBuffer(&m_read_queue, 0);
    appendWordToBuffer(&m_read_queue, 0);
  }

  int offset[SLIPPI_REMOTE_PLAYER_MAX];

  int32_t latestFrameRead[SLIPPI_REMOTE_PLAYER_MAX]{};

  // Get pad data for each remote player and write each of their latest frame nums to the buf
  for (int i = 0; i < remotePlayerCount; i++)
  {
    results[i] = slippi_netplay->GetSlippiRemotePad(i, ROLLBACK_MAX_FRAMES);
    // results[i] = slippi_netplay->GetFakePadOutput(frame);

    // determine offset from which to copy data
    offset[i] = (results[i]->latestFrame - frame) * SLIPPI_PAD_FULL_SIZE;
    offset[i] = offset[i] < 0 ? 0 : offset[i];

    // add latest frame we are transfering to begining of return buf
    int32_t latestFrame = results[i]->latestFrame;
    if (latestFrame > frame)
      latestFrame = frame;
    latestFrameRead[i] = latestFrame;
    appendWordToBuffer(&m_read_queue, static_cast<u32>(latestFrame));
    // INFO_LOG_FMT(SLIPPI_ONLINE, "Sending frame num {} for pIdx {} (offset: {})", latestFrame, i,
    // offset[i]);
  }
  // Send the current frame for any unused player slots.
  for (int i = remotePlayerCount; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    latestFrameRead[i] = frame;
    appendWordToBuffer(&m_read_queue, static_cast<u32>(frame));
  }

  s32* val = std::min_element(std::begin(latestFrameRead), std::end(latestFrameRead));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(*val));

  // copy pad data over
  for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    std::vector<u8> tx;

    // Get pad data if this remote player exists
    if (i < remotePlayerCount && offset[i] < results[i]->data.size())
    {
      auto txStart = results[i]->data.begin() + offset[i];
      auto txEnd = results[i]->data.end();
      tx.insert(tx.end(), txStart, txEnd);
    }

    tx.resize(SLIPPI_PAD_FULL_SIZE * ROLLBACK_MAX_FRAMES, 0);

    m_read_queue.insert(m_read_queue.end(), tx.begin(), tx.end());
  }

  // ERROR_LOG_FMT(SLIPPI_ONLINE, "EXI: [{}] %X %X %X %X %X %X %X %X", latestFrame, m_read_queue[5],
  // m_read_queue[6], m_read_queue[7], m_read_queue[8], m_read_queue[9], m_read_queue[10],
  // m_read_queue[11], m_read_queue[12]);
}

void CEXISlippi::handleCaptureSavestate(u8* payload)
{
#ifndef IS_PLAYBACK
  if (isDisconnected())
    return;
#endif

  s32 frame = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];

  // Grab an available savestate
  std::unique_ptr<SlippiSavestate> ss;
  if (!availableSavestates.empty())
  {
    ss = std::move(availableSavestates.back());
    availableSavestates.pop_back();
  }
  else
  {
    // If there were no available savestates, use the oldest one
    auto it = activeSavestates.begin();
    ss = std::move(it->second);
    activeSavestates.erase(it->first);
  }

  // If there is already a savestate for this frame, remove it and add it to available
  if (activeSavestates.count(frame))
  {
    availableSavestates.push_back(std::move(activeSavestates[frame]));
    activeSavestates.erase(frame);
  }

  ss->Capture();
  activeSavestates[frame] = std::move(ss);

  // u32 timeDiff = (u32)(Common::Timer::NowUs() - startTime);
  // INFO_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Captured savestate for frame {} in: %f ms", frame,
  //          ((double)timeDiff) / 1000);
}

void CEXISlippi::handleLoadSavestate(u8* payload)
{
  s32 frame = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
  u32* preserveArr = (u32*)(&payload[4]);

  if (!activeSavestates.count(frame))
  {
    // This savestate does not exist... uhhh? What do we do?
    ERROR_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Savestate for frame {} does not exist.", frame);
    return;
  }

  // Fetch preservation blocks
  std::vector<SlippiSavestate::PreserveBlock> blocks;

  // Get preservation blocks
  int idx = 0;
  while (Common::swap32(preserveArr[idx]) != 0)
  {
    SlippiSavestate::PreserveBlock p = {Common::swap32(preserveArr[idx]),
                                        Common::swap32(preserveArr[idx + 1])};
    blocks.push_back(p);
    idx += 2;
  }

  // Load savestate
  activeSavestates[frame]->Load(blocks);

  // Move all active savestates to available
  for (auto it = activeSavestates.begin(); it != activeSavestates.end(); ++it)
  {
    availableSavestates.push_back(std::move(it->second));
  }

  activeSavestates.clear();

  // u32 timeDiff = (u32)(Common::Timer::NowUs() - startTime);
  // INFO_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Loaded savestate for frame {} in: %f ms", frame,
  //          ((double)timeDiff) / 1000);
}

void CEXISlippi::startFindMatch(u8* payload)
{
  SlippiMatchmaking::MatchSearchSettings search;
  search.mode = (SlippiMatchmaking::OnlinePlayMode)payload[0];

  std::string shiftJisCode;
  shiftJisCode.insert(shiftJisCode.begin(), &payload[1], &payload[1] + 18);
  shiftJisCode.erase(std::find(shiftJisCode.begin(), shiftJisCode.end(), 0x00), shiftJisCode.end());

  // Log the direct code to file.
  if (search.mode == SlippiMatchmaking::DIRECT)
  {
    // Make sure to convert to UTF8, otherwise json library will fail when
    // calling dump().
    std::string utf8Code = SHIFTJISToUTF8(shiftJisCode);
    directCodes->AddOrUpdateCode(utf8Code);
  }
  else if (search.mode == SlippiMatchmaking::TEAMS)
  {
    std::string utf8Code = SHIFTJISToUTF8(shiftJisCode);
    teamsCodes->AddOrUpdateCode(utf8Code);
  }

  // TODO: Make this work so we dont have to pass shiftJis to mm server
  // search.connectCode = SHIFTJISToUTF8(shiftJisCode).c_str();
  search.connectCode = shiftJisCode;

  // Store this search so we know what was queued for
  lastSearch = search;

  // While we do have another condition that checks characters after being connected, it's nice to
  // give someone an early error before they even queue so that they wont enter the queue and make
  // someone else get force removed from queue and have to requeue
  if (SlippiMatchmaking::IsFixedRulesMode(search.mode))
  {
    // Character check
    if (localSelections.characterId >= 26)
    {
      forcedError = "The character you selected is not allowed in this mode";
      return;
    }

    // Stage check
    if (localSelections.isStageSelected &&
        std::find(allowedStages.begin(), allowedStages.end(), localSelections.stageId) ==
            allowedStages.end())
    {
      forcedError = "The stage being requested is not allowed in this mode";
      return;
    }
  }
  else if (search.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS)
  {
    // Some special handling for teams since it is being heavily used for unranked
    if (localSelections.characterId >= 26 &&
        SConfig::GetSlippiConfig().melee_version != Melee::Version::MEX)
    {
      forcedError = "The character you selected is not allowed in this mode";
      return;
    }
  }

#ifndef LOCAL_TESTING
  if (!isEnetInitialized)
  {
    // Initialize enet
    auto res = enet_initialize();
    if (res < 0)
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Failed to initialize enet res: {}", res);

    isEnetInitialized = true;
  }

  matchmaking->FindMatch(search);
#endif
}

bool CEXISlippi::doesTagMatchInput(u8* input, u8 inputLen, std::string tag)
{
  auto jisTag = UTF8ToSHIFTJIS(tag);

  // Check if this tag matches what has been input so far
  bool isMatch = true;
  for (int i = 0; i < inputLen; i++)
  {
    // ERROR_LOG_FMT(SLIPPI_ONLINE, "Entered: %X%X. History: %X%X", input[i * 3], input[i * 3 + 1],
    // (u8)jisTag[i * 2], (u8)jisTag[i * 2 + 1]);
    if (input[i * 3] != (u8)jisTag[i * 2] || input[i * 3 + 1] != (u8)jisTag[i * 2 + 1])
    {
      isMatch = false;
      break;
    }
  }

  return isMatch;
}

void CEXISlippi::handleNameEntryLoad(u8* payload)
{
  u8 inputLen = payload[24];
  u32 initialIndex = payload[25] << 24 | payload[26] << 16 | payload[27] << 8 | payload[28];
  u8 scrollDirection = payload[29];
  u8 curMode = payload[30];

  auto codeHistory = directCodes.get();
  if (curMode == SlippiMatchmaking::TEAMS)
  {
    codeHistory = teamsCodes.get();
  }

  // Adjust index
  u32 curIndex = initialIndex;
  if (scrollDirection == 1)
  {
    curIndex++;
  }
  else if (scrollDirection == 2)
  {
    curIndex = curIndex > 0 ? curIndex - 1 : curIndex;
  }
  else if (scrollDirection == 3)
  {
    curIndex = 0;
  }

  // Scroll to next tag that
  std::string tagAtIndex = "1";
  while (curIndex >= 0 && curIndex < static_cast<u32>(codeHistory->length()))
  {
    tagAtIndex = codeHistory->get(curIndex);

    // Break if we have found a tag that matches
    if (doesTagMatchInput(payload, inputLen, tagAtIndex))
      break;

    curIndex = scrollDirection == 2 ? curIndex - 1 : curIndex + 1;
  }

  // INFO_LOG_FMT(SLIPPI_ONLINE, "Idx: {}, InitIdx: {}, Scroll: {}. Len: {}", curIndex,
  // initialIndex,
  //          scrollDirection, inputLen);

  tagAtIndex = codeHistory->get(curIndex);
  if (tagAtIndex == "1")
  {
    // If we failed to find a tag at the current index, try the initial index again.
    // If the initial index matches the filter, preserve that suggestion. Without
    // this logic, the suggestion would get cleared
    auto initialTag = codeHistory->get(initialIndex);
    if (doesTagMatchInput(payload, inputLen, initialTag))
    {
      tagAtIndex = initialTag;
      curIndex = initialIndex;
    }
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "Retrieved tag: {}", tagAtIndex.c_str());
  std::string jisCode;
  m_read_queue.clear();

  if (tagAtIndex == "1")
  {
    m_read_queue.push_back(0);
    m_read_queue.insert(m_read_queue.end(), payload, payload + 3 * inputLen);
    m_read_queue.insert(m_read_queue.end(), 3 * (8 - inputLen), 0);
    m_read_queue.push_back(inputLen);
    appendWordToBuffer(&m_read_queue, initialIndex);
    return;
  }

  // Indicate we have a suggestion
  m_read_queue.push_back(1);

  // Convert to tag to shift jis and write to response
  jisCode = UTF8ToSHIFTJIS(tagAtIndex);

  // Write out connect code into buffer, injection null terminator after each letter
  for (int i = 0; i < 8; i++)
  {
    for (int j = i * 2; j < i * 2 + 2; j++)
    {
      m_read_queue.push_back(j < jisCode.length() ? jisCode[j] : 0);
    }

    m_read_queue.push_back(0x0);
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "New Idx: {}. Jis Code length: {}", curIndex,
               static_cast<u8>(jisCode.length() / 2));

  // Write length of tag
  m_read_queue.push_back(static_cast<u8>(jisCode.length() / 2));
  appendWordToBuffer(&m_read_queue, curIndex);
}

// teamId 0 = red, 1 = blue, 2 = green
int CEXISlippi::getCharColor(u8 charId, u8 teamId)
{
  switch (charId)
  {
  case 0x0:  // Falcon
    switch (teamId)
    {
    case 0:
      return 2;
    case 1:
      return 5;
    case 2:
      return 4;
    }
  case 0x2:  // Fox
    switch (teamId)
    {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 3;
    }
  case 0xC:  // Peach
    switch (teamId)
    {
    case 0:
      return 0;
    case 1:
      return 3;
    case 2:
      return 4;
    }
  case 0x13:  // Sheik
    switch (teamId)
    {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 3;
    }
  case 0x14:  // Falco
    switch (teamId)
    {
    case 0:
      return 1;
    case 1:
      return 2;
    case 2:
      return 3;
    }
  }
  return 0;
}

void CEXISlippi::prepareOnlineMatchState()
{
  // This match block is a VS match with P1 Red Falco vs P2 Red Bowser vs P3 Young Link vs P4 Young
  // Link on Battlefield. The proper values will be overwritten
  static std::vector<u8> onlineMatchBlock = {
      0x32, 0x01, 0x86, 0x4C, 0xC3, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x6E, 0x00,
      0x1F, 0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x3F,
      0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x09,
      0x00, 0x78, 0x00, 0xC0, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x05, 0x00, 0x04,
      0x01, 0x00, 0x01, 0x00, 0x00, 0x09, 0x00, 0x78, 0x00, 0xC0, 0x00, 0x04, 0x01, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F,
      0x80, 0x00, 0x00, 0x15, 0x03, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x09, 0x00, 0x78, 0x00,
      0xC0, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00,
      0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x15, 0x03, 0x04, 0x00, 0x00, 0xFF,
      0x00, 0x00, 0x09, 0x00, 0x78, 0x00, 0xC0, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00,
      0x21, 0x03, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x09, 0x00, 0x78, 0x00, 0x40, 0x00, 0x04,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80,
      0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x21, 0x03, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x09,
      0x00, 0x78, 0x00, 0x40, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00,
  };

  m_read_queue.clear();

  auto errorState = SlippiMatchmaking::ProcessState::ERROR_ENCOUNTERED;

  SlippiMatchmaking::ProcessState mmState =
      !forcedError.empty() ? errorState : matchmaking->GetMatchmakeState();

#ifdef LOCAL_TESTING
  if (localSelections.isCharacterSelected || isLocalConnected)
  {
    mmState = SlippiMatchmaking::ProcessState::CONNECTION_SUCCESS;
    isLocalConnected = true;
  }
#endif

  m_read_queue.push_back(mmState);  // Matchmaking State

  u8 localPlayerReady = localSelections.isCharacterSelected;
  u8 remotePlayersReady = 0;

  auto userInfo = user->GetUserInfo();

  if (mmState == SlippiMatchmaking::ProcessState::CONNECTION_SUCCESS)
  {
    m_local_player_index = matchmaking->LocalPlayerIndex();

    if (!slippi_netplay)
    {
#ifdef LOCAL_TESTING
      slippi_netplay = std::make_unique<SlippiNetplayClient>(true);
#else
      slippi_netplay = matchmaking->GetNetplayClient();
#endif

      // This happens on the initial connection to a player. The matchmaking object is ephemeral, it
      // gets re-created when a connection is terminated, that said, it can still be useful to know
      // who we were connected to after they disconnect from us, for example in the case of
      // reporting a match. So let's copy the results.
      allowedStages = recentMmResult.stages;
      // Clear stage pool so that when we call getRandomStage it will use full list
      stagePool.clear();
      localSelections.stageId = getRandomStage();
      slippi_netplay->SetMatchSelections(localSelections);
    }

#ifdef LOCAL_TESTING
    bool isConnected = true;
#else
    auto status = slippi_netplay->GetSlippiConnectStatus();
    bool isConnected =
        status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
#endif

    if (isConnected)
    {
      auto matchInfo = slippi_netplay->GetMatchInfo();
#ifdef LOCAL_TESTING
      remotePlayersReady = true;
#else
      remotePlayersReady = 1;
      u8 remotePlayerCount = matchmaking->RemotePlayerCount();
      for (int i = 0; i < remotePlayerCount; i++)
      {
        if (!matchInfo->remotePlayerSelections[i].isCharacterSelected)
        {
          remotePlayersReady = 0;
        }
      }

      if (remotePlayerCount == 1)
      {
        auto isDecider = slippi_netplay->IsDecider();
        m_local_player_index = isDecider ? 0 : 1;
        m_remote_player_index = isDecider ? 1 : 0;
      }
#endif

      auto isDecider = slippi_netplay->IsDecider();
      m_local_player_index = isDecider ? 0 : 1;
      m_remote_player_index = isDecider ? 1 : 0;
    }
    else
    {
#ifndef LOCAL_TESTING
      // If we get here, our opponent likely disconnected. Let's trigger a clean up
      handleConnectionCleanup();
      prepareOnlineMatchState();  // run again with new state
      return;
#endif
    }
    // Here we are connected, check to see if we should init play session
    if (!is_play_session_active)
    {
      game_reporter->StartNewSession();
      is_play_session_active = true;
    }
  }
  else
  {
    slippi_netplay = nullptr;
  }

  u32 rngOffset = 0;
  std::string localPlayerName = "";
  std::string oppName = "";
  std::string p1Name = "";
  std::string p2Name = "";
  u8 chatMessageId = 0;
  u8 chatMessagePlayerIdx = 0;
  u8 sentChatMessageId = 0;

#ifdef LOCAL_TESTING
  localPlayerIndex = 0;
  sentChatMessageId = localChatMessageId;
  chatMessagePlayerIdx = 0;
  localChatMessageId = 0;
  // in CSS p1 is always current player and p2 is opponent
  localPlayerName = p1Name = "Player 1";
  oppName = p2Name = "Player 2";
#endif

  m_read_queue.push_back(localPlayerReady);       // Local player ready
  m_read_queue.push_back(remotePlayersReady);     // Remote players ready
  m_read_queue.push_back(m_local_player_index);   // Local player index
  m_read_queue.push_back(m_remote_player_index);  // Remote player index

  // Set chat message if any
  if (slippi_netplay)
  {
    auto isSingleMode = matchmaking && matchmaking->RemotePlayerCount() == 1;
    bool isChatEnabled = isSlippiChatEnabled();
    sentChatMessageId = slippi_netplay->GetSlippiRemoteSentChatMessage(isChatEnabled);
    // Prevent processing a message in the same frame
    if (sentChatMessageId <= 0)
    {
      auto remoteMessageSelection = slippi_netplay->GetSlippiRemoteChatMessage(isChatEnabled);
      chatMessageId = remoteMessageSelection.messageId;
      chatMessagePlayerIdx = remoteMessageSelection.playerIdx;
      if (chatMessageId == SlippiPremadeText::CHAT_MSG_CHAT_DISABLED && !isSingleMode)
      {
        // Clear remote chat messages if we are on teams and the player has chat disabled.
        // Could also be handled on SlippiNetplay if the instance had acccess to the current
        // connection mode
        chatMessageId = chatMessagePlayerIdx = 0;
      }
    }
    else
    {
      chatMessagePlayerIdx = m_local_player_index;
    }

    if (isSingleMode || !matchmaking)
    {
      chatMessagePlayerIdx = sentChatMessageId > 0 ? m_local_player_index : m_remote_player_index;
    }
    // in CSS p1 is always current player and p2 is opponent
    localPlayerName = p1Name = userInfo.display_name;
  }

  std::vector<u8> leftTeamPlayers = {};
  std::vector<u8> rightTeamPlayers = {};

  if (localPlayerReady && remotePlayersReady)
  {
    auto isDecider = slippi_netplay->IsDecider();
    u8 remotePlayerCount = matchmaking->RemotePlayerCount();
    auto matchInfo = slippi_netplay->GetMatchInfo();
    SlippiPlayerSelections lps = matchInfo->localPlayerSelections;
    auto rps = matchInfo->remotePlayerSelections;

#ifdef LOCAL_TESTING
    lps.playerIdx = 0;

    // By default Local testing for teams is against
    // 1 RED TEAM Falco
    // 2 BLUE TEAM Falco
    for (int i = 0; i <= SLIPPI_REMOTE_PLAYER_MAX; i++)
    {
      if (i == 0)
      {
        rps[i].characterColor = 1;
        rps[i].teamId = 0;
      }
      else
      {
        rps[i].characterColor = 2;
        rps[i].teamId = 1;
      }

      rps[i].characterId = 0x14;
      rps[i].playerIdx = i + 1;
      rps[i].isCharacterSelected = true;
    }

    remotePlayerCount = lastSearch.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS ? 3 : 1;

    oppName = std::string("Player");
#endif

    // Check if someone is picking dumb characters in non-direct
    auto localCharOk = lps.characterId < 26;
    auto remoteCharOk = true;
    INFO_LOG_FMT(SLIPPI_ONLINE, "remotePlayerCount: {}", remotePlayerCount);
    for (int i = 0; i < remotePlayerCount; i++)
    {
      if (rps[i].characterId >= 26)
        remoteCharOk = false;
    }

    // TODO: Ideally remotePlayerSelections would just include everyone including the local player
    // the local player
    // TODO: Would also simplify some logic in the Netplay class
    // Here we are storing pointers to the player selections. That means that we can technically
    // modify the values from here, which is probably not the cleanest thing since they're coming
    // from the netplay class. Unfortunately, I think it might be required for the overwrite stuff
    // to work correctly though, maybe on a tiebreak in ranked?
    std::vector<SlippiPlayerSelections*> orderedSelections(remotePlayerCount + 1);
    orderedSelections[lps.playerIdx] = &lps;
    for (int i = 0; i < remotePlayerCount; i++)
    {
      orderedSelections[rps[i].playerIdx] = &rps[i];
    }

    // Overwrite selections
    for (int i = 0; i < overwrite_selections.size(); i++)
    {
      const auto& ow = overwrite_selections[i];

      orderedSelections[i]->characterId = ow.characterId;
      orderedSelections[i]->characterColor = ow.characterColor;
      orderedSelections[i]->stageId = ow.stageId;
    }

    // Overwrite stage information. Make sure everyone loads the same stage
    u16 stageId = 0x1F;  // Default to battlefield if there was no selection
    for (const auto& selections : orderedSelections)
    {
      if (!selections->isStageSelected)
        continue;

      // Stage selected by this player, use that selection
      stageId = selections->stageId;
      break;
    }

    if (SlippiMatchmaking::IsFixedRulesMode(lastSearch.mode))
    {
      // If we enter one of these conditions, someone is doing something bad, clear the lobby
      if (!localCharOk)
      {
        handleConnectionCleanup();
        forcedError = "The character you selected is not allowed in this mode";
        prepareOnlineMatchState();
        return;
      }

      if (!remoteCharOk)
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }

      if (std::find(allowedStages.begin(), allowedStages.end(), stageId) == allowedStages.end())
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }
    }
    else if (lastSearch.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS)
    {
      auto isMEX = SConfig::GetSlippiConfig().melee_version == Melee::Version::MEX;

      if (!localCharOk && !isMEX)
      {
        handleConnectionCleanup();
        forcedError = "The character you selected is not allowed in this mode";
        prepareOnlineMatchState();
        return;
      }

      if (!remoteCharOk && !isMEX)
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }
    }

    // Set rng offset
    rngOffset = isDecider ? lps.rngOffset : rps[0].rngOffset;
    INFO_LOG_FMT(SLIPPI_ONLINE, "Rng Offset: {:#x}", rngOffset);

    // Check if everyone is the same color
    auto color = orderedSelections[0]->teamId;
    bool areAllSameTeam = true;
    for (const auto& s : orderedSelections)
    {
      if (s->teamId != color)
        areAllSameTeam = false;
    }

    // Choose random team assignments
    // Previously there was a bug here where the shuffle was not consistent across platforms given
    // the same seed, this would cause desyncs during cross platform play (different teams). Got
    // around this by no longer using the shuffle function...
    std::vector<std::vector<u8>> teamAssignmentPermutations = {
        {0, 0, 1, 1}, {1, 1, 0, 0}, {0, 1, 1, 0}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 0, 1, 0},
    };
    auto teamAssignments =
        teamAssignmentPermutations[rngOffset % teamAssignmentPermutations.size()];

    // Overwrite player character choices
    for (auto& s : orderedSelections)
    {
      if (!s->isCharacterSelected)
      {
        continue;
      }
      if (areAllSameTeam)
      {
        // Overwrite teamId. Color is overwritten by ASM
        s->teamId = teamAssignments[s->playerIdx];
      }

      // Overwrite player character
      onlineMatchBlock[0x60 + (s->playerIdx) * 0x24] = s->characterId;
      onlineMatchBlock[0x63 + (s->playerIdx) * 0x24] = s->characterColor;
      onlineMatchBlock[0x67 + (s->playerIdx) * 0x24] = 0;
      onlineMatchBlock[0x69 + (s->playerIdx) * 0x24] = s->teamId;
    }

    // Handle Singles/Teams specific logic
    if (remotePlayerCount < 3)
    {
      onlineMatchBlock[0x8] = 0;  // is Teams = false

      // Set p3/p4 player type to none
      onlineMatchBlock[0x61 + 2 * 0x24] = 3;
      onlineMatchBlock[0x61 + 3 * 0x24] = 3;

      // Make one character lighter if same character, same color
      bool isSheikVsZelda = lps.characterId == 0x12 && rps[0].characterId == 0x13 ||
                            lps.characterId == 0x13 && rps[0].characterId == 0x12;
      bool charMatch = lps.characterId == rps[0].characterId || isSheikVsZelda;
      bool colMatch = lps.characterColor == rps[0].characterColor;

      onlineMatchBlock[0x67 + 0x24] = charMatch && colMatch ? 1 : 0;
    }
    else
    {
      onlineMatchBlock[0x8] = 1;  // is Teams = true

      // Set p3/p4 player type to human
      onlineMatchBlock[0x61 + 2 * 0x24] = 0;
      onlineMatchBlock[0x61 + 3 * 0x24] = 0;
    }

    u16* stage = (u16*)&onlineMatchBlock[0xE];
    *stage = Common::swap16(stageId);

    // Turn pause off in unranked/ranked, on in other modes
    auto pauseAllowed = lastSearch.mode == SlippiMatchmaking::OnlinePlayMode::DIRECT;
    u8* gameBitField3 = static_cast<u8*>(&onlineMatchBlock[2]);
    *gameBitField3 = pauseAllowed ? *gameBitField3 & 0xF7 : *gameBitField3 | 0x8;
    //*gameBitField3 = *gameBitField3 | 0x8;

    // Group players into left/right side for team splash screen display
    for (int i = 0; i < 4; i++)
    {
      int teamId = onlineMatchBlock[0x69 + i * 0x24];
      if (teamId == lps.teamId)
        leftTeamPlayers.push_back(i);
      else
        rightTeamPlayers.push_back(i);
    }
    int leftTeamSize = static_cast<int>(leftTeamPlayers.size());
    int rightTeamSize = static_cast<int>(rightTeamPlayers.size());
    leftTeamPlayers.resize(4, 0);
    rightTeamPlayers.resize(4, 0);
    leftTeamPlayers[3] = static_cast<u8>(leftTeamSize);
    rightTeamPlayers[3] = static_cast<u8>(rightTeamSize);
  }

  // Add rng offset to output
  appendWordToBuffer(&m_read_queue, rngOffset);

  // Add delay frames to output
  m_read_queue.push_back(static_cast<u8>(Config::Get(Config::SLIPPI_ONLINE_DELAY)));

  // Add chat messages id
  m_read_queue.push_back(static_cast<u8>(sentChatMessageId));
  m_read_queue.push_back(static_cast<u8>(chatMessageId));
  m_read_queue.push_back(static_cast<u8>(chatMessagePlayerIdx));

  // Add player groupings for VS splash screen
  leftTeamPlayers.resize(4, 0);
  rightTeamPlayers.resize(4, 0);
  m_read_queue.insert(m_read_queue.end(), leftTeamPlayers.begin(), leftTeamPlayers.end());
  m_read_queue.insert(m_read_queue.end(), rightTeamPlayers.begin(), rightTeamPlayers.end());

  // Add names to output
  // Always send static local player name
  localPlayerName = ConvertStringForGame(localPlayerName, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), localPlayerName.begin(), localPlayerName.end());

#ifdef LOCAL_TESTING
  std::string defaultNames[] = {"Player 1", "Player 2", "Player 3", "Player 4"};
#endif

  for (int i = 0; i < 4; i++)
  {
    std::string name = matchmaking->GetPlayerName(i);
#ifdef LOCAL_TESTING
    name = defaultNames[i];
#endif
    name = ConvertStringForGame(name, MAX_NAME_LENGTH);
    m_read_queue.insert(m_read_queue.end(), name.begin(), name.end());
  }

  // Create the opponent string using the names of all players on opposing teams
  std::vector<std::string> opponentNames = {};
  if (matchmaking->RemotePlayerCount() == 1)
  {
    opponentNames.push_back(matchmaking->GetPlayerName(remotePlayerIndex));
  }
  else
  {
    int teamIdx = onlineMatchBlock[0x69 + localPlayerIndex * 0x24];
    for (int i = 0; i < 4; i++)
    {
      if (localPlayerIndex == i || onlineMatchBlock[0x69 + i * 0x24] == teamIdx)
        continue;

      opponentNames.push_back(matchmaking->GetPlayerName(i));
    }
  }

  auto numOpponents = opponentNames.size() == 0 ? 1 : opponentNames.size();
  auto charsPerName = (MAX_NAME_LENGTH - (numOpponents - 1)) / numOpponents;
  std::string oppText = "";
  for (auto& name : opponentNames)
  {
    if (oppText != "")
      oppText += "/";

    oppText += TruncateLengthChar(name, charsPerName);
  }

  oppName = ConvertStringForGame(oppText, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), oppName.begin(), oppName.end());

#ifdef LOCAL_TESTING
  std::string defaultConnectCodes[] = {"PLYR#001", "PLYR#002", "PLYR#003", "PLYR#004"};
#endif

  auto playerInfo = matchmaking->GetPlayerInfo();
  for (int i = 0; i < 4; i++)
  {
    std::string connectCode = i < playerInfo.size() ? playerInfo[i].connect_code : "";
#ifdef LOCAL_TESTING
    connectCode = defaultConnectCodes[i];
#endif
    connectCode = ConvertConnectCodeForGame(connectCode);
    m_read_queue.insert(m_read_queue.end(), connectCode.begin(), connectCode.end());
  }

#ifdef LOCAL_TESTING
  std::string defaultUids[] = {"l6dqv4dp38a5ho6z1sue2wx2adlp", "jpvducykgbawuehrjlfbu2qud1nv",
                               "k0336d0tg3mgcdtaukpkf9jtf2k8", "v8tpb6uj9xil6e33od6mlot4fvdt"};
#endif

  for (int i = 0; i < 4; i++)
  {
    std::string uid = i < playerInfo.size() ? playerInfo[i].uid :
                                              "";  // UIDs are 28 characters + 1 null terminator
#ifdef LOCAL_TESTING
    uid = defaultUids[i];
#endif
    uid.resize(29);  // ensure a null terminator at the end
    m_read_queue.insert(m_read_queue.end(), uid.begin(), uid.end());
  }

  // Add error message if there is one
  auto errorStr = !forcedError.empty() ? forcedError : matchmaking->GetErrorMessage();
  errorStr = ConvertStringForGame(errorStr, 120);
  m_read_queue.insert(m_read_queue.end(), errorStr.begin(), errorStr.end());

  // Add the match struct block to output
  m_read_queue.insert(m_read_queue.end(), onlineMatchBlock.begin(), onlineMatchBlock.end());
}

u16 CEXISlippi::getRandomStage()
{
  static u16 selectedStage;

  // Reset stage pool if it's empty
  if (stagePool.empty())
    stagePool.insert(stagePool.end(), allowedStages.begin(), allowedStages.end());

  // Get random stage
  int randIndex = generator() % stagePool.size();
  selectedStage = stagePool[randIndex];

  // Remove last selection from stage pool
  stagePool.erase(stagePool.begin() + randIndex);

  return selectedStage;
}

void CEXISlippi::setMatchSelections(u8* payload)
{
  SlippiPlayerSelections s;

  s.teamId = payload[0];
  s.characterId = payload[1];
  s.characterColor = payload[2];
  s.isCharacterSelected = payload[3];

  s.stageId = Common::swap16(&payload[4]);
  u8 stageSelectOption = payload[6];
  // u8 onlineMode = payload[7];

  s.isStageSelected = stageSelectOption == 1 || stageSelectOption == 3;
  if (stageSelectOption == 3)
  {
    // If stage requested is random, select a random stage
    s.stageId = getRandomStage();
  }

  INFO_LOG_FMT(SLIPPI, "LPS set char: {}, iSS: {}, {}, stage: {}, team: {}", s.isCharacterSelected,
               stageSelectOption, s.isStageSelected, s.stageId, s.teamId);

  s.rngOffset = generator() % 0xFFFF;

  // Merge these selections
  localSelections.Merge(s);

  if (slippi_netplay)
  {
    slippi_netplay->SetMatchSelections(localSelections);
  }
}

void CEXISlippi::prepareFileLength(u8* payload)
{
  m_read_queue.clear();

  std::string fileName((char*)&payload[0]);

  std::string contents;
  u32 size = gameFileLoader->LoadFile(fileName, contents);

  INFO_LOG_FMT(SLIPPI, "Getting file size for: {} -> {}", fileName.c_str(), size);

  // Write size to output
  appendWordToBuffer(&m_read_queue, size);
}

void CEXISlippi::prepareFileLoad(u8* payload)
{
  m_read_queue.clear();

  std::string fileName((char*)&payload[0]);

  std::string contents;
  u32 size = gameFileLoader->LoadFile(fileName, contents);
  std::vector<u8> buf(contents.begin(), contents.end());

  INFO_LOG_FMT(SLIPPI, "Writing file contents: {} -> {}", fileName.c_str(), size);

  // Write the contents to output
  m_read_queue.insert(m_read_queue.end(), buf.begin(), buf.end());
}

void CEXISlippi::prepareGctLength()
{
  m_read_queue.clear();

  u32 size = Gecko::GetGctLength();

  INFO_LOG_FMT(SLIPPI, "Getting gct size: {}", size);

  // Write size to output
  appendWordToBuffer(&m_read_queue, size);
}

void CEXISlippi::prepareGctLoad(u8* payload)
{
  m_read_queue.clear();

  auto gct = Gecko::GenerateGct();

  // This is the address where the codes will be written to
  auto address = Common::swap32(&payload[0]);

  INFO_LOG_FMT(SLIPPI, "Preparing to write gecko codes at: {:#x}", address);

  m_read_queue.insert(m_read_queue.end(), gct.begin(), gct.end());
}

std::vector<u8> CEXISlippi::loadPremadeText(u8* payload)
{
  u8 textId = payload[0];
  std::vector<u8> premadeTextData;
  auto spt = SlippiPremadeText();

  if (textId >= SlippiPremadeText::SPT_CHAT_P1 && textId <= SlippiPremadeText::SPT_CHAT_P4)
  {
    auto port = textId - 1;
    std::string playerName;
    if (matchmaking)
      playerName = matchmaking->GetPlayerName(port);
#ifdef LOCAL_TESTING
    std::string defaultNames[] = {"Player 1", "lol u lost 2 dk", "Player 3", "Player 4"};
    playerName = defaultNames[port];
#endif

    u8 paramId = payload[1];

    for (auto it = spt.unsupportedStringMap.begin(); it != spt.unsupportedStringMap.end(); it++)
    {
      playerName = ReplaceAll(playerName.c_str(), it->second, "");  // Remove unsupported chars
      playerName = ReplaceAll(playerName.c_str(), it->first,
                              it->second);  // Remap delimiters for premade text
    }

    // Replaces spaces with premade text space
    playerName = ReplaceAll(playerName.c_str(), " ", "<S>");

    if (paramId == SlippiPremadeText::CHAT_MSG_CHAT_DISABLED)
    {
      return premadeTextData =
                 spt.GetPremadeTextData(SlippiPremadeText::SPT_CHAT_DISABLED, playerName.c_str());
    }
    auto chatMessage = spt.premadeTextsParams.at(paramId);
    std::string param = ReplaceAll(chatMessage.c_str(), " ", "<S>");
    premadeTextData = spt.GetPremadeTextData(textId, playerName.c_str(), param.c_str());
  }
  else
  {
    premadeTextData = spt.GetPremadeTextData(textId);
  }

  return premadeTextData;
}

void CEXISlippi::preparePremadeTextLength(u8* payload)
{
  std::vector<u8> premadeTextData = loadPremadeText(payload);

  m_read_queue.clear();
  // Write size to output
  appendWordToBuffer(&m_read_queue, static_cast<u32>(premadeTextData.size()));
}

void CEXISlippi::preparePremadeTextLoad(u8* payload)
{
  std::vector<u8> premadeTextData = loadPremadeText(payload);

  m_read_queue.clear();
  // Write data to output
  m_read_queue.insert(m_read_queue.end(), premadeTextData.begin(), premadeTextData.end());
}

bool CEXISlippi::isSlippiChatEnabled()
{
  auto chatEnabledChoice = Config::Get(Config::SLIPPI_ENABLE_QUICK_CHAT);
  bool res = true;
  switch (lastSearch.mode)
  {
  case SlippiMatchmaking::DIRECT:
    res = chatEnabledChoice == Slippi::Chat::ON || chatEnabledChoice == Slippi::Chat::DIRECT_ONLY;
    break;
  default:
    res = chatEnabledChoice == Slippi::Chat::ON;
    break;
  }
  return res;  // default is enabled
}

void CEXISlippi::handleChatMessage(u8* payload)
{
  if (!isSlippiChatEnabled())
    return;

  int messageId = payload[0];
  INFO_LOG_FMT(SLIPPI, "SLIPPI CHAT INPUT: {:#x}", messageId);

#ifdef LOCAL_TESTING
  localChatMessageId = 11;
#endif

  if (slippi_netplay)
  {
    auto userInfo = user->GetUserInfo();
    auto packet = std::make_unique<sf::Packet>();
    //		OSD::AddMessage("[Me]: "+ msg, OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
    slippi_netplay->remoteSentChatMessageId = messageId;
    slippi_netplay->WriteChatMessageToPacket(*packet, messageId, slippi_netplay->LocalPlayerPort());
    slippi_netplay->SendAsync(std::move(packet));
  }
}

void CEXISlippi::logMessageFromGame(u8* payload)
{
  // The first byte indicates whether to log the time or not
  if (payload[0] == 0)
  {
    GENERIC_LOG_FMT(Common::Log::LogType::SLIPPI, (Common::Log::LogLevel)payload[1], "{}",
                    (char*)&payload[2]);
  }
  else
  {
    GENERIC_LOG_FMT(Common::Log::LogType::SLIPPI, (Common::Log::LogLevel)payload[1], "{}: {}",
                    (char*)&payload[2], Common::Timer::NowUs());
  }
}

void CEXISlippi::handleLogInRequest()
{
  bool logInRes = user->AttemptLogin();
  if (!logInRes)
  {
    if (Host_RendererIsFullscreen())
      Host_Fullscreen();
    Host_LowerWindow();
    user->OpenLogInPage();
    user->ListenForLogIn();
  }
}

void CEXISlippi::handleLogOutRequest()
{
  user->LogOut();
}

void CEXISlippi::prepareOnlineStatus()
{
  m_read_queue.clear();

  auto isLoggedIn = user->IsLoggedIn();
  auto userInfo = user->GetUserInfo();

  u8 appState = 0;
  if (isLoggedIn)
  {
    // Check if we have the latest version, and if not, indicate we need to update
    version::Semver200_version latestVersion(userInfo.latest_version);
    version::Semver200_version currentVersion(Common::GetSemVerStr());

    appState = latestVersion > currentVersion ? 2 : 1;
  }

  m_read_queue.push_back(appState);

  // Write player name (31 bytes)
  std::string playerName = ConvertStringForGame(userInfo.display_name, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), playerName.begin(), playerName.end());

  // Write connect code (10 bytes)
  std::string connectCode = userInfo.connect_code;
  char shiftJisHashtag[] = {'\x81', '\x94', '\x00'};
  connectCode.resize(CONNECT_CODE_LENGTH);
  connectCode = ReplaceAll(connectCode, "#", shiftJisHashtag);
  auto codeBuf = connectCode.c_str();
  m_read_queue.insert(m_read_queue.end(), codeBuf, codeBuf + CONNECT_CODE_LENGTH + 2);
}

void doConnectionCleanup(std::unique_ptr<SlippiMatchmaking> mm,
                         std::unique_ptr<SlippiNetplayClient> nc)
{
  if (mm)
    mm.reset();

  if (nc)
    nc.reset();
}

void CEXISlippi::handleConnectionCleanup()
{
  ERROR_LOG_FMT(SLIPPI_ONLINE, "Connection cleanup started...");

  // Handle destructors in a separate thread to not block the main thread
  std::thread cleanup(doConnectionCleanup, std::move(matchmaking), std::move(slippi_netplay));
  cleanup.detach();

  // Reset matchmaking
  matchmaking = std::make_unique<SlippiMatchmaking>(user.get());

  // Disconnect netplay client
  slippi_netplay = nullptr;

  // Clear character selections
  localSelections.Reset();

  // Reset random stage pool
  stagePool.clear();

  // Reset any forced errors
  forcedError.clear();

  // Reset any selection overwrites
  overwrite_selections.clear();

  // Reset play session
  is_play_session_active = false;

#ifdef LOCAL_TESTING
  isLocalConnected = false;
#endif

  ERROR_LOG_FMT(SLIPPI_ONLINE, "Connection cleanup completed...");
}

void CEXISlippi::prepareNewSeed()
{
  m_read_queue.clear();

  u32 newSeed = generator() % 0xFFFFFFFF;

  appendWordToBuffer(&m_read_queue, newSeed);
}

void CEXISlippi::handleReportGame(const SlippiExiTypes::ReportGameQuery& query)
{
  SlippiGameReporter::GameReport r;
  r.match_id = recentMmResult.id;
  r.mode = static_cast<SlippiMatchmaking::OnlinePlayMode>(query.mode);
  r.duration_frames = query.frame_length;
  r.game_index = query.game_index;
  r.tiebreak_index = query.tiebreak_index;
  r.winner_idx = query.winner_idx;
  r.stage_id = Common::FromBigEndian(*(u16*)&query.game_info_block[0xE]);
  r.game_end_method = query.game_end_method;
  r.lras_initiator = query.lras_initiator;

  ERROR_LOG_FMT(SLIPPI_ONLINE,
                "Mode: {} / {}, Frames: {}, GameIdx: {}, TiebreakIdx: {}, WinnerIdx: {}, "
                "StageId: {}, GameEndMethod: {}, LRASInitiator: {}",
                r.mode, query.mode, r.duration_frames, r.game_index, r.tiebreak_index, r.winner_idx,
                r.stage_id, r.game_end_method, r.lras_initiator);

  auto mm_players = recentMmResult.players;

  for (auto i = 0; i < 4; ++i)
  {
    SlippiGameReporter::PlayerReport p;
    p.uid = mm_players.size() > i ? mm_players[i].uid : "";
    p.slot_type = query.players[i].slot_type;
    p.stocks_remaining = query.players[i].stocks_remaining;
    p.damage_done = query.players[i].damage_done;
    p.char_id = query.game_info_block[0x60 + 0x24 * i];
    p.color_id = query.game_info_block[0x63 + 0x24 * i];
    p.starting_stocks = query.game_info_block[0x62 + 0x24 * i];
    p.starting_percent = Common::FromBigEndian(*(u16*)&query.game_info_block[0x70 + 0x24 * i]);

    ERROR_LOG_FMT(SLIPPI_ONLINE,
                  "UID: {}, Port Type: {}, Stocks: {}, DamageDone: {}, CharId: {}, ColorId: {}, "
                  "StartStocks: {}, "
                  "StartPercent: {}",
                  p.uid, p.slot_type, p.stocks_remaining, p.damage_done, p.char_id, p.color_id,
                  p.starting_stocks, p.starting_percent);

    r.players.push_back(p);
  }

#ifndef LOCAL_TESTING
  game_reporter->StartReport(r);
#endif
}

void CEXISlippi::prepareDelayResponse()
{
  m_read_queue.clear();
  m_read_queue.push_back(1);  // Indicate this is a real response

  if (NetPlay::IsNetPlayRunning())
  {
    // If we are using the old netplay, we don't want to add any additional delay, so return 0
    m_read_queue.push_back(0);
    return;
  }
  m_read_queue.push_back(Config::Get(Config::SLIPPI_ONLINE_DELAY));
}

void CEXISlippi::handleOverwriteSelections(const SlippiExiTypes::OverwriteSelectionsQuery& query)
{
  overwrite_selections.clear();

  for (int i = 0; i < 4; i++)
  {
    // TODO: I'm pretty sure this contine would cause bugs if we tried to overwrite only player 1
    // TODO: and not player 0. Right now though GamePrep always overwrites both p0 and p1 so it's
    // fine
    // TODO: The bug would likely happen in the prepareOnlineMatchState, it would overwrite the
    // TODO: wrong players I think
    if (!query.chars[i].is_set)
      continue;

    SlippiPlayerSelections s;
    s.isCharacterSelected = true;
    s.characterId = query.chars[i].char_id;
    s.characterColor = query.chars[i].char_color_id;
    s.isStageSelected = true;
    s.stageId = query.stage_id;
    s.playerIdx = i;

    overwrite_selections.push_back(s);
  }
}

void CEXISlippi::handleGamePrepStepComplete(const SlippiExiTypes::GpCompleteStepQuery& query)
{
  if (!slippi_netplay)
    return;

  SlippiGamePrepStepResults res;
  res.step_idx = query.step_idx;
  res.char_selection = query.char_selection;
  res.char_color_selection = query.char_color_selection;
  memcpy(res.stage_selections, query.stage_selections, 2);

  slippi_netplay->SendGamePrepStep(res);
}

void CEXISlippi::prepareGamePrepOppStep(const SlippiExiTypes::GpFetchStepQuery& query)
{
  SlippiExiTypes::GpFetchStepResponse resp;

  m_read_queue.clear();

  // Start by indicating not found
  resp.is_found = false;

#ifdef LOCAL_TESTING
  static int delay_count = 0;

  delay_count++;
  if (delay_count >= 90)
  {
    resp.is_found = true;
    resp.is_skip = true;  // Will make client just pick the next available options

    delay_count = 0;
  }
#else
  SlippiGamePrepStepResults res;
  if (slippi_netplay && slippi_netplay->GetGamePrepResults(query.step_idx, res))
  {
    // If we have received a response from the opponent, prepare the values for response
    resp.is_found = true;
    resp.is_skip = false;
    resp.char_selection = res.char_selection;
    resp.char_color_selection = res.char_color_selection;
    memcpy(resp.stage_selections, res.stage_selections, 2);
  }
#endif

  auto data_ptr = (u8*)&resp;
  m_read_queue.insert(m_read_queue.end(), data_ptr,
                      data_ptr + sizeof(SlippiExiTypes::GpFetchStepResponse));
}

void CEXISlippi::DMAWrite(u32 _uAddr, u32 _uSize)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* memPtr = memory.GetPointer(_uAddr);

  u32 bufLoc = 0;

  if (memPtr == nullptr)
  {
    ASSERT(Core::IsCPUThread());
    Core::CPUThreadGuard guard(system);
    NOTICE_LOG_FMT(SLIPPI, "DMA Write was passed an invalid address: {:x}", _uAddr);
    Dolphin_Debugger::PrintCallstack(system, guard, Common::Log::LogType::SLIPPI,
                                     Common::Log::LogLevel::LNOTICE);
    m_read_queue.clear();
    return;
  }

  u8 byte = memPtr[0];
  if (byte == CMD_RECEIVE_COMMANDS)
  {
    time(&gameStartTime);  // Store game start time
    u8 receiveCommandsLen = memPtr[1];
    configureCommands(&memPtr[1], receiveCommandsLen);
    writeToFileAsync(&memPtr[0], receiveCommandsLen + 1, "create");
    bufLoc += receiveCommandsLen + 1;
    g_needInputForFrame = true;
    SlippiSpectateServer::getInstance().startGame();
    SlippiSpectateServer::getInstance().write(&memPtr[0], receiveCommandsLen + 1);
  }

  if (byte == CMD_MENU_FRAME)
  {
    SlippiSpectateServer::getInstance().write(&memPtr[0], _uSize);
    g_needInputForFrame = true;
  }

  INFO_LOG_FMT(EXPANSIONINTERFACE,
               "EXI SLIPPI DMAWrite: addr: {:#x} size: {}, bufLoc:[{} {} {} {} {}]", _uAddr, _uSize,
               memPtr[bufLoc], memPtr[bufLoc + 1], memPtr[bufLoc + 2], memPtr[bufLoc + 3],
               memPtr[bufLoc + 4]);

  u8 prevCommandByte = 0;

  while (bufLoc < _uSize)
  {
    byte = memPtr[bufLoc];
    if (!payloadSizes.count(byte))
    {
      // This should never happen. Do something else if it does?
      ERROR_LOG_FMT(SLIPPI, "EXI SLIPPI: Invalid command byte: {:#x}. Prev command: {:#x}", byte,
                    prevCommandByte);
      return;
    }

    u32 payloadLen = payloadSizes[byte];
    switch (byte)
    {
    case CMD_RECEIVE_GAME_END:
      writeToFileAsync(&memPtr[bufLoc], payloadLen + 1, "close");
      SlippiSpectateServer::getInstance().write(&memPtr[bufLoc], payloadLen + 1);
      SlippiSpectateServer::getInstance().endGame();
      break;
    case CMD_PREPARE_REPLAY:
      // log.open("log.txt");
      prepareGameInfo(&memPtr[bufLoc + 1]);
      break;
    case CMD_READ_FRAME:
      prepareFrameData(&memPtr[bufLoc + 1]);
      break;
    case CMD_FRAME_BOOKEND:
      g_needInputForFrame = true;
      writeToFileAsync(&memPtr[bufLoc], payloadLen + 1, "");
      SlippiSpectateServer::getInstance().write(&memPtr[bufLoc], payloadLen + 1);
      break;
    case CMD_IS_STOCK_STEAL:
      prepareIsStockSteal(&memPtr[bufLoc + 1]);
      break;
    case CMD_IS_FILE_READY:
      prepareIsFileReady();
      break;
    case CMD_GET_GECKO_CODES:
      m_read_queue.clear();
      m_read_queue.insert(m_read_queue.begin(), geckoList.begin(), geckoList.end());
      break;
    case CMD_ONLINE_INPUTS:
      handleOnlineInputs(&memPtr[bufLoc + 1]);
      break;
    case CMD_CAPTURE_SAVESTATE:
      handleCaptureSavestate(&memPtr[bufLoc + 1]);
      break;
    case CMD_LOAD_SAVESTATE:
      handleLoadSavestate(&memPtr[bufLoc + 1]);
      break;
    case CMD_GET_MATCH_STATE:
      prepareOnlineMatchState();
      break;
    case CMD_FIND_OPPONENT:
      startFindMatch(&memPtr[bufLoc + 1]);
      break;
    case CMD_SET_MATCH_SELECTIONS:
      setMatchSelections(&memPtr[bufLoc + 1]);
      break;
    case CMD_FILE_LENGTH:
      prepareFileLength(&memPtr[bufLoc + 1]);
      break;
    case CMD_FETCH_CODE_SUGGESTION:
      handleNameEntryLoad(&memPtr[bufLoc + 1]);
      break;
    case CMD_FILE_LOAD:
      prepareFileLoad(&memPtr[bufLoc + 1]);
      break;
    case CMD_PREMADE_TEXT_LENGTH:
      preparePremadeTextLength(&memPtr[bufLoc + 1]);
      break;
    case CMD_PREMADE_TEXT_LOAD:
      preparePremadeTextLoad(&memPtr[bufLoc + 1]);
      break;
    case CMD_OPEN_LOGIN:
      handleLogInRequest();
      break;
    case CMD_LOGOUT:
      handleLogOutRequest();
      break;
    case CMD_GET_ONLINE_STATUS:
      prepareOnlineStatus();
      break;
    case CMD_CLEANUP_CONNECTION:
      handleConnectionCleanup();
      break;
    case CMD_LOG_MESSAGE:
      logMessageFromGame(&memPtr[bufLoc + 1]);
      break;
    case CMD_SEND_CHAT_MESSAGE:
      handleChatMessage(&memPtr[bufLoc + 1]);
      break;
    case CMD_UPDATE:
      user->UpdateApp();
      break;
    case CMD_GET_NEW_SEED:
      prepareNewSeed();
      break;
    case CMD_REPORT_GAME:
      handleReportGame(SlippiExiTypes::Convert<SlippiExiTypes::ReportGameQuery>(&memPtr[bufLoc]));
      break;
    case CMD_GCT_LENGTH:
      prepareGctLength();
      break;
    case CMD_GCT_LOAD:
      prepareGctLoad(&memPtr[bufLoc + 1]);
      break;
    case CMD_GET_DELAY:
      prepareDelayResponse();
      break;
    case CMD_OVERWRITE_SELECTIONS:
      handleOverwriteSelections(
          SlippiExiTypes::Convert<SlippiExiTypes::OverwriteSelectionsQuery>(&memPtr[bufLoc]));
      break;
    case CMD_GP_FETCH_STEP:
      prepareGamePrepOppStep(
          SlippiExiTypes::Convert<SlippiExiTypes::GpFetchStepQuery>(&memPtr[bufLoc]));
      break;
    case CMD_GP_COMPLETE_STEP:
      handleGamePrepStepComplete(
          SlippiExiTypes::Convert<SlippiExiTypes::GpCompleteStepQuery>(&memPtr[bufLoc]));
      break;
    default:
      writeToFileAsync(&memPtr[bufLoc], payloadLen + 1, "");
      SlippiSpectateServer::getInstance().write(&memPtr[bufLoc], payloadLen + 1);
      break;
    }

    prevCommandByte = byte;
    bufLoc += payloadLen + 1;
  }
}

void CEXISlippi::DMARead(u32 addr, u32 size)
{
  if (m_read_queue.empty())
  {
    ERROR_LOG_FMT(SLIPPI, "EXI SLIPPI DMARead: Empty");
    return;
  }

  m_read_queue.resize(size, 0);  // Resize response array to make sure it's all full/allocated

  auto queueAddr = &m_read_queue[0];
  INFO_LOG_FMT(EXPANSIONINTERFACE,
               "EXI SLIPPI DMARead: addr: {:#x} size: {}, startResp: [{} {} {} {} {}]", addr, size,
               queueAddr[0], queueAddr[1], queueAddr[2], queueAddr[3], queueAddr[4]);

  // Copy buffer data to memory
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyToEmu(addr, queueAddr, size);
}

bool CEXISlippi::IsPresent() const
{
  return true;
}

void CEXISlippi::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
