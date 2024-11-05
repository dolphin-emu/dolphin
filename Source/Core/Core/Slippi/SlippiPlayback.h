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

  bool should_jump_back = false;
  bool should_jump_forward = false;
  bool in_slippi_playback = false;
  volatile bool should_run_threads = false;
  bool is_hard_FFW = false;
  bool is_soft_FFW = false;
  bool orig_OC_enable = SConfig::GetSlippiConfig().oc_enable;
  float orig_OC_factor = SConfig::GetSlippiConfig().oc_factor;

  s32 last_FFW_frame = INT_MIN;
  s32 current_playback_frame = INT_MIN;
  s32 target_frame_num = INT_MAX;
  s32 last_frame = Slippi::PLAYBACK_FIRST_SAVE;

  std::thread m_savestate_thread;

  void startThreads();
  void resetPlayback(void);
  bool shouldFFWFrame(s32 frame_idx) const;
  void prepareSlippiPlayback(s32& frame_idx);
  void setHardFFW(bool enable);
  std::unordered_map<u32, bool> getDenylist();
  std::vector<u8> getLegacyCodelist();
  void seekToFrame(Core::System& system);

private:
  void SavestateThread();
  void loadState(Core::System& system, s32 closest_state_frame);
  void processInitialState(Core::System& system);
  void updateWatchSettingsStartEnd();
  void generateDenylist();
  void generateLegacyCodelist();

  std::unordered_map<int32_t, std::shared_future<std::string>>
      future_diffs;               // State diffs keyed by frame_idx, processed async
  std::vector<u8> initial_state;  // The initial state
  std::vector<u8> curr_state;     // The current (latest) state

  std::unordered_map<u32, bool> deny_list;
  std::vector<u8> legacy_code_list;

  open_vcdiff::VCDiffDecoder decoder;
  open_vcdiff::VCDiffEncoder* encoder = NULL;
};
