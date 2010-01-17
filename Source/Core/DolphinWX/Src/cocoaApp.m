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
        NSAutoreleasePool *pool;

        if (!GetCurrentProcess(&psn)) {
                TransformProcessType(&psn, kProcessTransformToForegroundApplication);
                SetFrontProcess(&psn);
        }

        pool = [[NSAutoreleasePool alloc] init];

        if (NSApp == nil) {
                [NSApplication sharedApplication];
                //TODO : Create menu
                [NSApp finishLaunching];
        }

        if ([NSApp delegate] == nil) {
                [NSApp setDelegate:[[cocoaAppDelegate alloc] init]];
        }

        [NSApp appRunning];

        [pool release];

}

bool cocoaKeyCode(NSEvent *event)
{
	static bool CMDDown = false, QDown = false;
	bool Return = false;
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	NSConnection *connec = [NSConnection defaultConnection];

	[connec setRootObject: event];
	if ([connec registerName: @"DolphinCocoaEvent"] == NO)
	{
			//printf("error creating nsconnection\n");
	}
	
	if( [event type] != NSFlagsChanged )
	{
		NSString *NewString = [event characters];
		char *Keys = [NewString UTF8String];

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

	[pool release];
	return Return;
}

bool cocoaSendEvent(NSEvent *event)
{

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
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


	[pool release];
	return false;

}


