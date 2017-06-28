// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <QDialog>

class AboutDialog final : public QDialog
{
  W_OBJECT(AboutDialog)
public:
  explicit AboutDialog(QWidget* parent = nullptr);
};
