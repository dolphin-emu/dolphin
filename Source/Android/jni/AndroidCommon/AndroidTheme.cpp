// Copyright 2021 Dolphin MMJR
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AndroidTheme.h"

void AndroidTheme::Set(const std::string& theme)
{
  if (theme == NEON_RED)
  {
    currentFloat = F_RED;
    currentInt = &RED;
  }
  else if (theme == DOLPHIN_BLUE)
  {
    currentFloat = F_BLUE;
    currentInt = &BLUE;
  }
  else if (theme == LUIGI_GREEN)
  {
    currentFloat = F_GREEN;
    currentInt = &GREEN;
  }
  else
  {
    currentFloat = F_PURPLE;
    currentInt = &PURPLE;
  }
}

const float *AndroidTheme::GetFloat()
{
  return currentFloat;
}

const u32 *AndroidTheme::GetInt()
{
  return currentInt;
}
