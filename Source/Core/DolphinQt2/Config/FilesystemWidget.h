// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <QDialog>

#include "DolphinQt2/GameList/GameFile.h"

class FilesystemWidget final : public QWidget
{
  W_OBJECT(FilesystemWidget)
public:
  explicit FilesystemWidget(const GameFile& game);

private:
  GameFile m_game;
};
