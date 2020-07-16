#include <string>
#include <codecvt>
#include <locale>

#include "SlippiGame.h"

namespace Slippi {

  //**********************************************************************
  //*                         Event Handlers
  //**********************************************************************
  //The read operators will read a value and increment the index so the next read will read in the correct location
  uint8_t readByte(uint8_t* a, int& idx, uint32_t maxSize, uint8_t defaultValue) {
    if (idx >= (int)maxSize) {
      idx += 1;
      return defaultValue;
    }

    return a[idx++];
  }

  uint16_t readHalf(uint8_t* a, int& idx, uint32_t maxSize, uint16_t defaultValue) {
    if (idx >= (int)maxSize) {
      idx += 2;
      return defaultValue;
    }

    uint16_t value = a[idx] << 8 | a[idx + 1];
    idx += 2;
    return value;
  }

  uint32_t readWord(uint8_t* a, int& idx, uint32_t maxSize, uint32_t defaultValue) {
    if (idx >= (int)maxSize) {
      idx += 4;
      return defaultValue;
    }

    uint32_t value = a[idx] << 24 | a[idx + 1] << 16 | a[idx + 2] << 8 | a[idx + 3];
    idx += 4;
    return value;
  }

  float readFloat(uint8_t* a, int& idx, uint32_t maxSize, float defaultValue) {
    uint32_t bytes = readWord(a, idx, maxSize, *(uint32_t*)(&defaultValue));
    return *(float*)(&bytes);
  }

