#pragma once

#include <string>
#include "Core/Movie.h"

class StateAuxillary
{
public:
  static void saveState(const std::string& filename, bool wait = false);
  static void startRecording(const Movie::ControllerTypeArray& controllers, const Movie::WiimoteEnabledArray& wiimotes);
};
