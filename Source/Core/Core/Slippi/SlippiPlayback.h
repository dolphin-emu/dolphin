#pragma once

#include <climits>
#include <future>
#include <open-vcdiff/src/google/vcdecoder.h>
#include <open-vcdiff/src/google/vcencoder.h>
#include <SlippiLib/SlippiGame.h>
#include <unordered_map>
#include <vector>

#include "../../Common/CommonTypes.h"

class SlippiPlaybackStatus
{
public:
  SlippiPlaybackStatus();
  virtual ~SlippiPlaybackStatus();

  bool shouldJumpBack = false;
  bool shouldJumpForward = false;
  bool inSlippiPlayback = false;
  volatile bool shouldPause = false;
  volatile bool shouldRunThreads = false;
  bool isHardFFW = false;
  bool isSoftFFW = false;
  s32 lastFFWFrame = INT_MIN;
  s32 currentPlaybackFrame = INT_MIN;
  s32 targetFrameNum = INT_MAX;
  s32 lastFrame = Slippi::PLAYBACK_FIRST_SAVE;

  std::thread m_savestateThread;

  void startThreads(void);
  void resetPlayback(void);
  void prepareSlippiPlayback(s32& frameIndex);
  void SeekToFrame();

private:
  void SavestateThread(void);
  void processInitialState();
  void clearWatchSettingsStartEnd();

  std::unordered_map<int32_t, std::shared_future<std::string>>
    futureDiffs;        // State diffs keyed by frameIndex, processed async
  std::vector<u8> iState; // The initial state
  std::vector<u8> cState; // The current (latest) state

  open_vcdiff::VCDiffDecoder decoder;
  open_vcdiff::VCDiffEncoder* encoder = NULL;
};
