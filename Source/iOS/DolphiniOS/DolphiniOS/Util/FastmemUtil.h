// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef NS_ENUM(NSUInteger, DOLFastmemType)
{
  DOLFastmemTypeNone,
  DOLFastmemTypeProper,
  DOLFastmemTypeHacky
};

bool CanEnableFastmem(void);
DOLFastmemType GetFastmemType(void);

#ifdef __cplusplus
}
#endif
