// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#import "EAGLView.h"

@implementation EAGLView

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

+ (Class)layerClass
{
  return [CAEAGLLayer class];
}

#pragma clang diagnostic pop

@end
