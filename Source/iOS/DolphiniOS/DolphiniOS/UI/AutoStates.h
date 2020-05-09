// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, DOLAutoStateBootType) {
  DOLAutoStateBootTypeFile = 0,
  DOLAutoStateBootTypeNand,
  DOLAutoStateBootTypeGCIPL,
  DOLAutoStateBootTypeUnknown
};
