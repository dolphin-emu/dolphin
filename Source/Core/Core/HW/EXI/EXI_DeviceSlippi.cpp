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

#include "AudioCommon/AudioCommon.h"

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

// The Rust library that houses a "shadow" EXI Device that we can call into.
#include "EXI_DeviceSlippi.h"
#include "SlippiRustExtensions.h"

#define FRAME_INTERVAL 900
#define SLEEP_TIME_MS 8
#define WRITE_FILE_SLEEP_TIME_MS 85

// #define LOCAL_TESTING
// #define CREATE_DIFF_FILES
extern std::unique_ptr<SlippiPlaybackStatus> g_playback_status;
extern std::unique_ptr<SlippiReplayComm> g_replay_comm;
extern bool g_need_input_for_frame;

#ifdef LOCAL_TESTING
bool is_local_connected = false;
int local_chat_message_id = 0;
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
  auto word_vector = uint32ToVector(word);
  buf->insert(buf->end(), word_vector.begin(), word_vector.end());
}

void appendHalfToBuffer(std::vector<u8>* buf, u16 word)
{
  auto half_vector = uint16ToVector(word);
  buf->insert(buf->end(), half_vector.begin(), half_vector.end());
}

std::string ConvertConnectCodeForGame(const std::string& input)
{
  // Shift-Jis '#' symbol is two bytes (0x8194), followed by 0x00 null terminator
  signed char full_width_shift_jis_hashtag[] = {-127, -108, 0};  // 0x81, 0x94, 0x00
  std::string connect_code(input);
  // SLIPPITODO：Not the best use of ReplaceAll. potential bug if more than one '#' found.
  connect_code = ReplaceAll(connect_code, "#", std::string(reinterpret_cast<const char*>(full_width_shift_jis_hashtag)));
  // fixed length + full width (two byte) hashtag +1, null terminator +1
  connect_code.resize(CONNECT_CODE_LENGTH + 2);
  return connect_code;
}

// This function gets passed to the Rust EXI device to support emitting OSD messages
// across the Rust/C/C++ boundary.
void OSDMessageHandler(const char* message, u32 color, u32 duration_ms)
{
  // When called with a C str type, this constructor does a copy.
  //
  // We intentionally do this to ensure that there are no ownership issues with a C String coming
  // from the Rust side. This isn't a particularly hot code path so we don't need to care about
  // the extra allocation, but this could be revisited in the future.
  std::string msg(message);

  OSD::AddMessage(msg, duration_ms, color);
}

