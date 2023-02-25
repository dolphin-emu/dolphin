// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/CommonTypes.h"

class QPixmap;

namespace UICommon
{
struct GameBanner;
}

QPixmap ToQPixmap(const UICommon::GameBanner& banner);
QPixmap ToQPixmap(const std::vector<u32>& buffer, int width, int height);
