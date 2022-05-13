#pragma once

#include <string>
#include "Core/Movie.h"

class StateAuxillary
{
public:
  static void saveState(const std::string& filename, bool wait = false);
  static void startRecording();
  static void stopRecording(const std::string replay_path, tm* matchDateTimeParam);
};
