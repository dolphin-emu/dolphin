#pragma once

#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>

namespace Slippi {
  const uint8_t EVENT_SPLIT_MESSAGE = 0x10;
  const uint8_t EVENT_PAYLOAD_SIZES = 0x35;
  const uint8_t EVENT_GAME_INIT = 0x36;
  const uint8_t EVENT_PRE_FRAME_UPDATE = 0x37;
  const uint8_t EVENT_POST_FRAME_UPDATE = 0x38;
  const uint8_t EVENT_GAME_END = 0x39;
  const uint8_t EVENT_FRAME_START = 0x3A;
  const uint8_t EVENT_GECKO_LIST = 0x3D;

  const uint8_t GAME_INFO_HEADER_SIZE = 78;
  const uint8_t UCF_TOGGLE_SIZE = 8;
  const uint8_t NAMETAG_SIZE = 8;
  const int32_t GAME_FIRST_FRAME = -123;
  const int32_t PLAYBACK_FIRST_SAVE = -122;
  const uint8_t GAME_SHEIK_INTERNAL_ID = 0x7;
  const uint8_t GAME_SHEIK_EXTERNAL_ID = 0x13;

  const uint32_t SPLIT_MESSAGE_INTERNAL_DATA_LEN = 512;

  static uint8_t* data;

  typedef struct {
    // Every player update has its own rng seed because it might change in between players
    uint32_t randomSeed;

    uint8_t internalCharacterId;
    uint16_t animation;
    float locationX;
    float locationY;
    float facingDirection;
    uint8_t stocks;
    float percent;
    float shieldSize;
    uint8_t lastMoveHitId;
    uint8_t comboCount;
    uint8_t lastHitBy;

    //Controller information
    float joystickX;
    float joystickY;
    float cstickX;
    float cstickY;
    float trigger;
    uint32_t buttons; //This will include multiple "buttons" pressed on special buttons. For example I think pressing z sets 3 bits

    //This is extra controller information
    uint16_t physicalButtons; //A better representation of what a player is actually pressing
    float lTrigger;
    float rTrigger;

    uint8_t joystickXRaw;
  } PlayerFrameData;

  typedef struct FrameData {
    int32_t frame;
    uint32_t numSinceStart;
    bool randomSeedExists = false;
    uint32_t randomSeed;
    bool inputsFullyFetched = false;
    std::unordered_map<uint8_t, PlayerFrameData> players;
    std::unordered_map<uint8_t, PlayerFrameData> followers;
  } FrameData;

  typedef struct {
    //Static data
    uint8_t characterId;
    uint8_t characterColor;
    uint8_t playerType;
    uint8_t controllerPort;
    std::array<uint16_t, NAMETAG_SIZE> nametag;
  } PlayerSettings;

  typedef struct {
    uint16_t stage; //Stage ID
    uint32_t randomSeed;
    std::array<uint32_t, GAME_INFO_HEADER_SIZE> header;
    std::array<uint32_t, UCF_TOGGLE_SIZE> ucfToggles;
    std::unordered_map<uint8_t, PlayerSettings> players;
    uint8_t isPAL;
    uint8_t isFrozenPS;
    std::vector<uint8_t> geckoCodes;
  } GameSettings;

  typedef struct Game {
    std::array<uint8_t, 4> version;
    std::unordered_map<int32_t, FrameData*> framesByIndex;
    std::vector<std::unique_ptr<FrameData>> frames;
    GameSettings settings;
    bool areSettingsLoaded = false;

    int32_t frameCount; // Current/last frame count

    //From OnGameEnd event
    uint8_t winCondition;
  } Game;

  // TODO: This shouldn't be static. Doesn't matter too much atm because we always
  // TODO: only read one file at a time
  static std::unordered_map<uint8_t, uint32_t> asmEvents = {
    { EVENT_GAME_INIT, 320 },
    { EVENT_PRE_FRAME_UPDATE, 58 },
    { EVENT_POST_FRAME_UPDATE, 33 },
    { EVENT_GAME_END, 1 },
    { EVENT_FRAME_START, 8 }
  };

  class SlippiGame
  {
  public:
    static std::unique_ptr<SlippiGame> FromFile(std::string path);
    bool AreSettingsLoaded();
    bool DoesFrameExist(int32_t frame);
    std::array<uint8_t, 4> GetVersion();
    FrameData* GetFrame(int32_t frame);
    FrameData* GetFrameAt(uint32_t pos);
    int32_t GetLatestIndex();
    GameSettings* GetSettings();
    bool DoesPlayerExist(int8_t port);
    bool IsProcessingComplete();
  private:
    std::unique_ptr<Game> game;
    std::unique_ptr<std::ifstream> file;
    std::vector<uint8_t> rawData;
    std::string path;
    std::ofstream log;
    std::vector<uint8_t> splitMessageBuf;
    bool shouldResetSplitMessageBuf = false;

    bool isProcessingComplete = false;
    void processData();
  };
}
