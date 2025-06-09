// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QPixmap;

namespace UICommon
{
struct GameBanner;
}

QPixmap ToQPixmap(const UICommon::GameBanner& banner);
