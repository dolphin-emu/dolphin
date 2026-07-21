// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ImageConverter.h"

#include "UICommon/GameFile.h"

#include <QPixmap>

QPixmap ToQPixmap(const UICommon::GameBanner& banner)
{
  const auto* ptr = reinterpret_cast<const uchar*>(banner.buffer.data());
  QImage image(ptr, banner.width, banner.height, QImage::Format_RGBX8888);
  return QPixmap::fromImage(std::move(image).rgbSwapped());
}
