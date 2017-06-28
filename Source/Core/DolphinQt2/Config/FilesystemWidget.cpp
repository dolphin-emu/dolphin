// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wobjectimpl.h>
#include "DolphinQt2/Config/FilesystemWidget.h"

W_OBJECT_IMPL(FilesystemWidget)

FilesystemWidget::FilesystemWidget(const GameFile& game) : m_game(game)
{
}
