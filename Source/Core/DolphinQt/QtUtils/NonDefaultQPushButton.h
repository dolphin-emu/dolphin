// Copyright 2022 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QPushButton>

// A custom QPushButton that sets the autoDefault option to false
// This makes sure random buttons are not highlighted anymore by default
class NonDefaultQPushButton : public QPushButton
{
public:
  explicit NonDefaultQPushButton(const QString& text = {}, QWidget* parent = nullptr);
};
