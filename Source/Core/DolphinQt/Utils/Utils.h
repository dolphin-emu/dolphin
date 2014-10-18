// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QString>

#include "Common/CommonTypes.h"

// Shorter version of QStringLiteral(str)
#define SL(str) QStringLiteral(str)

QString NiceSizeFormat(s64 size);
