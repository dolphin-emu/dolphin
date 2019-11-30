// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Apple started to mark all OpenGL ES APIs as deprecated in the iOS 12 SDK.
// We can define this to silence all of the deprecation warnings.
#define GLES_SILENCE_DEPRECATION

#import <UIKit/UIKit.h>

@interface EAGLView : UIView

@end
