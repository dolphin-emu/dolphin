// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QString>

#include "Common/CommonTypes.h"

// Use this to encapsulate ASCII string literals
#define SL(str) QStringLiteral(str)

// Use this to encapsulate string constants and functions that
// return "char*"s
#define SC(str) QString::fromLatin1(str)

QString NiceSizeFormat(s64 size);
