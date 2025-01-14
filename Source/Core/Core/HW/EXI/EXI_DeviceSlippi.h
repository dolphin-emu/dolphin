// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/SPSCQueue.h"
#include "Core/Slippi/SlippiDirectCodes.h"
#include "Core/Slippi/SlippiExiTypes.h"
#include "Core/Slippi/SlippiGame.h"
#include "Core/Slippi/SlippiGameFileLoader.h"
#include "Core/Slippi/SlippiMatchmaking.h"
#include "Core/Slippi/SlippiNetplay.h"
#include "Core/Slippi/SlippiPlayback.h"
#include "Core/Slippi/SlippiReplayComm.h"
#include "Core/Slippi/SlippiSavestate.h"
#include "Core/Slippi/SlippiSpectate.h"
#include "Core/Slippi/SlippiUser.h"
#include "EXI_Device.h"

#define ROLLBACK_MAX_FRAMES 7
#define MAX_NAME_LENGTH 15
#define MAX_MESSAGE_LENGTH 25
#define CONNECT_CODE_LENGTH 8

namespace ExpansionInterface
{
// Emulated Slippi device used to receive and respond to in-game messages
class CEXISlippi : public IEXIDevice
{
public:
  CEXISlippi(Core::System& system, const std::string current_file_name);
  virtual ~CEXISlippi();

  void DMAWrite(u32 _uAddr, u32 _uSize) override;
  void DMARead(u32 addr, u32 size) override;

  void ConfigureJukebox();
  void UpdateJukeboxDolphinSystemVolume(int volume);
  void UpdateJukeboxDolphinMusicVolume(int volume);

  bool IsPresent() const override;

private:
  enum
  {
    CMD_UNKNOWN = 0x0,

    // Recording
    CMD_RECEIVE_COMMANDS = 0x35,
    CMD_RECEIVE_GAME_INFO = 0x36,
    CMD_RECEIVE_POST_FRAME_UPDATE = 0x38,
    CMD_RECEIVE_GAME_END = 0x39,
    CMD_FRAME_BOOKEND = 0x3C,
    CMD_MENU_FRAME = 0x3E,

    // Playback
    CMD_PREPARE_REPLAY = 0x75,
    CMD_READ_FRAME = 0x76,
    CMD_GET_LOCATION = 0x77,
    CMD_IS_FILE_READY = 0x88,
    CMD_IS_STOCK_STEAL = 0x89,
    CMD_GET_GECKO_CODES = 0x8A,

    // Online
    CMD_ONLINE_INPUTS = 0xB0,
    CMD_CAPTURE_SAVESTATE = 0xB1,
    CMD_LOAD_SAVESTATE = 0xB2,
    CMD_GET_MATCH_STATE = 0xB3,
    CMD_FIND_OPPONENT = 0xB4,
    CMD_SET_MATCH_SELECTIONS = 0xB5,
    CMD_OPEN_LOGIN = 0xB6,
    CMD_LOGOUT = 0xB7,
    CMD_UPDATE = 0xB8,
    CMD_GET_ONLINE_STATUS = 0xB9,
    CMD_CLEANUP_CONNECTION = 0xBA,
    CMD_SEND_CHAT_MESSAGE = 0xBB,
    CMD_GET_NEW_SEED = 0xBC,
    CMD_REPORT_GAME = 0xBD,
    CMD_FETCH_CODE_SUGGESTION = 0xBE,
    CMD_OVERWRITE_SELECTIONS = 0xBF,
    CMD_GP_COMPLETE_STEP = 0xC0,
    CMD_GP_FETCH_STEP = 0xC1,
    CMD_REPORT_SET_COMPLETE = 0xC2,
    CMD_GET_PLAYER_SETTINGS = 0xC3,

    // Misc
    CMD_LOG_MESSAGE = 0xD0,
    CMD_FILE_LENGTH = 0xD1,
    CMD_FILE_LOAD = 0xD2,
    CMD_GCT_LENGTH = 0xD3,
    CMD_GCT_LOAD = 0xD4,
    CMD_GET_DELAY = 0xD5,
    CMD_PLAY_MUSIC = 0xD6,
    CMD_STOP_MUSIC = 0xD7,
    CMD_CHANGE_MUSIC_VOLUME = 0xD8,
    CMD_PREMADE_TEXT_LENGTH = 0xE1,
    CMD_PREMADE_TEXT_LOAD = 0xE2,
  };

  enum
  {
    FRAME_RESP_WAIT = 0,
    FRAME_RESP_CONTINUE = 1,
    FRAME_RESP_TERMINATE = 2,
    FRAME_RESP_FASTFORWARD = 3,
  };

