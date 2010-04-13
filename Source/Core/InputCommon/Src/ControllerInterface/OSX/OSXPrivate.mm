#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <Cocoa/Cocoa.h>

#include "OSXPrivate.h"
NSWindow* m_Window;
void OSX_Init(void *_View)
{
	// _View is really a wxNSView
	//m_Window is really a wxNSWindow
	NSView *View = (NSView*)_View;
	m_Window = [View window];
}

void OSX_UpdateKeys( int max, char* keys )
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSEvent *event = [[NSEvent alloc] init];
	
	/*event = [m_Window nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES ];
	
	if ( event != nil ) {
		switch ([event type]) {
			case NSKeyDown:
			//case NSKeyUp:
			//case NSFlagsChanged: // For Command
				memcpy(keys, [[event characters] UTF8String], max);
				break;
			default:
				[m_Window sendEvent:event];
				break;
		}
	}*/
	
	[event release];
	[pool release];
}