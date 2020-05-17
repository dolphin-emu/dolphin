// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <utility>

#include <QFontMetrics>

// This exists purely to get rid of deprecation warnings while still supporting older Qt versions
template <typename... Args>
int FontMetricsWidth(const QFontMetrics& fm, Args&&... args)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  return fm.horizontalAdvance(std::forward<Args>(args)...);
#else
  return fm.width(std::forward<Args>(args)...);
#endif
}
