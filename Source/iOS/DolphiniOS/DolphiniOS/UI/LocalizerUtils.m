// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "LocalizerUtils.h"

@implementation LocalizerUtils

+ (NSString*)setFormatSpecifiersOnQString:(NSString*)str, ...
{
  NSString* current_str = str;
  
  va_list args;
  va_start(args, str);
  
  id arg;
  int arg_count = 0;
  
  while ((arg = va_arg(args, id)) != nil)
  {
    arg_count++;
    
    NSString* pre_arg_specifier = [NSString stringWithFormat:@"%%%d", arg_count];
    NSString* final_arg_specifier = [NSString stringWithFormat:@"%%%d$%@", arg_count, arg];
    current_str = [current_str stringByReplacingOccurrencesOfString:pre_arg_specifier withString:final_arg_specifier];
  }
  
  va_end(args);
  
  return current_str;
}

@end
