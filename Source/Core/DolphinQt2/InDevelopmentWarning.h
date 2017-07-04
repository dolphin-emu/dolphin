// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <wobjectdefs.h>

class InDevelopmentWarning final : public QDialog
{
  W_OBJECT(InDevelopmentWarning)

public:
  explicit InDevelopmentWarning(QWidget* parent = nullptr);
  ~InDevelopmentWarning();
};
