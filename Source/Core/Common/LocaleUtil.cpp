// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/LocaleUtil.h"

#include <locale.h>

namespace Common
{
std::locale GetEnvironmentLocale()
{
#ifdef _WIN32
  if (_locale_t loc = _create_locale(LC_ALL, ""))
  {
    _free_locale(loc);
    return std::locale{""};
  }
#else
  if (locale_t loc = newlocale(LC_ALL_MASK, "", static_cast<locale_t>(0)))
  {
    freelocale(loc);
    return std::locale{""};
  }
#endif
  return std::locale::classic();
}
}  // Namespace Common
