#import "cocoaApp.h"

@implementation NSApplication(i)
- (void)appRunning
{
	_running = 1;
}
@end

@interface cocoaAppDelegate : NSObject
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
@end

@implementation cocoaAppDelegate : NSObject
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	return NSTerminateCancel;
}
@end

void cocoaCreateApp()
{
        ProcessSerialNumber psn;

        if (!GetCurrentProcess(&psn)) {
                TransformProcessType(&psn,
			kProcessTransformToForegroundApplication);
                SetFrontProcess(&psn);
        }

        if (NSApp == nil) {
                [NSApplication sharedApplication];
                //TODO : Create menu
                [NSApp finishLaunching];
        }

        if ([NSApp delegate] == nil) {
                [NSApp setDelegate:[[cocoaAppDelegate alloc] init]];
        }

        [NSApp appRunning];
}

bool cocoaKeyCode(NSEvent *event)
{
	static bool CMDDown = false, QDown = false;
	bool Return = false;

	NSConnection *connec = [[NSConnection new] autorelease];

	[connec setRootObject: event];
	if ([connec registerName: @"DolphinCocoaEvent"] == NO)
	{
			//printf("error creating nsconnection\n");
	}
	
	if( [event type] != NSFlagsChanged )
	{
		const char *Keys = [[event characters] UTF8String];

		if( Keys[0] == 'q' && [event type] == NSKeyDown )
			QDown = true;
		if( Keys[0] == 'q' && [event type] == NSKeyUp )
			QDown = false;
	}
	else 
		if( [event modifierFlags] & NSCommandKeyMask )
			CMDDown = true;
		else 
			CMDDown = false;
		
	if( QDown && CMDDown )
		Return = true;

	return Return;
}

bool cocoaSendEvent(NSEvent *event)
{
	if ( event != nil ) {
		switch ([event type]) {
			case NSKeyDown:
			case NSKeyUp:
			case NSFlagsChanged: // For Command
				return cocoaKeyCode(event);
				break;
			default:
				[NSApp sendEvent:event];
				break;
			}
	}

	return false;
}
