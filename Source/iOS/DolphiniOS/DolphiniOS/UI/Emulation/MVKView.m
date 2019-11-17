// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "MVKView.h"

#import "MetalKit/MetalKit.h"

@implementation MVKView

+ (Class)layerClass
{
  return [CAMetalLayer class];
}

@end