  std::unordered_map<u8, u32> payloadSizes = {
      // The actual size of this command will be sent in one byte
      // after the command is received. The other receive command IDs
      // and sizes will be received immediately following
      {CMD_RECEIVE_COMMANDS, 1},

      // The following are all commands used to play back a replay and
      // have fixed sizes unless otherwise specified
      {CMD_PREPARE_REPLAY, 0xFFFF},  // Variable size... will only work if by itself
      {CMD_READ_FRAME, 4},
      {CMD_IS_STOCK_STEAL, 5},
      {CMD_GET_LOCATION, 6},
      {CMD_IS_FILE_READY, 0},
      {CMD_GET_GECKO_CODES, 0},

      // The following are used for Slippi online and also have fixed sizes
      {CMD_ONLINE_INPUTS, 25},
      {CMD_CAPTURE_SAVESTATE, 32},
      {CMD_LOAD_SAVESTATE, 32},
      {CMD_GET_MATCH_STATE, 0},
      {CMD_FIND_OPPONENT, 19},
      {CMD_SET_MATCH_SELECTIONS, 8},
      {CMD_SEND_CHAT_MESSAGE, 2},
      {CMD_OPEN_LOGIN, 0},
      {CMD_LOGOUT, 0},
      {CMD_UPDATE, 0},
      {CMD_GET_ONLINE_STATUS, 0},
      {CMD_CLEANUP_CONNECTION, 0},
      {CMD_GET_NEW_SEED, 0},
      {CMD_REPORT_GAME, static_cast<u32>(sizeof(SlippiExiTypes::ReportGameQuery) - 1)},
      {CMD_FETCH_CODE_SUGGESTION, 31},
      {CMD_OVERWRITE_SELECTIONS,
       static_cast<u32>(sizeof(SlippiExiTypes::OverwriteSelectionsQuery) - 1)},
      {CMD_GP_COMPLETE_STEP, static_cast<u32>(sizeof(SlippiExiTypes::GpCompleteStepQuery) - 1)},
      {CMD_GP_FETCH_STEP, static_cast<u32>(sizeof(SlippiExiTypes::GpFetchStepQuery) - 1)},
      {CMD_REPORT_SET_COMPLETE,
       static_cast<u32>(sizeof(SlippiExiTypes::ReportSetCompletionQuery) - 1)},
      {CMD_GET_PLAYER_SETTINGS, 0},

      // Misc
      {CMD_LOG_MESSAGE, 0xFFFF},  // Variable size... will only work if by itself
      {CMD_FILE_LENGTH, 0x40},
      {CMD_FILE_LOAD, 0x40},
      {CMD_GCT_LENGTH, 0x0},
      {CMD_GCT_LOAD, 0x4},
      {CMD_GET_DELAY, 0x0},
      {CMD_PLAY_MUSIC, static_cast<u32>(sizeof(SlippiExiTypes::PlayMusicQuery) - 1)},
      {CMD_STOP_MUSIC, 0x0},
      {CMD_CHANGE_MUSIC_VOLUME,
       static_cast<u32>(sizeof(SlippiExiTypes::ChangeMusicVolumeQuery) - 1)},
      {CMD_PREMADE_TEXT_LENGTH, 0x2},
      {CMD_PREMADE_TEXT_LOAD, 0x2},
  };

  struct WriteMessage
  {
    std::vector<u8> data;
    std::string operation;
  };

  // A pointer to a "shadow" EXI Device that lives on the Rust side of things.
  // This should be cleaned up in any destructor!
  uintptr_t slprs_exi_device_ptr;

  // .slp File creation stuff
  u32 written_byte_count = 0;

  // vars for metadata generation
  time_t game_start_time;
  s32 last_frame;
  std::unordered_map<u8, std::unordered_map<u8, u32>> character_usage;

  void updateMetadataFields(u8* payload, u32 length);
  void configureCommands(u8* payload, u8 length);
  void writeToFileAsync(u8* payload, u32 length, std::string file_option);
  void writeToFile(std::unique_ptr<WriteMessage> msg);
  std::vector<u8> generateMetadata();
  void createNewFile();
  void closeFile();
  std::string generateFileName();
  bool checkFrameFullyFetched(s32 frame_idx);
  // bool shouldFFWFrame(s32 frame_idx);

  // std::ofstream log;

  File::IOFile m_file;
  std::vector<u8> m_payload;

