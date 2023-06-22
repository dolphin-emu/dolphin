#pragma once

#include <climits>
#include <future>
#include <unordered_map>
#include <vector>

#include <open-vcdiff/src/google/vcdecoder.h>
#include <open-vcdiff/src/google/vcencoder.h>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "SlippiGame.h"

class SlippiPlaybackStatus
{
public:
  SlippiPlaybackStatus();
  ~SlippiPlaybackStatus();

  bool shouldJumpBack = false;
  bool shouldJumpForward = false;
  bool inSlippiPlayback = false;
  volatile bool shouldRunThreads = false;
  bool isHardFFW = false;
  bool isSoftFFW = false;
  bool origOCEnable = SConfig::GetSlippiConfig().oc_enable;
  float origOCFactor = SConfig::GetSlippiConfig().oc_factor;

  s32 lastFFWFrame = INT_MIN;
  s32 currentPlaybackFrame = INT_MIN;
  s32 targetFrameNum = INT_MAX;
  s32 lastFrame = Slippi::PLAYBACK_FIRST_SAVE;

  std::thread m_savestateThread;

  void startThreads(void);
  void resetPlayback(void);
  bool shouldFFWFrame(s32 frameIndex) const;
  void prepareSlippiPlayback(s32& frameIndex);
  void setHardFFW(bool enable);
  std::unordered_map<u32, bool> getDenylist();
  std::vector<u8> getLegacyCodelist();
  void seekToFrame();

private:
  void SavestateThread(void);
  void loadState(s32 closestStateFrame);
  void processInitialState();
  void updateWatchSettingsStartEnd();
  void generateDenylist();
  void generateLegacyCodelist();

  std::unordered_map<int32_t, std::shared_future<std::string>>
      futureDiffs;         // State diffs keyed by frameIndex, processed async
  std::vector<u8> iState;  // The initial state
  std::vector<u8> cState;  // The current (latest) state

  std::unordered_map<u32, bool> denylist;
  std::vector<u8> legacyCodelist;

  open_vcdiff::VCDiffDecoder decoder;
  open_vcdiff::VCDiffEncoder* encoder = NULL;
};