  void handleGameInit(Game* game, uint32_t maxSize) {
    int idx = 0;

    // Read version number
    for (int i = 0; i < 4; i++) {
      game->version[i] = readByte(data, idx, maxSize, 0);
    }

    // Read entire game info header
    for (int i = 0; i < GAME_INFO_HEADER_SIZE; i++) {
      game->settings.header[i] = readWord(data, idx, maxSize, 0);
    }

    // Load random seed
    game->settings.randomSeed = readWord(data, idx, maxSize, 0);

    // Read UCF toggle bytes
    bool shouldRead = game->version[0] >= 1;
    for (int i = 0; i < UCF_TOGGLE_SIZE; i++) {
      uint32_t value = shouldRead ? readWord(data, idx, maxSize, 0) : 0;
      game->settings.ucfToggles[i] = value;
    }

    // Read nametag for each player
    std::array<std::array<uint16_t, NAMETAG_SIZE>, 4> playerNametags;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < NAMETAG_SIZE; j++) {
        playerNametags[i][j] = readHalf(data, idx, maxSize, 0);
      }
    }

    // Read isPAL byte
    game->settings.isPAL = readByte(data, idx, maxSize, 0);

    // Read isFrozenPS byte
    game->settings.isFrozenPS = readByte(data, idx, maxSize, 0);

    // Pull header data into struct
    int player1Pos = 24; // This is the index of the first players character info
    std::array<uint32_t, Slippi::GAME_INFO_HEADER_SIZE> gameInfoHeader = game->settings.header;
    for (int i = 0; i < 4; i++) {
      // this is the position in the array that this player's character info is stored
      int pos = player1Pos + (9 * i);

      uint32_t playerInfo = gameInfoHeader[pos];
      uint8_t playerType = (playerInfo & 0x00FF0000) >> 16;
      if (playerType == 0x3) {
        // Player type 3 is an empty slot
        continue;
      }

      PlayerSettings p;

      // Get player settings
      p.controllerPort = i;
      p.characterId = playerInfo >> 24;
      p.playerType = playerType;
      p.characterColor = playerInfo & 0xFF;
      p.nametag = playerNametags[i];

      //Add player settings to result
      game->settings.players[i] = p;
    }

    game->settings.stage = gameInfoHeader[3] & 0xFFFF;

    auto majorVersion = game->version[0];
    auto minorVersion = game->version[1];
    if (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 1)) {
      // After version 3.1.0 we added a dynamic gecko loading process. These
      // are needed before starting the game. areSettingsLoaded will be set
      // to true when they are received
      game->areSettingsLoaded = false;
    }
    else if (majorVersion > 1 || (majorVersion == 1 && minorVersion >= 6)) {
      // Indicate settings loaded immediately if after version 1.6.0
      // Sheik game info was added in this version and so we no longer
      // need to wait
      game->areSettingsLoaded = true;
    }
  }

  void handleGeckoList(Game* game, uint32_t maxSize) {
    game->settings.geckoCodes.clear();
    game->settings.geckoCodes.insert(game->settings.geckoCodes.end(), data, data + maxSize);

    // File is good to load
    game->areSettingsLoaded = true;
  }

  void handleFrameStart(Game* game, uint32_t maxSize) {
    int idx = 0;

    //Check frame count
    int32_t frameCount = readWord(data, idx, maxSize, 0);
    game->frameCount = frameCount;

    auto frameUniquePtr = std::make_unique<FrameData>();
    FrameData* frame = frameUniquePtr.get();

    frame->frame = frameCount;
    frame->randomSeedExists = true;
    frame->randomSeed = readWord(data, idx, maxSize, 0);

    // Add frame to game. The frames are stored in multiple ways because
    // for games with rollback, the same frame may be replayed multiple times
    frame->numSinceStart = game->frames.size();
    game->frames.push_back(std::move(frameUniquePtr));
    game->framesByIndex[frameCount] = frame;
  }

  void handlePreFrameUpdate(Game* game, uint32_t maxSize) {
    int idx = 0;

    //Check frame count
    int32_t frameCount = readWord(data, idx, maxSize, 0);
    game->frameCount = frameCount;

    auto frameUniquePtr = std::make_unique<FrameData>();
    FrameData* frame = frameUniquePtr.get();
    bool isNewFrame = true;

    if (game->framesByIndex.count(frameCount)) {
      // If this frame already exists, get the current frame
      frame = game->frames.back().get();
      isNewFrame = false;
    }

    frame->frame = frameCount;

    PlayerFrameData p;

    uint8_t playerSlot = readByte(data, idx, maxSize, 0);
    uint8_t isFollower = readByte(data, idx, maxSize, 0);

    //Load random seed for player frame update
    p.randomSeed = readWord(data, idx, maxSize, 0);

    //Load player data
    p.animation = readHalf(data, idx, maxSize, 0);
    p.locationX = readFloat(data, idx, maxSize, 0);
    p.locationY = readFloat(data, idx, maxSize, 0);
    p.facingDirection = readFloat(data, idx, maxSize, 0);

    //Controller information
    p.joystickX = readFloat(data, idx, maxSize, 0);
    p.joystickY = readFloat(data, idx, maxSize, 0);
    p.cstickX = readFloat(data, idx, maxSize, 0);
    p.cstickY = readFloat(data, idx, maxSize, 0);
    p.trigger = readFloat(data, idx, maxSize, 0);
    p.buttons = readWord(data, idx, maxSize, 0);

    //Raw controller information
    p.physicalButtons = readHalf(data, idx, maxSize, 0);
    p.lTrigger = readFloat(data, idx, maxSize, 0);
    p.rTrigger = readFloat(data, idx, maxSize, 0);

    if (asmEvents[EVENT_PRE_FRAME_UPDATE] >= 59) {
      p.joystickXRaw = readByte(data, idx, maxSize, 0);
    }

    uint32_t noPercent = 0xFFFFFFFF;
    p.percent = readFloat(data, idx, maxSize, *(float*)(&noPercent));

    // Add player data to frame
    std::unordered_map<uint8_t, PlayerFrameData>* target;
    target = isFollower ? &frame->followers : &frame->players;

    // Set the player data for the player or follower
    target->operator[](playerSlot) = p;

    // Add frame to game
    if (isNewFrame) {
      frame->numSinceStart = game->frames.size();
      game->frames.push_back(std::move(frameUniquePtr));
      game->framesByIndex[frameCount] = frame;
    }
  }

  void handlePostFrameUpdate(Game* game, uint32_t maxSize) {
    int idx = 0;

    //Check frame count
    int32_t frameCount = readWord(data, idx, maxSize, 0);

    FrameData* frame;
    if (game->framesByIndex.count(frameCount)) {
      // If this frame already exists, get the current frame
      frame = game->frames.back().get();
    }

    // As soon as a post frame update happens, we know we have received all the inputs
    // This is used to determine if a frame is ready to be used for a replay (for mirroring)
    frame->inputsFullyFetched = true;

    uint8_t playerSlot = readByte(data, idx, maxSize, 0);
    uint8_t isFollower = readByte(data, idx, maxSize, 0);

    PlayerFrameData* p = isFollower ? &frame->followers[playerSlot] : &frame->players[playerSlot];

    p->internalCharacterId = readByte(data, idx, maxSize, 0);

    // Check if a player started as sheik and update
    if (frameCount == GAME_FIRST_FRAME && p->internalCharacterId == GAME_SHEIK_INTERNAL_ID) {
      game->settings.players[playerSlot].characterId = GAME_SHEIK_EXTERNAL_ID;
    }

    // Set settings loaded if this is the last character
    if (frameCount == GAME_FIRST_FRAME) {
      uint8_t lastPlayerIndex = 0;
      for (auto it = frame->players.begin(); it != frame->players.end(); ++it) {
        if (it->first <= lastPlayerIndex) {
          continue;
        }

        lastPlayerIndex = it->first;
      }

      if (playerSlot >= lastPlayerIndex) {
        game->areSettingsLoaded = true;
      }
    }
  }

  void handleGameEnd(Game* game, uint32_t maxSize) {
    int idx = 0;

    game->winCondition = readByte(data, idx, maxSize, 0);
  }

  // This function gets the position where the raw data starts
  int getRawDataPosition(std::ifstream* f) {
    char buffer[2];
    f->seekg(0, std::ios::beg);
    f->read(buffer, 2);

    if (buffer[0] == 0x36) {
      return 0;
    }

    if (buffer[0] != '{') {
      // TODO: Do something here to cause an error
      return 0;
    }

    // TODO: Read ubjson file to find the "raw" element and return the start of it
    // TODO: For now since raw is the first element the data will always start at 15
    return 15;
  }

  uint32_t getRawDataLength(std::ifstream* f, int position, int fileSize) {
    if (position == 0) {
      return fileSize;
    }

    char buffer[4];
    f->seekg(position - 4, std::ios::beg);
    f->read(buffer, 4);

    uint8_t* byteBuf = (uint8_t*)&buffer[0];
    uint32_t length = byteBuf[0] << 24 | byteBuf[1] << 16 | byteBuf[2] << 8 | byteBuf[3];
    return length;
  }

  std::unordered_map<uint8_t, uint32_t> getMessageSizes(std::ifstream* f, int position) {
    char buffer[2];
    f->seekg(position, std::ios::beg);
    f->read(buffer, 2);
    if (buffer[0] != EVENT_PAYLOAD_SIZES) {
      return {};
    }

    int payloadLength = buffer[1];
    std::unordered_map<uint8_t, uint32_t> messageSizes = {
      { EVENT_PAYLOAD_SIZES, payloadLength }
    };


    std::vector<char> messageSizesBuffer(payloadLength - 1);
    f->read(&messageSizesBuffer[0], payloadLength - 1);
    for (int i = 0; i < payloadLength - 1; i += 3) {
      uint8_t command = messageSizesBuffer[i];

      // Extract the bytes in u8s. Without this the chars don't or together well
      uint8_t byte1 = messageSizesBuffer[i + 1];
      uint8_t byte2 = messageSizesBuffer[i + 2];

      uint16_t size = byte1 << 8 | byte2;
      messageSizes[command] = size;
    }

    return messageSizes;
  }

  void SlippiGame::processData() {
    if (isProcessingComplete) {
      // If we have finished processing this file, return
      return;
    }

    // This function will process as much data as possible
    int startPos = (int)file->tellg();
    file->seekg(startPos);
    if (startPos == 0) {
      file->seekg(0, std::ios::end);
      int len = (int)file->tellg();
      if (len < 2) {
        // If we can't read message sizes payload size yet, return
        return;
      }

      int rawDataPos = getRawDataPosition(file.get());
      int rawDataLen = len - rawDataPos;
      if (rawDataLen < 2) {
        // If we don't have enough raw data yet to read the replay file, return
        // Reset to begining so that the startPos condition will be hit again
        file->seekg(0);
        return;
      }

      startPos = rawDataPos;

      char buffer[2];
      file->seekg(startPos);
      file->read(buffer, 2);
      file->seekg(startPos);
      auto messageSizesSize = (int)buffer[1];
      if (rawDataLen < messageSizesSize) {
        // If we haven't received the full payload sizes message, return
        // Reset to begining so that the startPos condition will be hit again
        file->seekg(0);
        return;
      }

      asmEvents = getMessageSizes(file.get(), rawDataPos);
    }

    // Read everything to the end
    file->seekg(0, std::ios::end);
    int endPos = (int)file->tellg();
    int sizeToRead = endPos - startPos;
    file->seekg(startPos);
    //log << "Size to read: " << sizeToRead << "\n";
    //log << "Start Pos: " << startPos << "\n";
    //log << "End Pos: " << endPos << "\n\n";
    if (sizeToRead <= 0) {
      return;
    }

    std::vector<char> newData(sizeToRead);
    file->read(&newData[0], sizeToRead);

    int newDataPos = 0;
    while (newDataPos < sizeToRead) {
      auto command = newData[newDataPos];
      auto payloadSize = asmEvents[command];

      //char buff[100];
      //snprintf(buff, sizeof(buff), "%x", command);
      //log << "Command: " << buff << " | Payload Size: " << payloadSize << "\n";

      auto remainingLen = sizeToRead - newDataPos;
      if (remainingLen < ((int)payloadSize + 1)) {
        // Here we don't have enough data to read the whole payload
        // Will be processed after getting more data (hopefully)
        file->seekg(-remainingLen, std::ios::cur);
        return;
      }

      data = (uint8_t*)&newData[newDataPos + 1];

      uint8_t isSplitComplete = false;
      uint32_t outerPayloadSize = payloadSize;

      // Handle a split message, combining in until we possess the entire message
      if (command == EVENT_SPLIT_MESSAGE) {
        if (shouldResetSplitMessageBuf)
        {
          splitMessageBuf.clear();
          shouldResetSplitMessageBuf = false;
        }

        int _ = 0;
        uint16_t blockSize = readHalf(&data[SPLIT_MESSAGE_INTERNAL_DATA_LEN], _, payloadSize, 0);
        splitMessageBuf.insert(splitMessageBuf.end(), data, data + blockSize);

        isSplitComplete = data[SPLIT_MESSAGE_INTERNAL_DATA_LEN + 3];
        if (isSplitComplete)
        {
          // Transform this message into a different message
          command = data[SPLIT_MESSAGE_INTERNAL_DATA_LEN + 2];
          data = &splitMessageBuf[0];
          payloadSize = asmEvents[command];
          shouldResetSplitMessageBuf = true;
        }
      }

      switch (command) {
      case EVENT_GAME_INIT:
        handleGameInit(game.get(), payloadSize);
        break;
      case EVENT_GECKO_LIST:
        handleGeckoList(game.get(), payloadSize);
        break;
      case EVENT_FRAME_START:
        handleFrameStart(game.get(), payloadSize);
        break;
      case EVENT_PRE_FRAME_UPDATE:
        handlePreFrameUpdate(game.get(), payloadSize);
        break;
      case EVENT_POST_FRAME_UPDATE:
        handlePostFrameUpdate(game.get(), payloadSize);
        break;
      case EVENT_GAME_END:
        handleGameEnd(game.get(), payloadSize);
        isProcessingComplete = true;
        break;
      case 0x55:
        // This is sort of a hack to prevent this functioning
        // from processing the metadata as raw data. 0x55 is 'U'
        // which is the first character after the raw data in the
        // ubjson file format
        //log.close();
        isProcessingComplete = true;
        file->seekg(-remainingLen, std::ios::cur);
        return;
      }

      payloadSize = isSplitComplete ? outerPayloadSize : payloadSize;
      newDataPos += payloadSize + 1;
    }
  }

  std::unique_ptr<SlippiGame> SlippiGame::FromFile(std::string path) {
    auto result = std::make_unique<SlippiGame>();
    result->game = std::make_unique<Game>();
    result->path = path;

#ifdef _WIN32
    // On Windows, we need to convert paths to std::wstring to deal with UTF-8
    std::wstring convertedPath = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(path);
    result->file = std::make_unique<std::ifstream>(convertedPath, std::ios::in | std::ios::binary);
#else
    result->file = std::make_unique<std::ifstream>(path, std::ios::in | std::ios::binary);
#endif

    //result->log.open("log.txt");
    if (!result->file->is_open()) {
      return nullptr;
    }

    //int fileLength = (int)file.tellg();
    //int rawDataPos = getRawDataPosition(&file);
    //uint32_t rawDataLength = getRawDataLength(&file, rawDataPos, fileLength);
    //asmEvents = getMessageSizes(&file, rawDataPos);

    //std::vector<char> rawData(rawDataLength);
    //file.seekg(rawDataPos, std::ios::beg);
    //file.read(&rawData[0], rawDataLength);

    //SlippiGame* result = processFile((uint8_t*)&rawData[0], rawDataLength);

    return std::move(result);
  }

  bool SlippiGame::IsProcessingComplete() {
    return isProcessingComplete;
  }

  bool SlippiGame::AreSettingsLoaded() {
    processData();
    return game->areSettingsLoaded;
  }

  bool SlippiGame::DoesFrameExist(int32_t frame) {
    processData();
    return (bool)game->framesByIndex.count(frame);
  }

  std::array<uint8_t, 4> SlippiGame::GetVersion()
  {
    return game->version;
  }

  FrameData* SlippiGame::GetFrame(int32_t frame) {
    // Get the frame we want
    return game->framesByIndex.at(frame);
  }

  FrameData* SlippiGame::GetFrameAt(uint32_t pos) {
    if (pos >= game->frames.size()) {
      return nullptr;
    }

    // Get the frame we want
    return game->frames[pos].get();
  }

  int32_t SlippiGame::GetLatestIndex() {
    processData();
    return game->frameCount;
  }

  GameSettings* SlippiGame::GetSettings() {
    processData();
    return &game->settings;
  }

  bool SlippiGame::DoesPlayerExist(int8_t port) {
    return game->settings.players.find(port) != game->settings.players.end();
  }
}
