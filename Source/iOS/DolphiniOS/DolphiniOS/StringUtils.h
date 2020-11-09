// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#define FoundationToCString(x) [x UTF8String]

#define CToFoundationString(x) [NSString stringWithUTF8String:x]

#ifdef __cplusplus

#import <string>

#define FoundationToCppString(x) std::string(FoundationToCString(x))

#define CppToFoundationString(x) CToFoundationString(x.c_str())

#endif
