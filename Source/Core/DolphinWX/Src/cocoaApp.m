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

void cocoaKeyCode(NSEvent *event)
{

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	NSConnection *connec = [NSConnection defaultConnection];

        [connec setRootObject: event];
        if ([connec registerName: @"DolphinCocoaEvent"] == NO)
        {
                printf("error creating nsconnection\n");
        }

	[pool release];


}

void cocoaSendEvent(NSEvent *event)
{

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	if ( event != nil ) {
		switch ([event type]) {
			case NSKeyDown:
				cocoaKeyCode(event);
				break;
			case NSKeyUp:
				cocoaKeyCode(nil);
				break;
			default:
				[NSApp sendEvent:event];
				break;
			}
	}


	[pool release];

}