CEXISlippi::CEXISlippi(Core::System& system, const std::string current_file_name)
    : IEXIDevice(system)
{
  INFO_LOG_FMT(SLIPPI, "EXI SLIPPI Constructor called.");

  std::string user_file_path = File::GetUserPath(F_USERJSON_IDX);

  SlippiRustEXIConfig slprs_exi_config;
  slprs_exi_config.iso_path = current_file_name.c_str();
  slprs_exi_config.user_json_path = user_file_path.c_str();
  slprs_exi_config.scm_slippi_semver_str = Common::GetSemVerStr().c_str();
  slprs_exi_config.osd_add_msg_fn = OSDMessageHandler;

  slprs_exi_device_ptr = slprs_exi_device_create(slprs_exi_config);

  user = std::make_unique<SlippiUser>(slprs_exi_device_ptr);
  g_playback_status = std::make_unique<SlippiPlaybackStatus>();
  matchmaking = std::make_unique<SlippiMatchmaking>(user.get());
  game_file_loader = std::make_unique<SlippiGameFileLoader>();
  g_replay_comm = std::make_unique<SlippiReplayComm>();
  direct_codes = std::make_unique<SlippiDirectCodes>("direct-codes.json");
  teams_codes = std::make_unique<SlippiDirectCodes>("teams-codes.json");

  // initialize the spectate server so we can connect without starting a game
  SlippiSpectateServer::getInstance();

  generator = std::default_random_engine(Common::Timer::NowMs());

  // Loggers will check 5 bytes, make sure we own that memory
  m_read_queue.reserve(5);

  // Initialize local selections to empty
  local_selections.Reset();

  // Forces savestate to re-init regions when a new ISO is loaded
  SlippiSavestate::should_force_init = true;

  // Update user file and then listen for User
#ifndef IS_PLAYBACK
  user->ListenForLogIn();
#endif

  // Use sane stage defaults (should get overwritten)
  allowed_stages = {
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
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.usd.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnMaAll.usd.diff", diff);

  // MnExtAll.usd
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.usd",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll-new.usd", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.usd.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnExtAll.usd.diff", diff);

  // SdMenu.usd
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.usd",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu-new.usd", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.usd.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\SdMenu.usd.diff", diff);

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
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll.dat.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnMaAll.dat.diff", diff);

  // MnExtAll.dat
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.dat",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll-new.dat", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\CSS\\MnExtAll.dat.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\MnExtAll.dat.diff", diff);

  // SdMenu.dat
  File::ReadFileToString("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.dat",
                         origStr);
  File::ReadFileToString(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu-new.dat", modifiedStr);
  orig = std::vector<u8>(origStr.begin(), origStr.end());
  modified = std::vector<u8>(modifiedStr.begin(), modifiedStr.end());
  diff = processDiff(orig, modified);
  File::WriteStringToFile(
      "C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\SdMenu.dat.diff", diff);
  File::WriteStringToFile("C:\\Dolphin\\IshiiDev\\Sys\\GameFiles\\GALE01\\SdMenu.dat.diff", diff);

  // TEMP - Restore orig
  // std::string stateString;
  // decoder.Decode((char *)orig.data(), orig.size(), diff, &stateString);
  // File::WriteStringToFile("C:\\Users\\Jas\\Documents\\Melee\\Textures\\Slippi\\MainMenu\\MnMaAll-restored.usd",
  // stateString);
#endif
}

CEXISlippi::~CEXISlippi()
{
  u8 empty[1];

  // Closes file gracefully to prevent file corruption when emulation
  // suddenly stops. This would happen often on netplay when the opponent
  // would close the emulation before the file successfully finished writing
  writeToFileAsync(&empty[0], 0, "close");
  write_thread_running = false;
  if (m_file_write_thread.joinable())
  {
    m_file_write_thread.join();
  }

  SlippiSpectateServer::getInstance().endGame(true);

  // Try to determine whether we were playing an in-progress ranked match, if so
  // indicate to server that this client has abandoned. Anyone trying to modify
  // this behavior to game their rating is subject to get banned.
  auto active_match_id = matchmaking->GetMatchmakeResult().id;
  if (active_match_id.find("mode.ranked") != std::string::npos)
  {
    ERROR_LOG_FMT(SLIPPI_ONLINE, "Exit during in-progress ranked game: {}", active_match_id);
    slprs_exi_device_report_match_abandonment(slprs_exi_device_ptr, active_match_id.c_str());
  }
  handleConnectionCleanup();

  local_selections.Reset();

  g_playback_status->resetPlayback();

  // Instruct the Rust EXI device to shut down/drop everything.
  slprs_exi_device_destroy(slprs_exi_device_ptr);

  // TODO: ENET shutdown should maybe be done at app shutdown instead.
  // Right now this might be problematic in the case where someone starts a netplay client
  // and then queues into online matchmaking, and then stops the game. That might deinit
  // the ENET libraries so that they can't be used anymore for the netplay lobby? Course
  // you'd have to be kinda dumb to do that sequence of stuff anyway so maybe it's nbd
  if (is_enet_initialized)
    enet_deinitialize();
}

void CEXISlippi::configureCommands(u8* payload, u8 length)
{
  for (int i = 1; i < length; i += 3)
  {
    // Go through the receive commands payload and set up other commands
    u8 command_byte = payload[i];
    u32 command_payload_size = payload[i + 1] << 8 | payload[i + 2];
    payloadSizes[command_byte] = command_payload_size;
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
  last_frame = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];

  // Keep track of character usage
  u8 player_idx = payload[5];
  u8 internal_character_id = payload[7];
  if (!character_usage.count(player_idx) ||
      !character_usage[player_idx].count(internal_character_id))
  {
    character_usage[player_idx][internal_character_id] = 0;
  }
  character_usage[player_idx][internal_character_id] += 1;
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
  u8 date_time_str_len = sizeof "2011-10-08T07:07:09Z";
  std::vector<char> date_time_buf(date_time_str_len);
  strftime(&date_time_buf[0], date_time_str_len, "%FT%TZ", gmtime(&game_start_time));
  date_time_buf.pop_back();  // Removes the \0 from the back of string
  metadata.insert(metadata.end(), {'U', 7, 's', 't', 'a', 'r', 't', 'A', 't', 'S', 'U',
                                   static_cast<u8>(date_time_buf.size())});
  metadata.insert(metadata.end(), date_time_buf.begin(), date_time_buf.end());

  // Add game duration
  std::vector<u8> last_frame_to_write = int32ToVector(last_frame);
  metadata.insert(metadata.end(), {'U', 9, 'l', 'a', 's', 't', 'F', 'r', 'a', 'm', 'e', 'l'});
  metadata.insert(metadata.end(), last_frame_to_write.begin(), last_frame_to_write.end());

  // Add players elements to metadata, one per player index
  metadata.insert(metadata.end(), {'U', 7, 'p', 'l', 'a', 'y', 'e', 'r', 's', '{'});

  auto player_names = getNetplayNames();

  for (auto it = character_usage.begin(); it != character_usage.end(); ++it)
  {
    auto player_idx = it->first;
    auto player_character_usage = it->second;

    metadata.push_back('U');
    std::string player_idx_str = std::to_string(player_idx);
    metadata.push_back(static_cast<u8>(player_idx_str.length()));
    metadata.insert(metadata.end(), player_idx_str.begin(), player_idx_str.end());
    metadata.push_back('{');

    // Add names element for this player
    metadata.insert(metadata.end(), {'U', 5, 'n', 'a', 'm', 'e', 's', '{'});

    if (player_names.count(player_idx))
    {
      auto player_name = player_names[player_idx];
      // Add netplay element for this player name
      metadata.insert(metadata.end(), {'U', 7, 'n', 'e', 't', 'p', 'l', 'a', 'y', 'S', 'U'});
      metadata.push_back(static_cast<u8>(player_name.length()));
      metadata.insert(metadata.end(), player_name.begin(), player_name.end());
    }

    if (slippi_connect_codes.count(player_idx))
    {
      auto connect_code = slippi_connect_codes[player_idx];
      // Add connection code element for this player name
      metadata.insert(metadata.end(), {'U', 4, 'c', 'o', 'd', 'e', 'S', 'U'});
      metadata.push_back(static_cast<u8>(connect_code.length()));
      metadata.insert(metadata.end(), connect_code.begin(), connect_code.end());
    }

    metadata.push_back('}');  // close names

    // Add character element for this player
    metadata.insert(metadata.end(),
                    {'U', 10, 'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', 's', '{'});
    for (auto it2 = player_character_usage.begin(); it2 != player_character_usage.end(); ++it2)
    {
      metadata.push_back('U');
      std::string internal_char_id_str = std::to_string(it2->first);
      metadata.push_back(static_cast<u8>(internal_char_id_str.length()));
      metadata.insert(metadata.end(), internal_char_id_str.begin(), internal_char_id_str.end());

      metadata.push_back('l');
      std::vector<u8> frame_count = uint32ToVector(it2->second);
      metadata.insert(metadata.end(), frame_count.begin(), frame_count.end());
    }
    metadata.push_back('}');  // close characters

    metadata.push_back('}');  // close player
  }
  metadata.push_back('}');

  // Indicate this was played on mainline dolphin
  metadata.insert(metadata.end(),
                  {'U', 8,   'p', 'l', 'a', 'y', 'e', 'd', 'O', 'n', 'S', 'U', 16,  'm', 'a',
                   'i', 'n', 'l', 'i', 'n', 'e', ' ', 'd', 'o', 'l', 'p', 'h', 'i', 'n'});

  metadata.push_back('}');
  return metadata;
}

void CEXISlippi::writeToFileAsync(u8* payload, u32 length, std::string file_option)
{
  if (!Config::Get(Config::SLIPPI_SAVE_REPLAYS))
  {
    return;
  }

  if (file_option == "create" && !write_thread_running)
  {
    WARN_LOG_FMT(SLIPPI, "Creating file write thread...");
    write_thread_running = true;
    m_file_write_thread = std::thread(&CEXISlippi::FileWriteThread, this);
  }

  if (!write_thread_running)
  {
    return;
  }

  std::vector<u8> payload_data;
  payload_data.insert(payload_data.end(), payload, payload + length);

  auto write_msg = std::make_unique<WriteMessage>();
  write_msg->data = payload_data;
  write_msg->operation = file_option;

  file_write_queue.Push(std::move(write_msg));
}

void CEXISlippi::FileWriteThread(void)
{
  Common::SetCurrentThreadName("Slippi File Write");
  while (write_thread_running || !file_write_queue.Empty())
  {
    // Process all messages
    while (!file_write_queue.Empty())
    {
      writeToFile(std::move(file_write_queue.Front()));
      file_write_queue.Pop();

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
  std::string file_option = msg->operation;

  std::vector<u8> data_to_write;
  if (file_option == "create")
  {
    // If the game sends over option 1 that means a file should be created
    createNewFile();

    // Start ubjson file and prepare the "raw" element that game
    // data output will be dumped into. The size of the raw output will
    // be initialized to 0 until all of the data has been received
    std::vector<u8> header_bytes({'{', 'U', 3, 'r', 'a', 'w', '[', '$', 'U', '#', 'l', 0, 0, 0, 0});
    data_to_write.insert(data_to_write.end(), header_bytes.begin(), header_bytes.end());

    // Used to keep track of how many bytes have been written to the file
    written_byte_count = 0;

    // Used to track character usage (sheik/zelda)
    character_usage.clear();

    // Reset last_frame
    last_frame = Slippi::GAME_FIRST_FRAME;

    // Get display names and connection codes from slippi netplay client
    if (slippi_netplay)
    {
      auto player_info = matchmaking->GetPlayerInfo();

      for (int i = 0; i < player_info.size(); i++)
      {
        slippi_names[i] = player_info[i].display_name;
        slippi_connect_codes[i] = player_info[i].connect_code;
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
  data_to_write.insert(data_to_write.end(), payload, payload + length);
  written_byte_count += length;

  // If we are going to close the file, generate data to complete the UBJSON file
  if (file_option == "close")
  {
    // This option indicates we are done sending over body
    std::vector<u8> closing_bytes = generateMetadata();
    closing_bytes.push_back('}');
    data_to_write.insert(data_to_write.end(), closing_bytes.begin(), closing_bytes.end());

    // Reset display names and connect codes retrieved from netplay client
    slippi_names.clear();
    slippi_connect_codes.clear();
  }

  // Write data to file
  bool result = m_file.WriteBytes(&data_to_write[0], data_to_write.size());
  if (!result)
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to write data to file.");
  }

  // If file should be closed, close it
  if (file_option == "close")
  {
    // Write the number of bytes for the raw output
    std::vector<u8> size_bytes = uint32ToVector(written_byte_count);
    m_file.Seek(11, File::SeekOrigin::Begin);
    m_file.WriteBytes(&size_bytes[0], size_bytes.size());

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

  std::string dir_path = Config::Get(Config::SLIPPI_REPLAY_DIR);
  // in case the config value just gets lost somehow
  if (dir_path.empty())
  {
    Config::Get(Config::SLIPPI_REPLAY_DIR) = File::GetHomeDirectory() + DIR_SEP + "Slippi";
    dir_path = Config::Get(Config::SLIPPI_REPLAY_DIR);
  }

  // Remove a trailing / or \\ if the user managed to have that in their config
  char dirpath_end = dir_path.back();
  if (dirpath_end == '/' || dirpath_end == '\\')
  {
    dir_path.pop_back();
  }

  // First, ensure that the root Slippi replay directory is created
  File::CreateFullPath(dir_path + DIR_SEP);

  // Now we have a dir such as /home/Replays but we need to make one such
  // as /home/Replays/2020-06 if month categorization is enabled
  if (Config::Get(Config::SLIPPI_REPLAY_MONTHLY_FOLDERS))
  {
    dir_path.push_back('/');

    // Append YYYY-MM to the directory path
    uint8_t year_month_str_len = sizeof "2020-06-Mainline";
    std::vector<char> year_month_buf(year_month_str_len);
    strftime(&year_month_buf[0], year_month_str_len, "%Y-%m-Mainline", localtime(&game_start_time));

    std::string year_month(&year_month_buf[0]);
    dir_path.append(year_month);

    // Ensure that the subfolder directory is created
    File::CreateDir(dir_path);
  }

  std::string file_path = dir_path + DIR_SEP + generateFileName();
  INFO_LOG_FMT(SLIPPI, "EXI_DeviceSlippi.cpp: Creating new replay file {}", file_path.c_str());

#ifdef _WIN32
  m_file = File::IOFile(file_path, "wb", File::SharedAccess::Read);
#else
  m_file = File::IOFile(file_path, "wb");
#endif

  if (!m_file)
  {
    PanicAlertFmtT("Could not create .slp replay file [{0}].\n\n"
                   "The replay folder's path might be invalid, or you might "
                   "not have permission to write to it.\n\n"
                   "You can change the replay folder in Config > Slippi > "
                   "Slippi Replay Settings.",
                   file_path);
  }
}

std::string CEXISlippi::generateFileName()
{
  // Add game start time
  u8 date_time_str_len = sizeof "20171015T095717";
  std::vector<char> date_time_buf(date_time_str_len);
  strftime(&date_time_buf[0], date_time_str_len, "%Y%m%dT%H%M%S", localtime(&game_start_time));

  std::string str(&date_time_buf[0]);
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
  playback_savestate_payload.clear();
  appendWordToBuffer(&playback_savestate_payload, 0);  // This space will be used to set frame index
  int bkp_pos = 0;
  while ((*(u32*)(&payload[bkp_pos * 8])) != 0)
  {
    bkp_pos += 1;
  }
  playback_savestate_payload.insert(playback_savestate_payload.end(), payload,
                                    payload + (bkp_pos * 8 + 4));

  Slippi::GameSettings* settings = m_current_game->GetSettings();

  // Unlikely but reset the overclocking in case we quit during a hard ffw in a previous play
  SConfig::GetSlippiConfig().oc_enable = g_playback_status->orig_OC_enable;
  SConfig::GetSlippiConfig().oc_factor = g_playback_status->orig_OC_factor;

  // Start in Fast Forward if this is mirrored
  auto replay_comm_settings = g_replay_comm->getSettings();
  if (!g_playback_status->is_hard_FFW)
    g_playback_status->is_hard_FFW = replay_comm_settings.mode == "mirror";
  g_playback_status->last_FFW_frame = INT_MIN;

  // Build a word containing the stage and the presence of the characters
  u32 random_seed = settings->randomSeed;
  appendWordToBuffer(&m_read_queue, random_seed);

  // This is kinda dumb but we need to handle the case where a player transforms
  // into sheik/zelda immediately. This info is not stored in the game info header
  // and so let's overwrite those values
  int player1_pos = 24;  // This is the index of the first players character info
  std::array<u32, Slippi::GAME_INFO_HEADER_SIZE> game_info_header = settings->header;
  for (int i = 0; i < 4; i++)
  {
    // check if this player is actually in the game
    bool player_exists = m_current_game->DoesPlayerExist(i);
    if (!player_exists)
    {
      continue;
    }

    // check if the player is playing sheik or zelda
    u8 external_char_id = settings->players[i].characterId;
    if (external_char_id != 0x12 && external_char_id != 0x13)
    {
      continue;
    }

    // this is the position in the array that this player's character info is stored
    int pos = player1_pos + (9 * i);

    // here we have determined the player is playing sheik or zelda...
    // at this point let's overwrite the player's character with the one
    // that they are playing
    game_info_header[pos] &= 0x00FFFFFF;
    game_info_header[pos] |= external_char_id << 24;
  }

  // Write entire header to game
  for (int i = 0; i < Slippi::GAME_INFO_HEADER_SIZE; i++)
  {
    appendWordToBuffer(&m_read_queue, game_info_header[i]);
  }

  // Write UCF toggles
  std::array<u32, Slippi::UCF_TOGGLE_SIZE> ucf_toggles = settings->ucfToggles;
  for (int i = 0; i < Slippi::UCF_TOGGLE_SIZE; i++)
  {
    appendWordToBuffer(&m_read_queue, ucf_toggles[i]);
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
  auto replay_version = m_current_game->GetVersion();
  auto major_version = replay_version[0];
  auto minor_version = replay_version[1];

  // Write PS pre-load byte
  auto should_preload_ps = major_version > 1 || (major_version == 1 && minor_version > 2);
  m_read_queue.push_back(should_preload_ps);

  // Write PS Frozen byte
  m_read_queue.push_back(settings->isFrozenPS);

  // Write should resync setting
  m_read_queue.push_back(replay_comm_settings.should_resync ? 1 : 0);

  // Write display names
  for (int i = 0; i < 4; i++)
  {
    auto display_name = settings->players[i].displayName;
    m_read_queue.insert(m_read_queue.end(), display_name.begin(), display_name.end());
  }

  // Return the size of the gecko code list
  prepareGeckoList();
  appendWordToBuffer(&m_read_queue, static_cast<u32>(gecko_list.size()));

  // Initialize frame sequence index value for reading rollbacks
  frame_seq_idx = 0;

  if (replay_comm_settings.rollback_display_method != "off")
  {
    // Prepare savestates
    available_savestates.clear();
    active_savestates.clear();

    // Prepare savestates for online play
    for (int i = 0; i < ROLLBACK_MAX_FRAMES; i++)
    {
      available_savestates.push_back(std::make_unique<SlippiSavestate>());
    }
  }
  else
  {
    // Prepare savestates
    available_savestates.clear();
    active_savestates.clear();

    // Add savestate for testing
    available_savestates.push_back(std::make_unique<SlippiSavestate>());
  }

  // Reset playback frame to begining
  g_playback_status->current_playback_frame = Slippi::GAME_FIRST_FRAME;

  // Initialize replay related threads if not viewing rollback versions of relays
  if (replay_comm_settings.rollback_display_method == "off" &&
      (replay_comm_settings.mode == "normal" || replay_comm_settings.mode == "queue"))
  {
    // g_playback_status->startThreads();
  }
}

void CEXISlippi::prepareGeckoList()
{
  // Assignment like this copies the values into a new map I think
  std::vector<u8> legacy_code_list = g_playback_status->getLegacyCodelist();
  std::unordered_map<u32, bool> deny_list = g_playback_status->getDenylist();

  auto replay_comm_settings = g_replay_comm->getSettings();
  // Some codes should only be denylisted when not displaying rollbacks, these are codes
  // that are required for things to not break when using Slippi savestates. Perhaps this
  // should be handled by actually applying these codes in the playback ASM instead? not sure
  auto should_deny = replay_comm_settings.rollback_display_method == "off";
  deny_list[0x8038add0] = should_deny;  // Online/Core/PreventFileAlarms/PreventMusicAlarm.asm
  deny_list[0x80023FFC] = should_deny;  // Online/Core/PreventFileAlarms/MuteMusic.asm

  gecko_list.clear();

  Slippi::GameSettings* settings = m_current_game->GetSettings();
  if (settings->geckoCodes.empty())
  {
    gecko_list = legacy_code_list;
    return;
  }

  std::vector<u8> source = settings->geckoCodes;
  INFO_LOG_FMT(SLIPPI, "Booting codes with source size: {}", source.size());

  int idx = 0;
  while (idx < source.size())
  {
    u8 code_type = source[idx] & 0xFE;
    u32 address =
        source[idx] << 24 | source[idx + 1] << 16 | source[idx + 2] << 8 | source[idx + 3];
    address = (address & 0x01FFFFFF) | 0x80000000;

    u32 code_offset = 8;  // Default code offset. Most codes are this length
    switch (code_type)
    {
    case 0xC0:
    case 0xC2:
    {
      u32 line_count =
          source[idx + 4] << 24 | source[idx + 5] << 16 | source[idx + 6] << 8 | source[idx + 7];
      code_offset = 8 + (line_count * 8);
      break;
    }
    case 0x08:
      code_offset = 16;
      break;
    case 0x06:
    {
      u32 byte_len =
          source[idx + 4] << 24 | source[idx + 5] << 16 | source[idx + 6] << 8 | source[idx + 7];
      // Round up to next 8 bytes and add the first 8 bytes
      code_offset = 8 + ((byte_len + 7) & 0xFFFFFFF8);
      break;
    }
    }

    idx += code_offset;

    // If this address is denylisted, we don't add it to what we will send to game
    if (deny_list[address])
      continue;

    INFO_LOG_FMT(SLIPPI, "Codetype [{:x}] Inserting section: {} - {} ({:x}, {})", code_type,
                 idx - code_offset, idx, address, code_offset);

    // If not denylisted, add code to return vector
    gecko_list.insert(gecko_list.end(), source.begin() + (idx - code_offset), source.begin() + idx);
  }

  // Add the termination sequence
  gecko_list.insert(gecko_list.end(), {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
}

void CEXISlippi::prepareCharacterFrameData(Slippi::FrameData* frame, u8 port, u8 is_follower)
{
  std::unordered_map<uint8_t, Slippi::PlayerFrameData> source;
  source = is_follower ? frame->followers : frame->players;

  // This must be updated if new data is added
  int character_data_len = 50;

  // Check if player exists
  if (!source.count(port))
  {
    // If player does not exist, insert blank section
    m_read_queue.insert(m_read_queue.end(), character_data_len, 0);
    return;
  }

  // Get data for this player
  Slippi::PlayerFrameData data = source[port];

  // log << frame_idx << "\t" << port << "\t" << data.locationX << "\t" << data.locationY << "\t"
  // << data.animation
  // << "\n";

  // WARN_LOG_FMT(EXPANSIONINTERFACE, "[Frame {}] [Player {}] Positions: %f | %f", frame_idx, port,
  // data.locationX, data.locationY);

  // Add all of the inputs in order
  appendWordToBuffer(&m_read_queue, data.randomSeed);
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.joystickX));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.joystickY));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.cstickX));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.cstickY));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.trigger));
  appendWordToBuffer(&m_read_queue, data.buttons);
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.locationX));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.locationY));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.facingDirection));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.animation));
  m_read_queue.push_back(data.joystickXRaw);
  m_read_queue.push_back(data.joystickYRaw);
  appendWordToBuffer(&m_read_queue, static_cast<u32>(data.percent));
  // NOTE TO DEV: If you add data here, make sure to increase the size above
}

