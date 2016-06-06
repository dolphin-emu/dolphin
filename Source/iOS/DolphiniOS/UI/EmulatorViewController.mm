//
//  ViewController.m
//  DolphiniOS
//
//  Created by Will Cobb on 5/20/16.
//
//

#import "UI/EmulatorViewController.h"

#import "Bridge/DolphinBridge.h"
#import "Bridge/DolphinGame.h"

#import "Controller/GCControllerView.h"

#import <GLKit/GLKit.h>
#import <OpenGLES/ES2/gl.h>

#include "Common/GL/GLInterfaceBase.h"

#include "InputCommon/ControllerInterface/iOS/iOSButtonManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "Core/HW/GCPadEmu.h"

@interface EmulatorViewController () <GLKViewDelegate, GCControllerViewDelegate> {
	DolphinBridge*		bridge;
	GCControllerView*	controllerView;
	u16 buttonState;
	CGPoint joyData[2];
}

@property (strong, nonatomic) EAGLContext* context;

@end

@implementation EmulatorViewController

GLKView* v;

- (void)viewDidLoad {
	[super viewDidLoad];

	// Add GLKView
	CGSize screenSize = [self currentScreenSizeAlwaysLandscape:YES];
	CGSize emulatorSize = CGSizeMake(screenSize.height * 1.21212, screenSize.height);
	self.glkView = [[GLKView alloc] initWithFrame:CGRectMake((screenSize.width - emulatorSize.width)/2, 0, emulatorSize.width, emulatorSize.height)];
	[self.view addSubview:self.glkView];

	v = self.glkView;
	v.delegate = self;

	NSLog(@"Loaded %@", self.glkView.delegate);
	// Do any additional setup after loading the view, typically from a nib.
	bridge = [DolphinBridge new];

	// Add controller View
	controllerView = [[GCControllerView alloc] initWithFrame:CGRectMake(0, 0, screenSize.width, screenSize.height)];
	controllerView.delegate = self;
	[self.view addSubview:controllerView];
}

- (void)launchGame:(DolphinGame* )game
{
	NSString* userDir = [bridge getUserDirectory];
	if (userDir.length == 0)
	{
		// let's setup everything
		NSString* docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0];
		[bridge setUserDirectory:[docDir stringByAppendingString:@"/Dolphin"]];
		[bridge createUserFolders];
		[bridge copyResources];
		[bridge saveDefaultPreferences];
	}
	[bridge performSelectorInBackground:@selector(openRomAtPath:) withObject:game.path];
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

-(CGSize)currentScreenSizeAlwaysLandscape:(BOOL)portrait
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

-(BOOL) isPortrait
{
	return UIInterfaceOrientationIsPortrait([UIApplication sharedApplication].statusBarOrientation);
}

- (void)didReceiveMemoryWarning
{
	[super didReceiveMemoryWarning];
}

void* Host_GetRenderHandle()
{
	printf("Asking for window handle\n");
	return (__bridge void*)v;
}

@end