  // online play stuff
  u16 getRandomStage();
  bool isDisconnected();
  void handleOnlineInputs(u8* payload);
  void prepareOpponentInputs(s32 frame, bool should_skip);
  void handleSendInputs(s32 frame, u8 delay, s32 checksum_frame, u32 checksum, u8* inputs);
  void handleCaptureSavestate(u8* payload);
  void handleLoadSavestate(u8* payload);
  void handleNameEntryLoad(u8* payload);
  void startFindMatch(u8* payload);
  void prepareOnlineMatchState();
  void setMatchSelections(u8* payload);
  bool shouldSkipOnlineFrame(s32 frame, s32 finalized_frame);
  bool shouldAdvanceOnlineFrame(s32 frame);
  bool opponentRunahead();
  void handleLogInRequest();
  void handleLogOutRequest();
  void prepareOnlineStatus();
  void handleConnectionCleanup();
  void prepareNewSeed();
  void handleReportGame(const SlippiExiTypes::ReportGameQuery& query);
  void handleOverwriteSelections(const SlippiExiTypes::OverwriteSelectionsQuery& query);
  void handleGamePrepStepComplete(const SlippiExiTypes::GpCompleteStepQuery& query);
  void prepareGamePrepOppStep(const SlippiExiTypes::GpFetchStepQuery& query);
  void handleCompleteSet(const SlippiExiTypes::ReportSetCompletionQuery& query);
  void handleGetPlayerSettings();

  // replay playback stuff
  void prepareGameInfo(u8* payload);
  void prepareGeckoList();
  void prepareCharacterFrameData(Slippi::FrameData* frame, u8 port, u8 is_follower);
  void prepareFrameData(u8* payload);
  void prepareIsStockSteal(u8* payload);
  void prepareIsFileReady();

  // misc stuff
  bool isSlippiChatEnabled();
  void handleChatMessage(u8* payload);
  void logMessageFromGame(u8* payload);
  void prepareFileLength(u8* payload);
  void prepareFileLoad(u8* payload);
  void prepareGctLength();
  void prepareGctLoad(u8* payload);
  void prepareDelayResponse();
  void preparePremadeTextLength(u8* payload);
  void preparePremadeTextLoad(u8* payload);
  bool doesTagMatchInput(u8* input, u8 input_len, std::string tag);

  std::vector<u8> loadPremadeText(u8* payload);

  void FileWriteThread(void);

  Common::SPSCQueue<std::unique_ptr<WriteMessage>, false> file_write_queue;
  bool write_thread_running = false;
  std::thread m_file_write_thread;

  std::unordered_map<u8, std::string> getNetplayNames();

  std::vector<u8> playback_savestate_payload;
  std::vector<u8> gecko_list;

  u32 stall_frame_count = 0;
  bool is_connection_stalled = false;

  std::vector<u8> m_read_queue;
  std::unique_ptr<Slippi::SlippiGame> m_current_game = nullptr;
  SlippiMatchmaking::MatchSearchSettings last_search;
  SlippiMatchmaking::MatchmakeResult recent_mm_result;

  std::vector<u16> stage_pool;

  // Used by ranked to set game prep selections
  std::vector<SlippiPlayerSelections> overwrite_selections;

  u32 frame_seq_idx = 0;

  bool is_enet_initialized = false;

  std::default_random_engine generator;

  std::string forced_error = "";

  // Used to determine when to detect when a new session has started
  bool is_play_session_active = false;

  // We put these at the class level to preserve values in the case of a disconnect
  // while loading. Without this, someone could load into a game playing the wrong char
  u8 m_local_player_idx = 0;
  u8 m_remote_player_idx = 1;

  // Frame skipping variables
  int frames_to_skip = 0;
  bool is_currently_skipping = false;

  // Frame advancing variables
  int frames_to_advance = 0;
  bool is_currently_advancing = false;
  int fall_behind_counter = 0;
  int fall_far_behind_counter = 0;

protected:
  void TransferByte(u8& byte) override;

private:
  SlippiPlayerSelections local_selections;

  std::unique_ptr<SlippiUser> user;
  std::unique_ptr<SlippiGameFileLoader> game_file_loader;
  std::unique_ptr<SlippiNetplayClient> slippi_netplay;
  std::unique_ptr<SlippiMatchmaking> matchmaking;
  std::unique_ptr<SlippiDirectCodes> direct_codes;
  std::unique_ptr<SlippiDirectCodes> teams_codes;

  std::map<s32, std::unique_ptr<SlippiSavestate>> active_savestates;
  std::deque<std::unique_ptr<SlippiSavestate>> available_savestates;

  std::vector<u16> allowed_stages;
};
}  // namespace ExpansionInterface
