// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "DolphinQt2/GameList/GameFile.h"

class PropertiesDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit PropertiesDialog(QWidget* parent, const GameFile& game);

signals:
  void OpenGeneralSettings();
};
