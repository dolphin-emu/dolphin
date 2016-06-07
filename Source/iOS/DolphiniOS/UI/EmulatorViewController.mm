// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "UI/EmulatorViewController.h"

#import "Bridge/DolphinBridge.h"
#import "Bridge/DolphinGame.h"

#import "Controller/GCControllerView.h"

#import <GLKit/GLKit.h>
#import <OpenGLES/ES2/gl.h>

#include "Common/GL/GLInterfaceBase.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/ControllerInterface/iOS/iOSButtonManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

@interface EmulatorViewController () <GLKViewDelegate, GCControllerViewDelegate> {
	DolphinBridge*		bridge;
	GCControllerView*	controllerView;
	u16 buttonState;
	CGPoint joyData[2];
}

@property (strong, nonatomic) EAGLContext* context;

@end

@implementation EmulatorViewController

- (void)viewDidLoad {
	[super viewDidLoad];

	self.view.backgroundColor = [UIColor blackColor];

	bridge = [DolphinBridge new];
	
	// Add GLKView
	CGSize screenSize = [self currentScreenSizeAlwaysLandscape:YES];
	CGSize emulatorSize = CGSizeMake(screenSize.height * 1.21212, screenSize.height);
	self.glkView = [[GLKView alloc] initWithFrame:CGRectMake((screenSize.width - emulatorSize.width)/2, 0, emulatorSize.width, emulatorSize.height)];
	[self.view addSubview:self.glkView];

	// Add controller View
	controllerView = [[GCControllerView alloc] initWithFrame:CGRectMake(0, 0, screenSize.width, screenSize.height)];
	controllerView.delegate = self;
	[self.view addSubview:controllerView];
}

- (void)launchGame:(DolphinGame* )game
{
	[bridge openRomAtPath:game.path inView:self.glkView];
	[self initController];
}

#pragma mark - Controller Delegate

// Create a new class to handle the controller later
- (void)joystick:(NSInteger)joyid movedToPosition:(CGPoint)joyPosition
{
	joyData[joyid] = joyPosition;
}

- (void)buttonStateChanged:(u16)bState
{
	buttonState = bState;
}

- (void)initController
{
}

#pragma mark - UIFunctions

- (NSUInteger)supportedInterfaceOrientations
{
	return UIInterfaceOrientationMaskLandscape;
}

- (CGSize)currentScreenSizeAlwaysLandscape:(BOOL)portrait
{
	if (!portrait)
		return [UIScreen mainScreen].bounds.size;
	// Get portrait size
	CGRect screenBounds = [UIScreen mainScreen].bounds ;
	CGFloat width = CGRectGetWidth(screenBounds);
	CGFloat height = CGRectGetHeight(screenBounds);
	if (![self isPortrait])
	{
		return CGSizeMake(width, height);
	}
	return CGSizeMake(height, width);
}

- (BOOL)isPortrait
{
	return UIInterfaceOrientationIsPortrait([UIApplication sharedApplication].statusBarOrientation);
}

- (void)didReceiveMemoryWarning
{
	[super didReceiveMemoryWarning];
}

@end
