#import <Cocoa/Cocoa.h>
#import <OpenGL/CGLRenderers.h>

#ifdef __cplusplus
extern "C"
{
#endif


void cocoaGLCreateApp();

NSWindow *cocoaGLCreateWindow(int w,int h);

void cocoaGLSetTitle(NSWindow *win, const char *title);

void cocoaGLMakeCurrent(NSOpenGLContext *ctx, NSWindow *win);

NSOpenGLContext* cocoaGLInit(int mode);

void cocoaGLDelete(NSOpenGLContext *ctx);

void cocoaGLSwap(NSOpenGLContext *ctx,NSWindow *window);

#ifdef __cplusplus
}
#endif

