// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NetPlayHolder.h"

@implementation NetPlayHolder

+ (NetPlayHolder*)sharedNetPlayHolder
{
  static dispatch_once_t _once_token = 0;
  static NetPlayHolder* _sharedHolder = nil;
  
  dispatch_once(&_once_token, ^{
      _sharedHolder = [[self alloc] init];
  });
  
  return _sharedHolder;
}




@end
