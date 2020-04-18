// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <GameController/GameController.h>

#import <MetalKit/MetalKit.h>

#import <UIKit/UIKit.h>

#import "DolphiniOS-Swift.h"

#import "EAGLView.h"

#import "UICommon/GameFile.h"

NS_ASSUME_NONNULL_BEGIN

@interface EmulationViewController : GCEventViewController
{
  @public std::vector<std::pair<int, TCView*>> m_controllers;
}

@property (weak, nonatomic) IBOutlet MTKView* m_metal_view;
@property (weak, nonatomic) IBOutlet EAGLView* m_eagl_view;

@property (weak, nonatomic) IBOutlet TCView* m_gc_pad;
@property (weak, nonatomic) IBOutlet TCView* m_wii_normal_pad;
@property (weak, nonatomic) IBOutlet TCView* m_wii_sideways_pad;
@property (weak, nonatomic) IBOutlet TCView* m_wii_classic_pad;

@property (strong, nonatomic) IBOutlet NSLayoutConstraint* m_metal_bottom_constraint;
@property (strong, nonatomic) IBOutlet NSLayoutConstraint* m_metal_half_constraint;
@property (strong, nonatomic) IBOutlet NSLayoutConstraint* m_eagl_bottom_constraint;
@property (strong, nonatomic) IBOutlet NSLayoutConstraint* m_eagl_half_constraint;

@property (strong, nonatomic) IBOutlet UIScreenEdgePanGestureRecognizer* m_edge_pan_recognizer;

@property (weak, nonatomic) IBOutlet UINavigationItem* m_navigation_item;

@property(nonatomic) UICommon::GameFile* m_game_file;
@property(nonatomic) UIView* m_renderer_view;
@property(nonatomic) int m_ts_active_port;
@property(weak, nonatomic) TCView* m_ts_active_view;
@property(nonatomic) UIBarButtonItem* m_stop_button;
@property(nonatomic) UIBarButtonItem* m_pause_button;
@property(nonatomic) UIBarButtonItem* m_play_button;

- (void)PopulatePortDictionary;
- (void)ChangeVisibleTouchControllerToPort:(int)port;

@end

NS_ASSUME_NONNULL_END
