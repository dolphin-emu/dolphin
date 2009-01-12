#import "cocoaApp.h"
#import <Cocoa/Cocoa.h>

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