bool CEXISlippi::checkFrameFullyFetched(s32 frame_idx)
{
  auto does_frame_exist = m_current_game->DoesFrameExist(frame_idx);
  if (!does_frame_exist)
    return false;

  Slippi::FrameData* frame = m_current_game->GetFrame(frame_idx);

  version::Semver200_version last_finalized_version("3.7.0");
  version::Semver200_version current_version(m_current_game->GetVersionString());

  bool frame_is_finalized = true;
  if (current_version >= last_finalized_version)
  {
    // If latest finalized frame should exist, check it as well. This will prevent us
    // from loading a non-committed frame when mirroring a rollback game
    frame_is_finalized = m_current_game->GetLastFinalizedFrame() >= frame_idx;
  }

  // This flag is set to true after a post frame update has been received. At that point
  // we know we have received all of the input data for the frame
  return frame->inputsFullyFetched && frame_is_finalized;
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
  s32 frame_idx = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];

  // If loading from queue, move on to the next replay if we have past end_frame
  auto watch_settings = g_replay_comm->current;
  if (frame_idx > watch_settings.end_frame)
  {
    INFO_LOG_FMT(SLIPPI, "Killing game because we are past end_frame");
    m_read_queue.push_back(FRAME_RESP_TERMINATE);
    return;
  }

  // Hides frame index message on waiting for game screen
  // OSD::AddTypedMessage(OSD::MessageType::FrameIndex, "", 0, OSD::Color::CYAN);

  // If a new replay should be played, terminate the current game
  auto is_new_replay = g_replay_comm->isNewReplay();
  if (is_new_replay)
  {
    m_read_queue.push_back(FRAME_RESP_TERMINATE);
    return;
  }

  auto is_processing_complete = m_current_game->IsProcessingComplete();
  // Wait until frame exists in our data before reading it. We also wait until
  // next frame has been found to ensure we have actually received all of the
  // data from this frame. Don't wait until next frame is processing is complete
  // (this is the last frame, in that case)
  auto is_frame_found = m_current_game->DoesFrameExist(frame_idx);
  g_playback_status->last_frame = m_current_game->GetLatestIndex();
  auto is_frame_complete = checkFrameFullyFetched(frame_idx);
  auto is_frame_ready = is_frame_found && (is_processing_complete || is_frame_complete);

  // If there is a start_frame configured, manage the fast-forward flag
  if (watch_settings.start_frame > Slippi::GAME_FIRST_FRAME)
  {
    if (frame_idx < watch_settings.start_frame)
    {
      g_playback_status->setHardFFW(true);
    }
    else if (frame_idx == watch_settings.start_frame)
    {
      // TODO: This might disable fast forward on first frame when we dont want to?
      g_playback_status->setHardFFW(false);
    }
  }

  auto comm_settings = g_replay_comm->getSettings();
  if (comm_settings.rollback_display_method == "normal")
  {
    auto next_frame = m_current_game->GetFrameAt(frame_seq_idx);
    bool should_hard_FFW =
        next_frame && next_frame->frame <= g_playback_status->current_playback_frame;
    g_playback_status->setHardFFW(should_hard_FFW);

    if (next_frame)
    {
      // This feels jank but without this g_playback_status ends up getting updated to
      // a value beyond the frame that actually gets played causes too much FFW
      frame_idx = next_frame->frame;
    }
  }

  // If RealTimeMode is enabled, let's trigger fast forwarding under certain conditions
  auto is_far_behind = g_playback_status->last_frame - frame_idx > 2;
  auto is_very_far_behind = g_playback_status->last_frame - frame_idx > 25;
  if (is_far_behind && comm_settings.mode == "mirror" && comm_settings.is_real_time_mode)
  {
    g_playback_status->is_soft_FFW = true;

    // Once is_hard_FFW has been turned on, do not turn it off with this condition, should
    // hard FFW to the latest point
    if (!g_playback_status->is_hard_FFW)
      g_playback_status->is_hard_FFW = is_very_far_behind;
  }

  if (g_playback_status->last_frame == frame_idx)
  {
    // The reason to disable fast forwarding here is in hopes
    // of disabling it on the last frame that we have actually received.
    // Doing this will allow the rendering logic to run to display the
    // last frame instead of the frame previous to fast forwarding.
    // Not sure if this fully works with partial frames
    g_playback_status->is_soft_FFW = false;
    g_playback_status->setHardFFW(false);
  }

  bool should_FFW = g_playback_status->shouldFFWFrame(frame_idx);
  u8 request_result_code = should_FFW ? FRAME_RESP_FASTFORWARD : FRAME_RESP_CONTINUE;
  if (!is_frame_ready)
  {
    // If processing is complete, the game has terminated early. Tell our playback
    // to end the game as well.
    auto should_terminate_game = is_processing_complete;
    request_result_code = should_terminate_game ? FRAME_RESP_TERMINATE : FRAME_RESP_WAIT;
    m_read_queue.push_back(request_result_code);

    // Disable fast forward here too... this shouldn't be necessary but better
    // safe than sorry I guess
    g_playback_status->is_soft_FFW = false;
    g_playback_status->setHardFFW(false);

    if (request_result_code == FRAME_RESP_TERMINATE)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Game should terminate on frame {} [{}]", frame_idx,
                    frame_idx);
    }

    return;
  }

  u8 rollback_code = 0;  // 0 = not rollback, 1 = rollback, perhaps other options in the future?

  // Increment frame index if greater
  if (frame_idx > g_playback_status->current_playback_frame)
  {
    g_playback_status->current_playback_frame = frame_idx;
  }
  else if (comm_settings.rollback_display_method != "off")
  {
    rollback_code = 1;
  }

  // Keep track of last FFW frame, used for soft FFW's
  if (should_FFW)
  {
    WARN_LOG_FMT(SLIPPI, "[Frame {}] FFW frame, behind by: {} frames.", frame_idx,
                 g_playback_status->last_frame - frame_idx);
    g_playback_status->last_FFW_frame = frame_idx;
  }

  // Return success code
  m_read_queue.push_back(request_result_code);

  // Get frame
  Slippi::FrameData* frame = m_current_game->GetFrame(frame_idx);
  if (comm_settings.rollback_display_method != "off")
  {
    auto previous_frame = m_current_game->GetFrameAt(frame_seq_idx - 1);
    frame = m_current_game->GetFrameAt(frame_seq_idx);

    // SLIPPITODO: document what is going on here
    *(s32*)(&playback_savestate_payload[0]) = Common::swap32(frame->frame);

    if (previous_frame && frame->frame <= previous_frame->frame)
    {
      // Here we should load a savestate
      handleLoadSavestate(&playback_savestate_payload[0]);
    }

    // Here we should save a savestate
    handleCaptureSavestate(&playback_savestate_payload[0]);

    frame_seq_idx += 1;
  }

  // For normal replays, modify slippi seek/playback data as needed
  // TODO: maybe handle other modes too?
  if (comm_settings.mode == "normal" || comm_settings.mode == "queue")
  {
    g_playback_status->prepareSlippiPlayback(frame->frame);
  }

  // Push RB code
  m_read_queue.push_back(rollback_code);

  // Add frame rng seed to be restored at priority 0
  u8 rng_result = frame->randomSeedExists ? 1 : 0;
  m_read_queue.push_back(rng_result);
  appendWordToBuffer(&m_read_queue, static_cast<u32>(frame->randomSeed));

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
  s32 frame_idx = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
  u8 player_idx = payload[4];

  // I'm not sure checking for the frame should be necessary. Theoretically this
  // should get called after the frame request so the frame should already exist
  auto is_frame_found = m_current_game->DoesFrameExist(frame_idx);
  if (!is_frame_found)
  {
    m_read_queue.push_back(0);
    return;
  }

  // Load the data from this frame into the read buffer
  Slippi::FrameData* frame = m_current_game->GetFrame(frame_idx);
  auto players = frame->players;

  u8 player_is_back = players.count(player_idx) ? 1 : 0;
  m_read_queue.push_back(player_is_back);
}

void CEXISlippi::prepareIsFileReady()
{
  m_read_queue.clear();

  auto is_new_replay = g_replay_comm->isNewReplay();
  if (!is_new_replay)
  {
    g_replay_comm->nextReplay();
    m_read_queue.push_back(0);
    return;
  }

  // Attempt to load game if there is a new replay file
  // this can come pack falsy if the replay file does not exist
  m_current_game = g_replay_comm->loadGame();
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
  g_playback_status->resetPlayback();

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
    available_savestates.clear();
    active_savestates.clear();

    // Prepare savestates for online play
    for (int i = 0; i < ROLLBACK_MAX_FRAMES; i++)
    {
      available_savestates.push_back(std::make_unique<SlippiSavestate>());
    }

    // Reset stall counter
    is_connection_stalled = false;
    stall_frame_count = 0;

    // Reset skip variables
    frames_to_skip = 0;
    is_currently_skipping = false;

    // Reset advance stuff
    frames_to_advance = 0;
    is_currently_advancing = false;
    fall_behind_counter = 0;
    fall_far_behind_counter = 0;

    // Reset character selections such that they are cleared for next game
    local_selections.Reset();
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

  bool should_skip = shouldSkipOnlineFrame(frame, finalized_frame);
  if (should_skip)
  {
    // Send inputs that have not yet been acked
    slippi_netplay->SendSlippiPad(nullptr);
  }
  else
  {
    // Send the input for this frame along with everything that has yet to be acked
    handleSendInputs(frame, delay, finalized_frame, finalized_frame_checksum, inputs);
  }

  prepareOpponentInputs(frame, should_skip);
}

