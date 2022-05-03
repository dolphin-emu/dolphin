#pragma once

#include <string>

class StateAuxillary
{
public:
  static void saveState(const std::string& filename, bool wait = false);
};
