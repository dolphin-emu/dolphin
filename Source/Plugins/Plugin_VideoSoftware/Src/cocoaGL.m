#import "cocoaGL.h"

NSWindow *cocoaGLCreateWindow(int w, int h)
{
	NSWindow *window;
	window = [[NSWindow alloc] initWithContentRect: NSMakeRect(50, 50, w, h)
			styleMask: NSTitledWindowMask | NSResizableWindowMask
			backing: NSBackingStoreBuffered defer: FALSE];
	[window setReleasedWhenClosed: YES];

	[window setTitle:@"Dolphin on OSX"];
	//[window makeKeyAndOrderFront: nil];

	return window;
}

void cocoaGLSetTitle(NSWindow *win, const char *title)
{
	[win setTitle: [[[NSString alloc] initWithCString: title
			encoding: NSASCIIStringEncoding] autorelease]];
}

void cocoaGLMakeCurrent(NSOpenGLContext *ctx, NSWindow *win)
{
	int value = 0;
	[ctx setValues:&value forParameter:NSOpenGLCPSwapInterval];

	if (ctx) {
		[ctx setView:[win contentView]];
		[ctx update];
		[ctx makeCurrentContext];
	}
	else
		[NSOpenGLContext clearCurrentContext];
}

NSOpenGLContext* cocoaGLInit(int mode)
{
	NSOpenGLPixelFormatAttribute attr[32];
	NSOpenGLPixelFormat *fmt;
	NSOpenGLContext *context;
	int i = 0;

	attr[i++] = NSOpenGLPFADepthSize;
	attr[i++] = 24;
	attr[i++] = NSOpenGLPFADoubleBuffer;

	attr[i++] = NSOpenGLPFASampleBuffers;
	attr[i++] = mode;
	attr[i++] = NSOpenGLPFASamples;
	attr[i++] = 1;

	attr[i++] = NSOpenGLPFANoRecovery;
#ifdef GL_VERSION_1_3

#else
#ifdef GL_VERSION_1_2
#warning "your card only supports ogl 1.2, dolphin will use software renderer"
	//if opengl < 1.3 uncomment this twoo lines to use software renderer
	attr[i++] = NSOpenGLPFARendererID;
	attr[i++] = kCGLRendererGenericFloatID;
#endif
#endif
	attr[i++] = NSOpenGLPFAScreenMask;
	attr[i++] = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());

	attr[i] = 0;

	fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
	if (fmt == nil) {
		printf("failed to create pixel format\n");
		return NULL;
	}

	context = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];

	[fmt release];

	if (context == nil) {
		printf("failed to create context\n");
		return NULL;
	}

	return context;
}

void cocoaGLDelete(NSOpenGLContext *ctx)
{
	[ctx clearDrawable];
	[ctx release];
}

void cocoaGLDeleteWindow(NSWindow *window)
{
	[window close];

	return;
}

void cocoaGLSwap(NSOpenGLContext *ctx, NSWindow *window)
{
	[window makeKeyAndOrderFront: nil];

	ctx = [NSOpenGLContext currentContext];
	if (ctx != nil)
		[ctx flushBuffer];
	else
		printf("bad cocoa gl ctx\n");
}
