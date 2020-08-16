// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <GameController/GameController.h>

#import <MetalKit/MetalKit.h>

#import "NKitWarningNoticeViewController.h"

#import <UIKit/UIKit.h>

#import "Core/Boot/Boot.h"

#import "DiscIO/Enums.h"

#import "DolphiniOS-Swift.h"

#import "EAGLView.h"

#import "UICommon/GameFile.h"

typedef NS_ENUM(NSUInteger, DOLTopBarPullDownMode) {
  DOLTopBarPullDownModeSwipe = 0,
  DOLTopBarPullDownModeButton,
  DOLTopBarPullDownModeAlwaysVisible,
  DOLTopBarPullDownModeAlwaysHidden
};

NS_ASSUME_NONNULL_BEGIN

@interface EmulationViewController : UIViewController <NKitWarningNoticeDelegate>
{
  @public std::unique_ptr<BootParameters> m_boot_parameters;
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
@property (weak, nonatomic) IBOutlet UIButton* m_pull_down_button;

@property(nonatomic) bool m_first_appear_done;
@property(nonatomic) UIView* m_renderer_view;
@property(nonatomic) bool m_is_wii;
@property(nonatomic) bool m_is_homebrew;
@property(nonatomic) DiscIO::Region m_ipl_region;
@property(nonatomic) int m_ts_active_port;
@property(nonatomic) DOLTopBarPullDownMode m_pull_down_mode;
@property(weak, nonatomic) TCView* m_ts_active_view;
@property(nonatomic) UIBarButtonItem* m_stop_button;
@property(nonatomic) UIBarButtonItem* m_pause_button;
@property(nonatomic) UIBarButtonItem* m_play_button;

- (void)RunningTitleUpdated;
- (void)PopulatePortDictionary;
- (void)ChangeVisibleTouchControllerToPort:(int)port;

@end

NS_ASSUME_NONNULL_END
