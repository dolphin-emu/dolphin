// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <MetalKit/MetalKit.h>

#import <UIKit/UIKit.h>

#import "EAGLView.h"

#import "UICommon/GameFile.h"

NS_ASSUME_NONNULL_BEGIN

@interface EmulationViewController : UIViewController

@property (weak, nonatomic) IBOutlet MTKView* m_metal_view;
@property (weak, nonatomic) IBOutlet EAGLView* m_eagl_view;

@property (strong, nonatomic) IBOutletCollection(UIView) NSArray* m_gc_controllers;
@property (strong, nonatomic) IBOutletCollection(UIView) NSArray* m_wii_controllers;

@property(nonatomic) UIView* m_renderer_view;
@property(nonatomic) UICommon::GameFile* m_game_file;

@end

NS_ASSUME_NONNULL_END
