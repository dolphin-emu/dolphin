// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#define DOLocalizedString(x) NSLocalizedStringFromTable(x, @"Dolphin", nil)

#define DOLocalizedStringWithArgs(x, ...) [LocalizerUtils setFormatSpecifiersOnQString:DOLocalizedString(x), __VA_ARGS__, nil]

NS_ASSUME_NONNULL_BEGIN

@interface LocalizerUtils : NSObject

+ (NSString*)setFormatSpecifiersOnQString:(NSString*)str, ...;

@end

NS_ASSUME_NONNULL_END
