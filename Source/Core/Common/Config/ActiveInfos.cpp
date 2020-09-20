// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Config/ActiveInfos.h"

#include <memory>
#include <vector>

namespace Config
{
std::unique_ptr<std::vector<void (*)()>> ActiveInfosBase::s_invalidate_cache_functions{};

bool ActiveInfosBase::s_program_is_exiting = false;

void ActiveInfosBase::InvalidateCache()
{
  if (s_invalidate_cache_functions)
  {
    for (auto f : *s_invalidate_cache_functions)
      f();
  }
}

}  // namespace Config