bool CEXISlippi::shouldSkipOnlineFrame(s32 frame, s32 finalized_frame)
{
  auto status = slippi_netplay->GetSlippiConnectStatus();
  bool connection_failed =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_FAILED;
  bool connection_disconnected =
      status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_DISCONNECTED;
  if (connection_failed || connection_disconnected)
  {
    // If connection failed just continue the game
    return false;
  }

  if (is_connection_stalled)
  {
    return false;
  }

  // Return true if we are too far ahead for rollback. ROLLBACK_MAX_FRAMES is the number of frames
  // we can receive for the opponent at one time and is our "look-ahead" limit
  // Example: finalized_frame = 100 means the last savestate we need is 101. We can then store
  // states 101 to 107 before running out of savestates. So 107 - 100 = 7. We need to make sure
  // we have enough inputs to finalize to not overflow the available states, so if our latest frame
  // is 101, we can't let frame 109 be created. 101 - 100 >= 109 - 100 - 7 : 1 >= 2 (false).
  // It has to work this way because we only have room to move our states forward by one for frame
  // 108
  s32 latest_remote_frame = slippi_netplay->GetSlippiLatestRemoteFrame(ROLLBACK_MAX_FRAMES);
  auto has_enough_new_inputs =
      latest_remote_frame - finalized_frame >= (frame - finalized_frame - ROLLBACK_MAX_FRAMES);
  if (!has_enough_new_inputs)
  {
    stall_frame_count++;
    if (stall_frame_count > 60 * 7)
    {
      // 7 second stall will disconnect game
      is_connection_stalled = true;
    }

    WARN_LOG_FMT(
        SLIPPI_ONLINE,
        "Halting for one frame due to rollback limit (frame: {} | latest: {} | finalized: {})...",
        frame, latest_remote_frame, finalized_frame);
    return true;
  }

  stall_frame_count = 0;

  s32 frame_time = 16683;
  s32 t1 = 10000;
  s32 t2 = (2 * frame_time) + t1;

  // 8/8/23: Removed the halting time sync logic after frame 120 in favor of emulation speed.
  // Hopefully less halts means less dropped inputs.
  // Only skip once for a given frame because our time detection method doesn't take into
  // consideration waiting for a frame. Also it's less jarring and it happens often enough that it
  // will smoothly get to the right place
  // 9/18/23: Brought back frame skips when behind in the other location
  auto is_time_sync_frame =
      frame % SLIPPI_ONLINE_LOCKSTEP_INTERVAL;  // Only time sync every 30 frames
  if (is_time_sync_frame == 0 && !is_currently_skipping && frame <= 120)
  {
    auto offset_us = slippi_netplay->CalcTimeOffsetUs();
    INFO_LOG_FMT(SLIPPI_ONLINE, "[Frame {}] Offset for skip is: {} us", frame, offset_us);

    // At the start of the game, let's make sure to sync perfectly, but after that let the slow
    // instance try to do more work before we stall

    // The decision to skip a frame only happens when we are already pretty far off ahead. The hope
    // is that this won't really be used much because the frame advance of the slow client along
    // with dynamic emulation speed will pick up the difference most of the time. But at some point
    // it's probably better to slow down...
    if (offset_us > (frame <= 120 ? t1 : t2))
    {
      is_currently_skipping = true;

      int max_skip_frames = frame <= 120 ? 5 : 1;  // On early frames, support skipping more frames
      frames_to_skip = ((offset_us - t1) / frame_time) + 1;
      frames_to_skip = frames_to_skip > max_skip_frames ? max_skip_frames :
                                                          frames_to_skip;  // Only skip 5 frames max

      WARN_LOG_FMT(SLIPPI_ONLINE,
                   "Halting on frame {} due to time sync. Offset: {} us. Frames: {}...", frame,
                   offset_us, frames_to_skip);
    }
  }

  // Handle the skipped frames
  if (frames_to_skip > 0)
  {
    // If ahead by 60% of a frame, stall. I opted to use 60% instead of half a frame
    // because I was worried about two systems continuously stalling for each other
    frames_to_skip = frames_to_skip - 1;
    return true;
  }

  is_currently_skipping = false;

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
  auto is_time_sync_frame =
      (frame % SLIPPI_ONLINE_LOCKSTEP_INTERVAL) == 0;  // Only time sync every 30 frames
  if (is_time_sync_frame)
  {
    auto offset_us = slippi_netplay->CalcTimeOffsetUs();

    // Dynamically adjust emulation speed in order to fine-tune time sync to reduce one sided
    // rollbacks even more
    // Modify emulation speed up to a max of 1% at 3 frames offset or more. Don't slow down the
    // front instance as much because we want to prioritize performance for the fast PC
    float deviation = 0;
    float max_slow_down_amount = 0.005f;
    float max_speed_up_amount = 0.01f;
    int slow_down_frame_window = 3;
    int speed_up_frame_window = 3;
    if (offset_us > -250 && offset_us < 8000)
    {
      // Do nothing, leave deviation at 0 for 100% emulation speed when ahead by 8 ms or less
    }
    else if (offset_us < 0)
    {
      // Here we are behind, so let's speed up our instance
      float frame_window_multiplier =
          std::min(-offset_us / (speed_up_frame_window * 16683.0f), 1.0f);
      deviation = frame_window_multiplier * max_speed_up_amount;
    }
    else
    {
      // Here we are ahead, so let's slow down our instance
      float frame_window_multiplier =
          std::min(offset_us / (slow_down_frame_window * 16683.0f), 1.0f);
      deviation = frame_window_multiplier * -max_slow_down_amount;
    }

    auto dynamic_emu_speed = 1.0f + deviation;
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED, dynamic_emu_speed);
    // SConfig::GetInstance().m_EmulationSpeed = dynamic_emu_speed;
    //  SConfig::GetInstance().m_EmulationSpeed = 0.97f; // used for testing

    INFO_LOG_FMT(SLIPPI_ONLINE, "[Frame {}] Offset for advance is: {} us. New speed: {:.4}%", frame,
                 offset_us, dynamic_emu_speed * 100.0f);

    s32 frame_time = 16683;
    s32 t1 = 10000;
    s32 t2 = frame_time + t1;

    // Count the number of times we're below a threshold we should easily be able to clear. This is
    // checked twice per second.
    fall_behind_counter += offset_us < -t1 ? 1 : 0;
    fall_far_behind_counter += offset_us < -t2 ? 1 : 0;

    bool is_slow = (offset_us < -t1 && fall_behind_counter > 50) ||
                   (offset_us < -t2 && fall_far_behind_counter > 15);
    if (is_slow && last_search.mode != SlippiMatchmaking::OnlinePlayMode::TEAMS)
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

    if (offset_us < -t2 && !is_currently_advancing)
    {
      is_currently_advancing = true;

      // On early frames, don't advance any frames. Let the stalling logic handle the initial sync

      int max_adv_frames = frame > 120 ? 3 : 0;
      frames_to_advance = ((-offset_us - t1) / frame_time) + 1;
      frames_to_advance = frames_to_advance > max_adv_frames ? max_adv_frames : frames_to_advance;

      WARN_LOG_FMT(SLIPPI_ONLINE,
                   "Advancing on frame {} due to time sync. Offset: {} us. Frames: {}...", frame,
                   offset_us, frames_to_advance);
    }
  }

  // Handle the skipped frames
  if (frames_to_advance > 0)
  {
    // Only advance once every 5 frames in an attempt to make the speed up feel smoother
    if (frame % 5 != 0)
    {
      return false;
    }

    frames_to_advance = frames_to_advance - 1;
    return true;
  }

  is_currently_advancing = false;
  return false;
}

void CEXISlippi::handleSendInputs(s32 frame, u8 delay, s32 checksum_frame, u32 checksum, u8* inputs)
{
  if (is_connection_stalled)
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

void CEXISlippi::prepareOpponentInputs(s32 frame, bool should_skip)
{
  m_read_queue.clear();

  u8 frame_result = 1;  // Indicates to continue frame

  auto state = slippi_netplay->GetSlippiConnectStatus();
  if (should_skip)
  {
    // Event though we are skipping an input, we still want to prepare the opponent inputs because
    // in the case where we get a stall on an advance frame, we need to keep the RXB inputs
    // populated for when the frame inputs are requested on a rollback
    frame_result = 2;
  }
  else if (state != SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED ||
           is_connection_stalled)
  {
    frame_result = 3;  // Indicates we have disconnected
  }
  else if (shouldAdvanceOnlineFrame(frame))
  {
    frame_result = 4;
  }

  m_read_queue.push_back(frame_result);  // Write out the control message value

  u8 remote_player_count = matchmaking->RemotePlayerCount();
  m_read_queue.push_back(remote_player_count);  // Indicate the number of remote players

  std::unique_ptr<SlippiRemotePadOutput> results[SLIPPI_REMOTE_PLAYER_MAX];

  for (int i = 0; i < remote_player_count; i++)
  {
    results[i] = slippi_netplay->GetSlippiRemotePad(i, ROLLBACK_MAX_FRAMES);
    // results[i] = slippi_netplay->GetFakePadOutput(frame);

    // INFO_LOG_FMT(SLIPPI_ONLINE, "Sending checksum values: [{}] %08x", results[i]->checksum_frame,
    // results[i]->checksum);
    appendWordToBuffer(&m_read_queue, static_cast<u32>(results[i]->checksum_frame));
    appendWordToBuffer(&m_read_queue, results[i]->checksum);
  }
  for (int i = remote_player_count; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    // Send dummy data for unused players
    appendWordToBuffer(&m_read_queue, 0);
    appendWordToBuffer(&m_read_queue, 0);
  }

  int offset[SLIPPI_REMOTE_PLAYER_MAX];

  int32_t latest_frame_read[SLIPPI_REMOTE_PLAYER_MAX]{};

  // Get pad data for each remote player and write each of their latest frame nums to the buf
  for (int i = 0; i < remote_player_count; i++)
  {
    // results[i] = slippi_netplay->GetSlippiRemotePad(i, ROLLBACK_MAX_FRAMES);
    // results[i] = slippi_netplay->GetFakePadOutput(frame);

    // determine offset from which to copy data
    offset[i] = (results[i]->latest_frame - frame) * SLIPPI_PAD_FULL_SIZE;
    offset[i] = offset[i] < 0 ? 0 : offset[i];

    // add latest frame we are transfering to begining of return buf
    int32_t latest_frame = results[i]->latest_frame;
    if (latest_frame > frame)
      latest_frame = frame;
    latest_frame_read[i] = latest_frame;
    appendWordToBuffer(&m_read_queue, static_cast<u32>(latest_frame));
    // INFO_LOG_FMT(SLIPPI_ONLINE, "Sending frame num {} for pIdx {} (offset: {})", latest_frame, i,
    // offset[i]);
  }
  // Send the current frame for any unused player slots.
  for (int i = remote_player_count; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    latest_frame_read[i] = frame;
    appendWordToBuffer(&m_read_queue, static_cast<u32>(frame));
  }

  s32* val = std::min_element(std::begin(latest_frame_read), std::end(latest_frame_read));
  appendWordToBuffer(&m_read_queue, static_cast<u32>(*val));

  // copy pad data over
  for (int i = 0; i < SLIPPI_REMOTE_PLAYER_MAX; i++)
  {
    std::vector<u8> tx;

    // Get pad data if this remote player exists
    if (i < remote_player_count && offset[i] < results[i]->data.size())
    {
      auto tx_start = results[i]->data.begin() + offset[i];
      auto tx_end = results[i]->data.end();
      tx.insert(tx.end(), tx_start, tx_end);
    }

    tx.resize(SLIPPI_PAD_FULL_SIZE * ROLLBACK_MAX_FRAMES, 0);

    m_read_queue.insert(m_read_queue.end(), tx.begin(), tx.end());
  }

  // ERROR_LOG_FMT(SLIPPI_ONLINE, "EXI: [{}] %X %X %X %X %X %X %X %X", latest_frame,
  // m_read_queue[5], m_read_queue[6], m_read_queue[7], m_read_queue[8], m_read_queue[9],
  // m_read_queue[10], m_read_queue[11], m_read_queue[12]);
}

void CEXISlippi::handleCaptureSavestate(u8* payload)
{
#ifndef IS_PLAYBACK
  if (isDisconnected())
    return;
#endif

  s32 frame = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];

  // u64 startTime = Common::Timer::GetTimeUs();

  // Grab an available savestate
  std::unique_ptr<SlippiSavestate> ss;
  if (!available_savestates.empty())
  {
    ss = std::move(available_savestates.back());
    available_savestates.pop_back();
  }
  else
  {
    // If there were no available savestates, use the oldest one
    auto it = active_savestates.begin();
    ss = std::move(it->second);
    active_savestates.erase(it->first);
  }

  // If there is already a savestate for this frame, remove it and add it to available
  if (active_savestates.count(frame))
  {
    available_savestates.push_back(std::move(active_savestates[frame]));
    active_savestates.erase(frame);
  }

  ss->Capture();
  active_savestates[frame] = std::move(ss);

  // u32 timeDiff = static_cast<u32>(Common::Timer::NowUs() - startTime);
  // INFO_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Captured savestate for frame {} in: {} ms", frame,
  //          ((double)timeDiff) / 1000);
}

void CEXISlippi::handleLoadSavestate(u8* payload)
{
  s32 frame = payload[0] << 24 | payload[1] << 16 | payload[2] << 8 | payload[3];
  u32* preserve_arr = (u32*)(&payload[4]);

  if (!active_savestates.count(frame))
  {
    // This savestate does not exist... uhhh? What do we do?
    ERROR_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Savestate for frame {} does not exist.", frame);
    return;
  }

  // u64 startTime = Common::Timer::GetTimeUs();

  // Fetch preservation blocks
  std::vector<SlippiSavestate::PreserveBlock> blocks;

  // Get preservation blocks
  int idx = 0;
  while (Common::swap32(preserve_arr[idx]) != 0)
  {
    SlippiSavestate::PreserveBlock p = {Common::swap32(preserve_arr[idx]),
                                        Common::swap32(preserve_arr[idx + 1])};
    blocks.push_back(p);
    idx += 2;
  }

  // Load savestate
  active_savestates[frame]->Load(blocks);

  // Move all active savestates to available
  for (auto it = active_savestates.begin(); it != active_savestates.end(); ++it)
  {
    available_savestates.push_back(std::move(it->second));
  }

  active_savestates.clear();

  // u32 timeDiff = static_cast<u32>(Common::Timer::NowUs() - startTime);
  // INFO_LOG_FMT(SLIPPI_ONLINE, "SLIPPI ONLINE: Loaded savestate for frame {} in: %f ms", frame,
  //          ((double)timeDiff) / 1000);
}

