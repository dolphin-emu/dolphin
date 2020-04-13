// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#ifdef __cplusplus

#import <string>

#define FoundationToCppString(x) std::string([x UTF8String])

#define CppToFoundationString(x) [NSString stringWithUTF8String:x.c_str()]

#endif

#define CToFoundationString(x) [NSString stringWithUTF8String:x]
