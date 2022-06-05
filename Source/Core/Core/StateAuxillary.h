#pragma once

#include <string>
#include "Core/Movie.h"
#include <Core/NetPlayProto.h>

class StateAuxillary
{
public:
  static void saveState(const std::string& filename, bool wait = false);
  static void startRecording();
  static void stopRecording(const std::string replay_path, tm* matchDateTimeParam);
  static void endPlayback();
  static void setNetPlayControllers(NetPlay::PadMappingArray m_pad_map);
};