void CEXISlippi::startFindMatch(u8* payload)
{
  SlippiMatchmaking::MatchSearchSettings search;
  search.mode = (SlippiMatchmaking::OnlinePlayMode)payload[0];

  std::string shift_jis_code;
  shift_jis_code.insert(shift_jis_code.begin(), &payload[1], &payload[1] + 18);
  shift_jis_code.erase(std::find(shift_jis_code.begin(), shift_jis_code.end(), 0x00),
                       shift_jis_code.end());

  // Log the direct code to file.
  {
    // Make sure to convert to UTF8, otherwise json library will fail when
    // calling dump().
    std::string utf8_code = SHIFTJISToUTF8(shift_jis_code);
    if (search.mode == SlippiMatchmaking::DIRECT)
    {
      direct_codes->AddOrUpdateCode(utf8_code);
    }
    else if (search.mode == SlippiMatchmaking::TEAMS)
    {
      teams_codes->AddOrUpdateCode(utf8_code);
    }
  }

  // TODO: Make this work so we dont have to pass shiftJis to mm server
  // search.connect_code = SHIFTJISToUTF8(shift_jis_code).c_str();
  search.connect_code = shift_jis_code;

  // Store this search so we know what was queued for
  last_search = search;

  // While we do have another condition that checks characters after being connected, it's nice to
  // give someone an early error before they even queue so that they wont enter the queue and make
  // someone else get force removed from queue and have to requeue
  if (SlippiMatchmaking::IsFixedRulesMode(search.mode))
  {
    // Character check
    if (local_selections.character_id >= 26)
    {
      forced_error = "The character you selected is not allowed in this mode";
      return;
    }

    // Stage check
    if (local_selections.is_stage_selected &&
        std::find(allowed_stages.begin(), allowed_stages.end(), local_selections.stage_id) ==
            allowed_stages.end())
    {
      forced_error = "The stage being requested is not allowed in this mode";
      return;
    }
  }
  else if (search.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS)
  {
    // Some special handling for teams since it is being heavily used for unranked
    if (local_selections.character_id >= 26 &&
        SConfig::GetSlippiConfig().melee_version != Melee::Version::MEX)
    {
      forced_error = "The character you selected is not allowed in this mode";
      return;
    }
  }

#ifndef LOCAL_TESTING
  if (!is_enet_initialized)
  {
    // Initialize enet
    auto res = enet_initialize();
    if (res < 0)
      ERROR_LOG_FMT(SLIPPI_ONLINE, "Failed to initialize enet res: {}", res);

    is_enet_initialized = true;
  }

  matchmaking->FindMatch(search);
#endif
}

bool CEXISlippi::doesTagMatchInput(u8* input, u8 input_len, std::string tag)
{
  auto jis_tag = UTF8ToSHIFTJIS(tag);

  // Check if this tag matches what has been input so far
  bool is_match = true;
  for (int i = 0; i < input_len; i++)
  {
    // ERROR_LOG_FMT(SLIPPI_ONLINE, "Entered: %X%X. History: %X%X", input[i * 3], input[i * 3 + 1],
    // (u8)jis_tag[i * 2], (u8)jis_tag[i * 2 + 1]);
    if (input[i * 3] != (u8)jis_tag[i * 2] || input[i * 3 + 1] != (u8)jis_tag[i * 2 + 1])
    {
      is_match = false;
      break;
    }
  }

  return is_match;
}

void CEXISlippi::handleNameEntryLoad(u8* payload)
{
  u8 input_len = payload[24];
  u32 initial_idx = payload[25] << 24 | payload[26] << 16 | payload[27] << 8 | payload[28];
  u8 scroll_direction = payload[29];
  u8 curr_mode = payload[30];

  auto code_history = direct_codes.get();
  if (curr_mode == SlippiMatchmaking::TEAMS)
  {
    code_history = teams_codes.get();
  }

  // Adjust index
  u32 curr_idx = initial_idx;
  if (scroll_direction == 1)
  {
    curr_idx++;
  }
  else if (scroll_direction == 2)
  {
    curr_idx = curr_idx > 0 ? curr_idx - 1 : curr_idx;
  }
  else if (scroll_direction == 3)
  {
    curr_idx = 0;
  }

  // Scroll to next tag that
  std::string tag_at_idx = "1";
  while (curr_idx >= 0 && curr_idx < static_cast<u32>(code_history->length()))
  {
    tag_at_idx = code_history->get(curr_idx);

    // Break if we have found a tag that matches
    if (doesTagMatchInput(payload, input_len, tag_at_idx))
      break;

    curr_idx = scroll_direction == 2 ? curr_idx - 1 : curr_idx + 1;
  }

  // INFO_LOG_FMT(SLIPPI_ONLINE, "Idx: {}, InitIdx: {}, Scroll: {}. Len: {}", curr_idx,
  // initial_idx,
  //          scroll_direction, input_len);

  tag_at_idx = code_history->get(curr_idx);
  if (tag_at_idx == "1")
  {
    // If we failed to find a tag at the current index, try the initial index again.
    // If the initial index matches the filter, preserve that suggestion. Without
    // this logic, the suggestion would get cleared
    auto initial_tag = code_history->get(initial_idx);
    if (doesTagMatchInput(payload, input_len, initial_tag))
    {
      tag_at_idx = initial_tag;
      curr_idx = initial_idx;
    }
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "Retrieved tag: {}", tag_at_idx.c_str());
  std::string jis_code;
  m_read_queue.clear();

  if (tag_at_idx == "1")
  {
    m_read_queue.push_back(0);
    m_read_queue.insert(m_read_queue.end(), payload, payload + 3 * input_len);
    m_read_queue.insert(m_read_queue.end(), 3 * (8 - input_len), 0);
    m_read_queue.push_back(input_len);
    appendWordToBuffer(&m_read_queue, initial_idx);
    return;
  }

  // Indicate we have a suggestion
  m_read_queue.push_back(1);

  // Convert to tag to shift jis and write to response
  jis_code = UTF8ToSHIFTJIS(tag_at_idx);

  // Write out connect code into buffer, injection null terminator after each letter
  for (int i = 0; i < 8; i++)
  {
    for (int j = i * 2; j < i * 2 + 2; j++)
    {
      m_read_queue.push_back(j < jis_code.length() ? jis_code[j] : 0);
    }

    m_read_queue.push_back(0x0);
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "New Idx: {}. Jis Code length: {}", curr_idx,
               static_cast<u8>(jis_code.length() / 2));

  // Write length of tag
  m_read_queue.push_back(static_cast<u8>(jis_code.length() / 2));
  appendWordToBuffer(&m_read_queue, curr_idx);
}

