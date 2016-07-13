// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class InDevelopmentWarning final : public QDialog
{
  Q_OBJECT

public:
  explicit InDevelopmentWarning(QWidget* parent = nullptr);
  ~InDevelopmentWarning();
};
