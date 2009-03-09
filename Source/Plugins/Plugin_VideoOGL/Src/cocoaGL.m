#import "cocoaGL.h"

NSWindow *cocoaGLCreateWindow(int w,int h)
{

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    	NSWindow *window;
    	window = [[NSWindow alloc] initWithContentRect:NSMakeRect(50,50,w,h)
        				styleMask:NSTitledWindowMask | NSResizableWindowMask
        				backing:NSBackingStoreBuffered
        				defer:FALSE];

	[window setTitle:@"Dolphin on OSX"];
	[window makeKeyAndOrderFront: nil];

	[pool release];

	return window;
}

void cocoaGLSetTitle(NSWindow *win, const char *title)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [win setTitle: [[[NSString alloc] initWithCString: title encoding: NSASCIIStringEncoding] autorelease]];

    [pool release];
}

void cocoaGLMakeCurrent(NSOpenGLContext *ctx, NSWindow *win)
{
    	NSAutoreleasePool *pool;

  	pool = [[NSAutoreleasePool alloc] init];

	int value = 0;
        [ctx setValues:&value forParameter:NSOpenGLCPSwapInterval];

    	if (ctx) {

            	[ctx setView:[win contentView]];
            	[ctx update];
        	[ctx makeCurrentContext];
	} else {
        	[NSOpenGLContext clearCurrentContext];
    	}

    	[pool release];

}



NSOpenGLContext* cocoaGLInit(int mode)
{
   	NSAutoreleasePool *pool;
    	NSOpenGLPixelFormatAttribute attr[32];
    	NSOpenGLPixelFormat *fmt;
    	NSOpenGLContext *context;
    	int i = 0;

	pool = [[NSAutoreleasePool alloc] init];
	
	attr[i++] = NSOpenGLPFADepthSize;
	attr[i++] = 24;
	attr[i++] = NSOpenGLPFADoubleBuffer;
	
        attr[i++] = NSOpenGLPFASampleBuffers;
        attr[i++] = mode;
        attr[i++] = NSOpenGLPFASamples;
        attr[i++] = 1;
#ifdef GL_VERSION_1_2
	#warning "your car support ogl 1.2, dolphin wil use software renderer"
	//if opengl < 1.3 uncomment this twoo lines to use software renderer
        attr[i++] = NSOpenGLPFARendererID;
        attr[i++] = kCGLRendererGenericFloatID;
#endif
    	attr[i++] = NSOpenGLPFAScreenMask;
    	attr[i++] = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());

    	attr[i] = 0;

    	fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
   	if (fmt == nil) {
        	printf("failed to create pixel format\n");
        	[pool release];
        	return NULL;
    	}

    	context = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];

    	[fmt release];

    	if (context == nil) {
        	printf("failed to create context\n");
        	[pool release];
       	 	return NULL;
    	}

    	[pool release];

    	return context;

}

void cocoaGLDelete(NSOpenGLContext *ctx)
{
    	NSAutoreleasePool *pool;

    	pool = [[NSAutoreleasePool alloc] init];

    	[ctx clearDrawable];
    	[ctx release];

    	[pool release];

}

void cocoaGLSwap(NSOpenGLContext *ctx,NSWindow *window)
{
    	NSAutoreleasePool *pool;

    	pool = [[NSAutoreleasePool alloc] init];
	[window makeKeyAndOrderFront: nil];

    	ctx = [NSOpenGLContext currentContext];
   	if (ctx != nil) {
        	[ctx flushBuffer];
    	}
	else
	{
		printf("bad cocoa gl ctx\n");
	}
    	[pool release];

}