void CEXISlippi::prepareOnlineMatchState()
{
  // This match block is a VS match with P1 Red Falco vs P2 Red Bowser vs P3 Young Link vs P4 Young
  // Link on Battlefield. The proper values will be overwritten
  static std::vector<u8> online_match_block = {
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

  auto error_state = SlippiMatchmaking::ProcessState::ERROR_ENCOUNTERED;

  SlippiMatchmaking::ProcessState mm_state =
      !forced_error.empty() ? error_state : matchmaking->GetMatchmakeState();

#ifdef LOCAL_TESTING
  if (local_selections.is_character_selected || is_local_connected)
  {
    mm_state = SlippiMatchmaking::ProcessState::CONNECTION_SUCCESS;
    is_local_connected = true;
  }
#endif

  m_read_queue.push_back(mm_state);  // Matchmaking State

  u8 local_player_ready = local_selections.is_character_selected;
  u8 remote_players_ready = 0;

  auto user_info = user->GetUserInfo();

  if (mm_state == SlippiMatchmaking::ProcessState::CONNECTION_SUCCESS)
  {
    m_local_player_idx = matchmaking->LocalPlayerIndex();

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
      recent_mm_result = matchmaking->GetMatchmakeResult();
      allowed_stages = recent_mm_result.stages;
      if (allowed_stages.empty())
      {
        allowed_stages = {
            0x2,   // FoD
            0x3,   // Pokemon
            0x8,   // Yoshi's Story
            0x1C,  // Dream Land
            0x1F,  // Battlefield
            0x20,  // Final Destination
        };
      }

      // Clear stage pool so that when we call getRandomStage it will use full list
      stage_pool.clear();
      local_selections.stage_id = getRandomStage();
      slippi_netplay->SetMatchSelections(local_selections);
    }

#ifdef LOCAL_TESTING
    bool is_connected = true;
#else
    auto status = slippi_netplay->GetSlippiConnectStatus();
    bool is_connected =
        status == SlippiNetplayClient::SlippiConnectStatus::NET_CONNECT_STATUS_CONNECTED;
#endif

    if (is_connected)
    {
      auto match_info = slippi_netplay->GetMatchInfo();
#ifdef LOCAL_TESTING
      remote_players_ready = true;
#else
      remote_players_ready = 1;
      u8 remote_player_count = matchmaking->RemotePlayerCount();
      for (int i = 0; i < remote_player_count; i++)
      {
        if (!match_info->remote_player_selections[i].is_character_selected)
        {
          remote_players_ready = 0;
        }
      }

      if (remote_player_count == 1)
      {
        auto is_decider = slippi_netplay->IsDecider();
        m_local_player_idx = is_decider ? 0 : 1;
        m_remote_player_idx = is_decider ? 1 : 0;
      }
#endif
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
      slprs_exi_device_start_new_reporter_session(slprs_exi_device_ptr);
      is_play_session_active = true;
    }
  }
  else
  {
    slippi_netplay = nullptr;
  }

  u32 rng_offset = 0;
  std::string local_player_name = "";
  std::string opp_name = "";
  std::string p1_ame = "";
  std::string p2_name = "";
  u8 chat_message_id = 0;
  u8 chat_message_player_idx = 0;
  u8 sent_chat_message_id = 0;

#ifdef LOCAL_TESTING
  local_player_idx = 0;
  sent_chat_message_id = local_chat_message_id;
  chat_message_player_idx = 0;
  local_chat_message_id = 0;
  // in CSS p1 is always current player and p2 is opponent
  local_player_name = p1_ame = user_info.display_name;
  opp_name = p2_name = "Player 2";
#endif

  SlippiDesyncRecoveryResp desync_recovery;
  if (slippi_netplay)
  {
    desync_recovery = slippi_netplay->GetDesyncRecoveryState();
  }

  // If we have an active desync recovery and haven't received the opponent's state, wait
  if (desync_recovery.is_recovering && desync_recovery.is_waiting)
  {
    remote_players_ready = 0;
  }

  if (desync_recovery.is_error)
  {
    // If desync recovery failed, just disconnect connection. Hopefully this will almost never
    // happen
    handleConnectionCleanup();
    prepareOnlineMatchState();  // run again with new state
    return;
  }

  m_read_queue.push_back(local_player_ready);    // Local player ready
  m_read_queue.push_back(remote_players_ready);  // Remote players ready
  m_read_queue.push_back(m_local_player_idx);    // Local player index
  m_read_queue.push_back(m_remote_player_idx);   // Remote player index

  // Set chat message if any
  if (slippi_netplay)
  {
    auto is_single_mode = matchmaking && matchmaking->RemotePlayerCount() == 1;
    bool is_chat_enabled = isSlippiChatEnabled();
    sent_chat_message_id = slippi_netplay->GetSlippiRemoteSentChatMessage(is_chat_enabled);
    // Prevent processing a message in the same frame
    if (sent_chat_message_id <= 0)
    {
      auto remote_message_selection = slippi_netplay->GetSlippiRemoteChatMessage(is_chat_enabled);
      chat_message_id = remote_message_selection.message_id;
      chat_message_player_idx = remote_message_selection.player_idx;
      if (chat_message_id == SlippiPremadeText::CHAT_MSG_CHAT_DISABLED && !is_single_mode)
      {
        // Clear remote chat messages if we are on teams and the player has chat disabled.
        // Could also be handled on SlippiNetplay if the instance had acccess to the current
        // connection mode
        chat_message_id = chat_message_player_idx = 0;
      }
    }
    else
    {
      chat_message_player_idx = m_local_player_idx;
    }

    if (is_single_mode || !matchmaking)
    {
      chat_message_player_idx = sent_chat_message_id > 0 ? m_local_player_idx : m_remote_player_idx;
    }
    // in CSS p1 is always current player and p2 is opponent
    local_player_name = p1_ame = user_info.display_name;
  }

  std::vector<u8> left_team_players = {};
  std::vector<u8> right_team_players = {};

  if (local_player_ready && remote_players_ready)
  {
    auto is_decider = slippi_netplay->IsDecider();
    u8 remote_player_count = matchmaking->RemotePlayerCount();
    auto match_info = slippi_netplay->GetMatchInfo();
    SlippiPlayerSelections lps = match_info->local_player_selections;
    auto rps = match_info->remote_player_selections;

#ifdef LOCAL_TESTING
    lps.player_idx = 0;

    // By default Local testing for teams is against
    // 1 RED TEAM Falco
    // 2 BLUE TEAM Falco
    for (int i = 0; i <= SLIPPI_REMOTE_PLAYER_MAX; i++)
    {
      if (i == 0)
      {
        rps[i].character_color = 1;
        rps[i].team_id = 0;
      }
      else
      {
        rps[i].character_color = 2;
        rps[i].team_id = 1;
      }

      rps[i].character_id = 0x14;
      rps[i].player_idx = i + 1;
      rps[i].is_character_selected = true;
    }

    remote_player_count = last_search.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS ? 3 : 1;

    opp_name = std::string("Player");
#endif

    // Check if someone is picking dumb characters in non-direct
    auto local_char_ok = lps.character_id < 26;
    auto remote_char_ok = true;
    INFO_LOG_FMT(SLIPPI_ONLINE, "remote_player_count: {}", remote_player_count);
    for (int i = 0; i < remote_player_count; i++)
    {
      if (rps[i].character_id >= 26)
        remote_char_ok = false;
    }

    // TODO: Ideally remote_player_selections would just include everyone including the local player
    // TODO: Would also simplify some logic in the Netplay class

    // Here we are storing pointers to the player selections. That means that we can technically
    // modify the values from here, which is probably not the cleanest thing since they're coming
    // from the netplay class. Unfortunately, I think it might be required for the overwrite stuff
    // to work correctly though, maybe on a tiebreak in ranked?
    std::vector<SlippiPlayerSelections*> ordered_selections(remote_player_count + 1);
    ordered_selections[lps.player_idx] = &lps;
    for (int i = 0; i < remote_player_count; i++)
    {
      ordered_selections[rps[i].player_idx] = &rps[i];
    }

    // Overwrite selections
    for (int i = 0; i < overwrite_selections.size(); i++)
    {
      const auto& ow = overwrite_selections[i];

      ordered_selections[i]->character_id = ow.character_id;
      ordered_selections[i]->character_color = ow.character_color;
      ordered_selections[i]->stage_id = ow.stage_id;
    }

    // Overwrite stage information. Make sure everyone loads the same stage
    u16 stage_id = 0x1F;  // Default to battlefield if there was no selection
    for (const auto& selections : ordered_selections)
    {
      if (!selections->is_stage_selected)
        continue;

      // Stage selected by this player, use that selection
      stage_id = selections->stage_id;
      break;
    }

    if (SlippiMatchmaking::IsFixedRulesMode(last_search.mode))
    {
      // If we enter one of these conditions, someone is doing something bad, clear the lobby
      if (!local_char_ok)
      {
        handleConnectionCleanup();
        forced_error = "The character you selected is not allowed in this mode";
        prepareOnlineMatchState();
        return;
      }

      if (!remote_char_ok)
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }

      if (std::find(allowed_stages.begin(), allowed_stages.end(), stage_id) == allowed_stages.end())
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }
    }
    else if (last_search.mode == SlippiMatchmaking::OnlinePlayMode::TEAMS)
    {
      auto is_MEX = SConfig::GetSlippiConfig().melee_version == Melee::Version::MEX;

      if (!local_char_ok && !is_MEX)
      {
        handleConnectionCleanup();
        forced_error = "The character you selected is not allowed in this mode";
        prepareOnlineMatchState();
        return;
      }

      if (!remote_char_ok && !is_MEX)
      {
        handleConnectionCleanup();
        prepareOnlineMatchState();
        return;
      }
    }

    // Set rng offset
    rng_offset = is_decider ? lps.rng_offset : rps[0].rng_offset;
    INFO_LOG_FMT(SLIPPI_ONLINE, "Rng Offset: {:#x}", rng_offset);

    // Check if everyone is the same color
    auto color = ordered_selections[0]->team_id;
    bool are_all_same_team = true;
    for (const auto& s : ordered_selections)
    {
      if (s->team_id != color)
        are_all_same_team = false;
    }

    // Choose random team assignments
    // Previously there was a bug here where the shuffle was not consistent across platforms given
    // the same seed, this would cause desyncs during cross platform play (different teams). Got
    // around this by no longer using the shuffle function...
    std::vector<std::vector<u8>> team_assignment_permutations = {
        {0, 0, 1, 1}, {1, 1, 0, 0}, {0, 1, 1, 0}, {1, 0, 0, 1}, {0, 1, 0, 1}, {1, 0, 1, 0},
    };
    auto team_assignments =
        team_assignment_permutations[rng_offset % team_assignment_permutations.size()];

    // Overwrite player character choices
    for (auto& s : ordered_selections)
    {
      if (!s->is_character_selected)
      {
        continue;
      }

      auto team_id = s->team_id;
      if (are_all_same_team)
      {
        // Overwrite team_id. Color is overwritten by ASM
        team_id = team_assignments[s->player_idx];
      }

      // Overwrite player character
      online_match_block[0x60 + (s->player_idx) * 0x24] = s->character_id;
      online_match_block[0x63 + (s->player_idx) * 0x24] = s->character_color;
      online_match_block[0x67 + (s->player_idx) * 0x24] = 0;
      online_match_block[0x69 + (s->player_idx) * 0x24] = team_id;
    }

    // Handle Singles/Teams specific logic
    if (remote_player_count <= 2)
    {
      online_match_block[0x8] = 0;  // is Teams = false

      // Set p3/p4 player type to none
      online_match_block[0x61 + 2 * 0x24] = 3;
      online_match_block[0x61 + 3 * 0x24] = 3;

      // Make one character lighter if same character, same color
      bool is_sheik_vs_zelda = lps.character_id == 0x12 && rps[0].character_id == 0x13 ||
                               lps.character_id == 0x13 && rps[0].character_id == 0x12;
      bool char_match = lps.character_id == rps[0].character_id || is_sheik_vs_zelda;
      bool color_match = lps.character_color == rps[0].character_color;

      online_match_block[0x67 + 0x24] = char_match && color_match ? 1 : 0;
    }
    else
    {
      online_match_block[0x8] = 1;  // is Teams = true

      // Set p3/p4 player type to human
      online_match_block[0x61 + 2 * 0x24] = 0;
      online_match_block[0x61 + 3 * 0x24] = 0;
    }

    u16* stage = (u16*)&online_match_block[0xE];
    *stage = Common::swap16(stage_id);

    // Turn pause off in unranked/ranked, on in other modes
    auto pause_allowed = last_search.mode == SlippiMatchmaking::OnlinePlayMode::DIRECT;
    u8* game_bit_field3 = static_cast<u8*>(&online_match_block[2]);
    *game_bit_field3 = pause_allowed ? *game_bit_field3 & 0xF7 : *game_bit_field3 | 0x8;
    //*game_bit_field3 = *game_bit_field3 | 0x8;

    // Group players into left/right side for team splash screen display
    for (int i = 0; i < 4; i++)
    {
      int team_id = online_match_block[0x69 + i * 0x24];
      if (team_id == lps.team_id)
        left_team_players.push_back(i);
      else
        right_team_players.push_back(i);
    }
    int left_team_size = static_cast<int>(left_team_players.size());
    int right_team_size = static_cast<int>(right_team_players.size());
    left_team_players.resize(4, 0);
    right_team_players.resize(4, 0);
    left_team_players[3] = static_cast<u8>(left_team_size);
    right_team_players[3] = static_cast<u8>(right_team_size);

    // Handle desync recovery. The default values in desync_recovery.state are 480 seconds (8 min
    // timer) and 4-stock/0 percent damage for the fighters. That means if we are not in a desync
    // recovery state, the state of the timer and fighters will be restored to the defaults
    u32* seconds_remaining = reinterpret_cast<u32*>(&online_match_block[0x10]);
    *seconds_remaining = Common::swap32(desync_recovery.state.seconds_remaining);

    for (int i = 0; i < 4; i++)
    {
      online_match_block[0x62 + i * 0x24] = desync_recovery.state.fighters[i].stocks_remaining;

      u16* current_health = reinterpret_cast<u16*>(&online_match_block[0x70 + i * 0x24]);
      *current_health = Common::swap16(desync_recovery.state.fighters[i].current_health);
    }
  }

  // Add rng offset to output
  appendWordToBuffer(&m_read_queue, rng_offset);

  // Add delay frames to output
  m_read_queue.push_back(static_cast<u8>(Config::Get(Config::SLIPPI_ONLINE_DELAY)));

  // Add chat messages id
  m_read_queue.push_back(static_cast<u8>(sent_chat_message_id));
  m_read_queue.push_back(static_cast<u8>(chat_message_id));
  m_read_queue.push_back(static_cast<u8>(chat_message_player_idx));

  // Add player groupings for VS splash screen
  left_team_players.resize(4, 0);
  right_team_players.resize(4, 0);
  m_read_queue.insert(m_read_queue.end(), left_team_players.begin(), left_team_players.end());
  m_read_queue.insert(m_read_queue.end(), right_team_players.begin(), right_team_players.end());

  // Add names to output
  // Always send static local player name
  local_player_name = ConvertStringForGame(local_player_name, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), local_player_name.begin(), local_player_name.end());

#ifdef LOCAL_TESTING
  std::string default_names[] = {"Player 1", "Player 2", "Player 3", "Player 4"};
#endif

  for (int i = 0; i < 4; i++)
  {
    std::string name = matchmaking->GetPlayerName(i);
#ifdef LOCAL_TESTING
    name = default_names[i];
#endif
    name = ConvertStringForGame(name, MAX_NAME_LENGTH);
    m_read_queue.insert(m_read_queue.end(), name.begin(), name.end());
  }

  // Create the opponent string using the names of all players on opposing teams
  std::vector<std::string> opponent_names = {};
  if (matchmaking->RemotePlayerCount() == 1)
  {
    opponent_names.push_back(matchmaking->GetPlayerName(m_remote_player_idx));
  }
  else
  {
    int team_idx = online_match_block[0x69 + m_local_player_idx * 0x24];
    for (int i = 0; i < 4; i++)
    {
      if (m_local_player_idx == i || online_match_block[0x69 + i * 0x24] == team_idx)
        continue;

      opponent_names.push_back(matchmaking->GetPlayerName(i));
    }
  }

  int num_opponents = opponent_names.size() == 0 ? 1 : static_cast<int>(opponent_names.size());
  auto chars_per_ame = (MAX_NAME_LENGTH - (num_opponents - 1)) / num_opponents;
  std::string opp_text = "";
  for (auto& name : opponent_names)
  {
    if (opp_text != "")
      opp_text += "/";

    opp_text += TruncateLengthChar(name, chars_per_ame);
  }

  opp_name = ConvertStringForGame(opp_text, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), opp_name.begin(), opp_name.end());

#ifdef LOCAL_TESTING
  std::string default_connect_codes[] = {"PLYR#001", "PLYR#002", "PLYR#003", "PLYR#004"};
#endif

  auto player_info = matchmaking->GetPlayerInfo();
  for (int i = 0; i < 4; i++)
  {
    std::string connect_code = i < player_info.size() ? player_info[i].connect_code : "";
#ifdef LOCAL_TESTING
    connect_code = default_connect_codes[i];
#endif
    connect_code = ConvertConnectCodeForGame(connect_code);
    m_read_queue.insert(m_read_queue.end(), connect_code.begin(), connect_code.end());
  }

#ifdef LOCAL_TESTING
  std::string default_uids[] = {"l6dqv4dp38a5ho6z1sue2wx2adlp", "jpvducykgbawuehrjlfbu2qud1nv",
                                "k0336d0tg3mgcdtaukpkf9jtf2k8", "v8tpb6uj9xil6e33od6mlot4fvdt"};
