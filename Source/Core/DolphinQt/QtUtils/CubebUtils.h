// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QComboBox;
class QGridLayout;
class QWidget;

#include "Common/Config/Config.h"

namespace QtUtils
{
QGridLayout* CubebVolumeLayout(const Config::Info<s16>& cfg_volume, QWidget* parent);
QComboBox* CubebMicrophonesComboBox(const Config::Info<std::string>& cfg_microphone,
                                    QWidget* parent);
}  // namespace QtUtils
