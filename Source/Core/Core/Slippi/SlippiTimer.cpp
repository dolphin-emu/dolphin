// SLIPPITODO: refactor with qt

/*#include "SlippiTimer.h"
#include "DolphinWX/Frame.h"
#include "SlippiPlayback.h"

extern std::unique_ptr<SlippiPlaybackStatus> g_playbackStatus;

void SlippiTimer::Notify()
{
  if (!m_slider || !m_text)
  {
    // If there is no slider, do nothing
    return;
  }

  int totalSeconds = (int)((g_playbackStatus->latestFrame - Slippi::GAME_FIRST_FRAME) / 60);
  int totalMinutes = (int)(totalSeconds / 60);
  int totalRemainder = (int)(totalSeconds % 60);

  int currSeconds = int((g_playbackStatus->currentPlaybackFrame - Slippi::GAME_FIRST_FRAME) / 60);
  int currMinutes = (int)(currSeconds / 60);
  int currRemainder = (int)(currSeconds % 60);
  // Position string (i.e. MM:SS)
  char endTime[6];
  sprintf(endTime, "%02d:%02d", totalMinutes, totalRemainder);
  char currTime[6];
  sprintf(currTime, "%02d:%02d", currMinutes, currRemainder);

  std::string time = std::string(currTime) + " / " + std::string(endTime);

  // Setup the slider and gauge min/max values
  int minValue = m_slider->GetMin();
  int maxValue = m_slider->GetMax();
  if (maxValue != (int)g_playbackStatus->latestFrame || minValue != Slippi::PLAYBACK_FIRST_SAVE)
  {
    m_slider->SetRange(Slippi::PLAYBACK_FIRST_SAVE, (int)(g_playbackStatus->latestFrame));
  }

  // Only update values while not actively seeking
  if (g_playbackStatus->targetFrameNum == INT_MAX && m_slider->isDraggingSlider == false)
  {
    m_text->SetLabel(_(time));
    m_slider->SetValue(g_playbackStatus->currentPlaybackFrame);
  }
}*/