#endif

  for (int i = 0; i < 4; i++)
  {
    // UIDs are 28 characters + 1 null terminator
    std::string uid = i < player_info.size() ? player_info[i].uid : "";
#ifdef LOCAL_TESTING
    uid = default_uids[i];
#endif
    uid.resize(29);  // ensure a null terminator at the end
    m_read_queue.insert(m_read_queue.end(), uid.begin(), uid.end());
  }

  // Add error message if there is one
  auto error_str = !forced_error.empty() ? forced_error : matchmaking->GetErrorMessage();
  error_str = ConvertStringForGame(error_str, 120);
  m_read_queue.insert(m_read_queue.end(), error_str.begin(), error_str.end());

  // Add the match struct block to output
  m_read_queue.insert(m_read_queue.end(), online_match_block.begin(), online_match_block.end());

  // Add match id to output
  std::string match_id = recent_mm_result.id;
  match_id.resize(51);
  m_read_queue.insert(m_read_queue.end(), match_id.begin(), match_id.end());
}

u16 CEXISlippi::getRandomStage()
{
  static u16 selected_stage;

  // Reset stage pool if it's empty
  if (stage_pool.empty())
    stage_pool.insert(stage_pool.end(), allowed_stages.begin(), allowed_stages.end());

  // Get random stage
  int rand_idx = generator() % stage_pool.size();
  selected_stage = stage_pool[rand_idx];

  // Remove last selection from stage pool
  stage_pool.erase(stage_pool.begin() + rand_idx);

  return selected_stage;
}

void CEXISlippi::setMatchSelections(u8* payload)
{
  SlippiPlayerSelections s;

  s.team_id = payload[0];
  s.character_id = payload[1];
  s.character_color = payload[2];
  s.is_character_selected = payload[3];

  s.stage_id = Common::swap16(&payload[4]);
  u8 stage_select_option = payload[6];
  // u8 online_mode = payload[7];

  s.is_stage_selected = stage_select_option == 1 || stage_select_option == 3;
  if (stage_select_option == 3)
  {
    // If stage requested is random, select a random stage
    s.stage_id = getRandomStage();
  }

  INFO_LOG_FMT(SLIPPI, "LPS set char: {}, iSS: {}, {}, stage: {}, team: {}",
               s.is_character_selected, stage_select_option, s.is_stage_selected, s.stage_id,
               s.team_id);

  s.rng_offset = generator() % 0xFFFF;

  // Merge these selections
  local_selections.Merge(s);

  if (slippi_netplay)
  {
    slippi_netplay->SetMatchSelections(local_selections);
  }
}

void CEXISlippi::prepareFileLength(u8* payload)
{
  m_read_queue.clear();

  std::string file_name((char*)&payload[0]);

  std::string contents;
  u32 size = game_file_loader->LoadFile(file_name, contents);

  INFO_LOG_FMT(SLIPPI, "Getting file size for: {} -> {}", file_name.c_str(), size);

  // Write size to output
  appendWordToBuffer(&m_read_queue, size);
}

void CEXISlippi::prepareFileLoad(u8* payload)
{
  m_read_queue.clear();

  std::string file_name((char*)&payload[0]);

  std::string contents;
  u32 size = game_file_loader->LoadFile(file_name, contents);
  std::vector<u8> buf(contents.begin(), contents.end());

  INFO_LOG_FMT(SLIPPI, "Writing file contents: {} -> {}", file_name.c_str(), size);

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

  // SLIPPITODO: do we still need this?
  // // Overwrite the instructions which load address pointing to codeset
  // PowerPC::HostWrite_U32(0x3DE00000 | (address >> 16),
  //                        0x80001f58);  // lis r15, 0xXXXX # top half of address
  // PowerPC::HostWrite_U32(0x61EF0000 | (address & 0xFFFF),
  //                        0x80001f5C);               // ori r15, r15, 0xXXXX # bottom half of
  //                        address
  // PowerPC::ppcState.iCache.Invalidate(0x80001f58);  // This should invalidate both instructions

  INFO_LOG_FMT(SLIPPI, "Preparing to write gecko codes at: {:#x}", address);

  m_read_queue.insert(m_read_queue.end(), gct.begin(), gct.end());
}

std::vector<u8> CEXISlippi::loadPremadeText(u8* payload)
{
  u8 text_id = payload[0];
  std::vector<u8> premade_text_data;
  auto spt = SlippiPremadeText();

  if (text_id >= SlippiPremadeText::SPT_CHAT_P1 && text_id <= SlippiPremadeText::SPT_CHAT_P4)
  {
    auto port = text_id - 1;
    std::string player_name;
    if (matchmaking)
      player_name = matchmaking->GetPlayerName(port);
#ifdef LOCAL_TESTING
    std::string default_names[] = {"Player 1", "lol u lost 2 dk", "Player 3", "Player 4"};
    player_name = default_names[port];
#endif

    u8 param_id = payload[1];

    for (auto it = spt.unsupportedStringMap.begin(); it != spt.unsupportedStringMap.end(); it++)
    {
      player_name = ReplaceAll(player_name.c_str(), it->second, "");  // Remove unsupported chars
      player_name = ReplaceAll(player_name.c_str(), it->first,
                               it->second);  // Remap delimiters for premade text
    }

    // Replaces spaces with premade text space
    player_name = ReplaceAll(player_name.c_str(), " ", "<S>");

    if (param_id == SlippiPremadeText::CHAT_MSG_CHAT_DISABLED)
    {
      return premade_text_data =
                 spt.GetPremadeTextData(SlippiPremadeText::SPT_CHAT_DISABLED, player_name.c_str());
    }
    auto chat_message = spt.premadeTextsParams.at(param_id);
    std::string param = ReplaceAll(chat_message.c_str(), " ", "<S>");
    premade_text_data = spt.GetPremadeTextData(text_id, player_name.c_str(), param.c_str());
  }
  else
  {
    premade_text_data = spt.GetPremadeTextData(text_id);
  }

  return premade_text_data;
}

void CEXISlippi::preparePremadeTextLength(u8* payload)
{
  std::vector<u8> premade_text_data = loadPremadeText(payload);

  m_read_queue.clear();
  // Write size to output
  appendWordToBuffer(&m_read_queue, static_cast<u32>(premade_text_data.size()));
}

void CEXISlippi::preparePremadeTextLoad(u8* payload)
{
  std::vector<u8> premade_text_data = loadPremadeText(payload);

  m_read_queue.clear();
  // Write data to output
  m_read_queue.insert(m_read_queue.end(), premade_text_data.begin(), premade_text_data.end());
}

bool CEXISlippi::isSlippiChatEnabled()
{
  auto chat_enabled_choice = Config::Get(Config::SLIPPI_ENABLE_QUICK_CHAT);
  bool res = true;
  switch (last_search.mode)
  {
  case SlippiMatchmaking::DIRECT:
    res =
        chat_enabled_choice == Slippi::Chat::ON || chat_enabled_choice == Slippi::Chat::DIRECT_ONLY;
    break;
  default:
    res = chat_enabled_choice == Slippi::Chat::ON;
    break;
  }
  return res;  // default is enabled
}

void CEXISlippi::handleChatMessage(u8* payload)
{
  if (!isSlippiChatEnabled())
    return;

  int message_id = payload[0];
  INFO_LOG_FMT(SLIPPI, "SLIPPI CHAT INPUT: {:#x}", message_id);

#ifdef LOCAL_TESTING
  local_chat_message_id = 11;
#endif

  if (slippi_netplay)
  {
    auto packet = std::make_unique<sf::Packet>();
    //		OSD::AddMessage("[Me]: "+ msg, OSD::Duration::VERY_LONG, OSD::Color::YELLOW);
    slippi_netplay->remote_sent_chat_message_id = message_id;
    slippi_netplay->WriteChatMessageToPacket(*packet, message_id,
                                             slippi_netplay->LocalPlayerPort());
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
  bool log_in_res = user->AttemptLogin();
  if (!log_in_res)
  {
    // no easy way to raise and lower the window
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

  auto is_logged_in = user->IsLoggedIn();
  auto user_info = user->GetUserInfo();

  u8 app_state = 0;
  if (is_logged_in)
  {
    // Check if we have the latest version, and if not, indicate we need to update
    version::Semver200_version latest_version(user_info.latest_version);
    version::Semver200_version current_version(Common::GetSemVerStr());

    app_state = latest_version > current_version ? 2 : 1;
  }

  m_read_queue.push_back(app_state);

  // Write player name (31 bytes)
  std::string player_name = ConvertStringForGame(user_info.display_name, MAX_NAME_LENGTH);
  m_read_queue.insert(m_read_queue.end(), player_name.begin(), player_name.end());

  // Write connect code (10 bytes)
  std::string connect_code = ConvertConnectCodeForGame(user_info.connect_code);
  m_read_queue.insert(m_read_queue.end(), connect_code.begin(), connect_code.end());
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
  local_selections.Reset();

  // Reset random stage pool
  stage_pool.clear();

  // Reset any forced errors
  forced_error.clear();

  // Reset any selection overwrites
  overwrite_selections.clear();

  // Reset play session
  is_play_session_active = false;

#ifdef LOCAL_TESTING
  is_local_connected = false;
#endif

  ERROR_LOG_FMT(SLIPPI_ONLINE, "Connection cleanup completed...");
}

void CEXISlippi::prepareNewSeed()
{
  m_read_queue.clear();

  u32 new_seed = generator() % 0xFFFFFFFF;

  appendWordToBuffer(&m_read_queue, new_seed);
}

void CEXISlippi::handleReportGame(const SlippiExiTypes::ReportGameQuery& query)
{
  std::string match_id = recent_mm_result.id;
  SlippiMatchmakingOnlinePlayMode online_mode =
      static_cast<SlippiMatchmakingOnlinePlayMode>(query.mode);
  u32 duration_frames = query.frame_length;
  u32 game_idx = query.game_idx;
  u32 tiebreak_idx = query.tiebreak_idx;
  s8 winner_idx = query.winner_idx;
  int stage_id = Common::FromBigEndian(*(u16*)&query.game_info_block[0xE]);
  u8 game_end_method = query.game_end_method;
  s8 lras_initiator = query.lras_initiator;

  ERROR_LOG_FMT(SLIPPI_ONLINE,
                "Mode: {} / {}, Frames: {}, GameIdx: {}, TiebreakIdx: {}, WinnerIdx: {}, StageId: "
                "{}, GameEndMethod: {}, "
                "LRASInitiator: {}",
                static_cast<u8>(online_mode), query.mode, duration_frames, game_idx, tiebreak_idx,
                winner_idx, stage_id, game_end_method, lras_initiator);

  auto user_info = user->GetUserInfo();

  // We pass `uid` and `playKey` here until the User side of things is
  // ported to Rust.
  uintptr_t game_report =
      slprs_game_report_create(user_info.uid.c_str(), user_info.play_key.c_str(), online_mode,
                               match_id.c_str(), duration_frames, game_idx, tiebreak_idx,
                               winner_idx, game_end_method, lras_initiator, stage_id);

  auto mm_players = recent_mm_result.players;

  for (auto i = 0; i < 4; ++i)
  {
    std::string uid = mm_players.size() > i ? mm_players[i].uid : "";
    u8 slot_type = query.players[i].slot_type;
    u8 stocks_remaining = query.players[i].stocks_remaining;
    float damage_done = query.players[i].damage_done;
    u8 char_id = query.game_info_block[0x60 + 0x24 * i];
    u8 color_id = query.game_info_block[0x63 + 0x24 * i];
    int starting_stocks = query.game_info_block[0x62 + 0x24 * i];
    int starting_percent = Common::FromBigEndian(*(u16*)&query.game_info_block[0x70 + 0x24 * i]);

    ERROR_LOG_FMT(SLIPPI_ONLINE,
                  "UID: {}, Port Type: {}, Stocks: {}, DamageDone: {}, CharId: {}, ColorId: {}, "
                  "StartStocks: {}, "
                  "StartPercent: {}",
                  uid.c_str(), slot_type, stocks_remaining, damage_done, char_id, color_id,
                  starting_stocks, starting_percent);

    uintptr_t player_report =
        slprs_player_report_create(uid.c_str(), slot_type, damage_done, stocks_remaining, char_id,
                                   color_id, starting_stocks, starting_percent);

    slprs_game_report_add_player_report(game_report, player_report);
  }

  // If ranked mode and the game ended with a quit out, this is either a desync or an interrupted
  // game, attempt to send synced values to opponents in order to restart the match where it was
  // left off
  if (online_mode == SlippiMatchmakingOnlinePlayMode::Ranked && game_end_method == 7)
  {
    SlippiSyncedGameState s;
    s.match_id = match_id;
    s.game_idx = game_idx;
    s.tiebreak_idx = tiebreak_idx;
    s.seconds_remaining = query.synced_timer;
    for (int i = 0; i < 4; i++)
    {
      s.fighters[i].stocks_remaining = query.players[i].synced_stocks_remaining;
      s.fighters[i].current_health = query.players[i].synced_current_health;
    }

    if (slippi_netplay)
      slippi_netplay->SendSyncedGameState(s);
  }

#ifndef LOCAL_TESTING
  slprs_exi_device_log_game_report(slprs_exi_device_ptr, game_report);
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
    s.is_character_selected = true;
    s.character_id = query.chars[i].char_id;
    s.character_color = query.chars[i].char_color_id;
    s.is_stage_selected = true;
    s.stage_id = query.stage_id;
    s.player_idx = i;

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

void CEXISlippi::handleCompleteSet(const SlippiExiTypes::ReportSetCompletionQuery& query)
{
  auto last_match_id = recent_mm_result.id;
  if (last_match_id.find("mode.ranked") != std::string::npos)
  {
    INFO_LOG_FMT(SLIPPI_ONLINE, "Reporting set completion: {}", last_match_id);

    slprs_exi_device_report_match_completion(slprs_exi_device_ptr, last_match_id.c_str(),
                                             query.end_mode);
  }
}

void CEXISlippi::handleGetPlayerSettings()
{
  m_read_queue.clear();

  SlippiExiTypes::GetPlayerSettingsResponse resp = {};

  std::vector<std::vector<std::string>> messages_by_player = {{}, {}, {}, {}};

  // These chat messages will be used when previewing messages
  auto user_chat_messages = user->GetUserChatMessages();
  if (user_chat_messages.size() == 16)
  {
    messages_by_player[0] = user_chat_messages;
  }

  // These chat messages will be set when we have an opponent. We load their and our messages
  auto player_info = matchmaking->GetPlayerInfo();
  for (auto& player : player_info)
  {
    messages_by_player[player.port - 1] = player.chat_messages;
  }

  for (int i = 0; i < 4; i++)
  {
    // If any of the users in the chat messages vector have a payload that is incorrect,
    // force that player to the default chat messages. A valid payload is 16 entries.
    if (messages_by_player[i].size() != 16)
    {
      messages_by_player[i] = user->GetDefaultChatMessages();
    }

    for (int j = 0; j < 16; j++)
    {
      auto str = ConvertStringForGame(messages_by_player[i][j], MAX_MESSAGE_LENGTH);
      sprintf(resp.settings[i].chat_messages[j], "%s", str.c_str());
    }
  }

  auto data_ptr = (u8*)&resp;
  m_read_queue.insert(m_read_queue.end(), data_ptr,
                      data_ptr + sizeof(SlippiExiTypes::GetPlayerSettingsResponse));
}

void CEXISlippi::DMAWrite(u32 _uAddr, u32 _uSize)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8* mem_ptr = memory.GetPointer(_uAddr);

  u32 buf_loc = 0;

  if (mem_ptr == nullptr)
  {
    ASSERT(Core::IsCPUThread());
    Core::CPUThreadGuard guard(system);
    NOTICE_LOG_FMT(SLIPPI, "DMA Write was passed an invalid address: {:x}", _uAddr);
    Dolphin_Debugger::PrintCallstack(guard, Common::Log::LogType::SLIPPI,
                                     Common::Log::LogLevel::LNOTICE);
    m_read_queue.clear();
    return;
  }

  u8 byte = mem_ptr[0];
  if (byte == CMD_RECEIVE_COMMANDS)
  {
    time(&game_start_time);  // Store game start time
    u8 receive_commands_len = mem_ptr[1];
    configureCommands(&mem_ptr[1], receive_commands_len);
    writeToFileAsync(&mem_ptr[0], receive_commands_len + 1, "create");
    buf_loc += receive_commands_len + 1;
    g_need_input_for_frame = true;
    SlippiSpectateServer::getInstance().startGame();
    SlippiSpectateServer::getInstance().write(&mem_ptr[0], receive_commands_len + 1);
    slprs_exi_device_reporter_push_replay_data(slprs_exi_device_ptr, &mem_ptr[0],
                                               receive_commands_len + 1);
  }

  if (byte == CMD_MENU_FRAME)
  {
    SlippiSpectateServer::getInstance().write(&mem_ptr[0], _uSize);
    g_need_input_for_frame = true;
  }

  INFO_LOG_FMT(EXPANSIONINTERFACE,
               "EXI SLIPPI DMAWrite: addr: {:#x} size: {}, buf_loc:[{} {} {} {} {}]", _uAddr,
               _uSize, mem_ptr[buf_loc], mem_ptr[buf_loc + 1], mem_ptr[buf_loc + 2],
               mem_ptr[buf_loc + 3], mem_ptr[buf_loc + 4]);

  u8 prev_command_byte = 0;

  while (buf_loc < _uSize)
  {
    byte = mem_ptr[buf_loc];
    if (!payloadSizes.count(byte))
    {
      // This should never happen. Do something else if it does?
      ERROR_LOG_FMT(SLIPPI, "EXI SLIPPI: Invalid command byte: {:#x}. Prev command: {:#x}", byte,
                    prev_command_byte);
      return;
    }

    u32 payload_len = payloadSizes[byte];
    switch (byte)
    {
    case CMD_RECEIVE_GAME_END:
      writeToFileAsync(&mem_ptr[buf_loc], payload_len + 1, "close");
      SlippiSpectateServer::getInstance().write(&mem_ptr[buf_loc], payload_len + 1);
      SlippiSpectateServer::getInstance().endGame();
      slprs_exi_device_reporter_push_replay_data(slprs_exi_device_ptr, &mem_ptr[buf_loc],
                                                 payload_len + 1);
      break;
    case CMD_PREPARE_REPLAY:
      prepareGameInfo(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_READ_FRAME:
      prepareFrameData(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_FRAME_BOOKEND:
      g_need_input_for_frame = true;
      writeToFileAsync(&mem_ptr[buf_loc], payload_len + 1, "");
      SlippiSpectateServer::getInstance().write(&mem_ptr[buf_loc], payload_len + 1);
      slprs_exi_device_reporter_push_replay_data(slprs_exi_device_ptr, &mem_ptr[buf_loc],
                                                 payload_len + 1);
      break;
    case CMD_IS_STOCK_STEAL:
      prepareIsStockSteal(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_IS_FILE_READY:
      prepareIsFileReady();
      break;
    case CMD_GET_GECKO_CODES:
      m_read_queue.clear();
      m_read_queue.insert(m_read_queue.begin(), gecko_list.begin(), gecko_list.end());
      break;
    case CMD_ONLINE_INPUTS:
      handleOnlineInputs(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_CAPTURE_SAVESTATE:
      handleCaptureSavestate(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_LOAD_SAVESTATE:
      handleLoadSavestate(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_GET_MATCH_STATE:
      prepareOnlineMatchState();
      break;
    case CMD_FIND_OPPONENT:
      startFindMatch(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_SET_MATCH_SELECTIONS:
      setMatchSelections(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_FILE_LENGTH:
      prepareFileLength(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_FETCH_CODE_SUGGESTION:
      handleNameEntryLoad(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_FILE_LOAD:
      prepareFileLoad(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_PREMADE_TEXT_LENGTH:
      preparePremadeTextLength(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_PREMADE_TEXT_LOAD:
      preparePremadeTextLoad(&mem_ptr[buf_loc + 1]);
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
      logMessageFromGame(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_SEND_CHAT_MESSAGE:
      handleChatMessage(&mem_ptr[buf_loc + 1]);
      break;
    case CMD_UPDATE:
      user->UpdateApp();
      break;
    case CMD_GET_NEW_SEED:
      prepareNewSeed();
      break;
    case CMD_REPORT_GAME:
      handleReportGame(SlippiExiTypes::Convert<SlippiExiTypes::ReportGameQuery>(&mem_ptr[buf_loc]));
      break;
    case CMD_GCT_LENGTH:
      prepareGctLength();
      break;
    case CMD_GCT_LOAD:
      prepareGctLoad(&mem_ptr[buf_loc + 1]);
      ConfigureJukebox();
      break;
    case CMD_GET_DELAY:
      prepareDelayResponse();
      break;
    case CMD_OVERWRITE_SELECTIONS:
      handleOverwriteSelections(
          SlippiExiTypes::Convert<SlippiExiTypes::OverwriteSelectionsQuery>(&mem_ptr[buf_loc]));
      break;
    case CMD_GP_FETCH_STEP:
      prepareGamePrepOppStep(
          SlippiExiTypes::Convert<SlippiExiTypes::GpFetchStepQuery>(&mem_ptr[buf_loc]));
      break;
    case CMD_GP_COMPLETE_STEP:
      handleGamePrepStepComplete(
          SlippiExiTypes::Convert<SlippiExiTypes::GpCompleteStepQuery>(&mem_ptr[buf_loc]));
      break;
    case CMD_REPORT_SET_COMPLETE:
      handleCompleteSet(
          SlippiExiTypes::Convert<SlippiExiTypes::ReportSetCompletionQuery>(&mem_ptr[buf_loc]));
      break;
    case CMD_GET_PLAYER_SETTINGS:
      handleGetPlayerSettings();
      break;
    case CMD_PLAY_MUSIC:
    {
      auto args = SlippiExiTypes::Convert<SlippiExiTypes::PlayMusicQuery>(&mem_ptr[buf_loc]);
      slprs_jukebox_start_song(slprs_exi_device_ptr, args.offset, args.size);
      break;
    }
    case CMD_STOP_MUSIC:
      slprs_jukebox_stop_music(slprs_exi_device_ptr);
      break;
    case CMD_CHANGE_MUSIC_VOLUME:
    {
      auto args =
          SlippiExiTypes::Convert<SlippiExiTypes::ChangeMusicVolumeQuery>(&mem_ptr[buf_loc]);
      slprs_jukebox_set_melee_music_volume(slprs_exi_device_ptr, args.volume);
      break;
    }
    default:
      writeToFileAsync(&mem_ptr[buf_loc], payload_len + 1, "");
      SlippiSpectateServer::getInstance().write(&mem_ptr[buf_loc], payload_len + 1);
      slprs_exi_device_reporter_push_replay_data(slprs_exi_device_ptr, &mem_ptr[buf_loc],
                                                 payload_len + 1);
      break;
    }

    prev_command_byte = byte;
    buf_loc += payload_len + 1;
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

  auto queue_addr = &m_read_queue[0];
  INFO_LOG_FMT(EXPANSIONINTERFACE,
               "EXI SLIPPI DMARead: addr: {:#x} size: {}, startResp: [{} {} {} {} {}]", addr, size,
               queue_addr[0], queue_addr[1], queue_addr[2], queue_addr[3], queue_addr[4]);

  // Copy buffer data to memory
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyToEmu(addr, queue_addr, size);
}

// Configures (or reconfigures) the Jukebox by calling over the C FFI boundary.
//
// This method can also be called, indirectly, from the Settings panel.
void CEXISlippi::ConfigureJukebox()
{
#ifndef IS_PLAYBACK
  // Exclusive WASAPI will prevent Jukebox from running on the main device, relegating it to another
  // device at random, so we will simply prevent Jukebox from running if the user is using WASAPI on
  // the default device (assuming they don't select the default device directly).
#ifdef _WIN32
  std::string backend = Config::Get(Config::MAIN_AUDIO_BACKEND);
  std::string audio_device = Config::Get(Config::MAIN_WASAPI_DEVICE);
  if (backend.find(BACKEND_WASAPI) != std::string::npos && audio_device == "default")
  {
    OSD::AddMessage(
        "Slippi Jukebox does not work with WASAPI on the default device. Music will not play.");
    return;
  }
#endif

  int dolphin_system_volume =
      Config::Get(Config::MAIN_AUDIO_MUTED) ? 0 : Config::Get(Config::MAIN_AUDIO_VOLUME);

  int dolphin_music_volume = Config::Get(Config::SLIPPI_JUKEBOX_VOLUME);

  slprs_exi_device_configure_jukebox(slprs_exi_device_ptr,
                                     Config::Get(Config::SLIPPI_ENABLE_JUKEBOX),
                                     dolphin_system_volume, dolphin_music_volume);
#endif
}

void CEXISlippi::UpdateJukeboxDolphinSystemVolume(int volume)
{
  slprs_jukebox_set_dolphin_system_volume(slprs_exi_device_ptr, volume);
}

void CEXISlippi::UpdateJukeboxDolphinMusicVolume(int volume)
{
  slprs_jukebox_set_dolphin_music_volume(slprs_exi_device_ptr, volume);
}

bool CEXISlippi::IsPresent() const
{
  return true;
}

void CEXISlippi::TransferByte(u8& byte)
{
}
}  // namespace ExpansionInterface
