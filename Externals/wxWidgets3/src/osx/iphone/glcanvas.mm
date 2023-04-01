///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/iphone/glcanvas.mm
// Purpose:     wxGLCanvas, for using OpenGL with wxWidgets under iPhone
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_GLCANVAS

#include "wx/glcanvas.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/osx/private.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

// a lot of the code is from the OpenGL ES Template

// can be turned on for ES 2.0 only
#define USE_DEPTH_BUFFER 0

@interface wxUICustomOpenGLView : UIView
{
    CGRect oldRect;
    EAGLContext* context;

    /* The pixel dimensions of the backbuffer */
    GLint backingWidth;
    GLint backingHeight;

    /* OpenGL names for the renderbuffer and framebuffers used to render to this view */
    GLuint viewRenderbuffer, viewFramebuffer;
    
    /* OpenGL name for the depth buffer that is attached to viewFramebuffer, if it exists (0 if it does not exist) */
    GLuint depthRenderbuffer;
}

- (BOOL) createFramebuffer;
- (void) destroyFramebuffer;
- (id) initWithFrame:(CGRect) rect;

@end

@implementation wxUICustomOpenGLView

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized) 
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

- (id) initWithFrame:(CGRect)rect
{
    if ( !(self=[super initWithFrame:rect]) )
        return nil;
    
    oldRect = rect;
    return self;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)createFramebuffer {
    
    glGenFramebuffersOES(1, &viewFramebuffer);
    glGenRenderbuffersOES(1, &viewRenderbuffer);
    glEnable(GL_CULL_FACE);
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);
    
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);
    
    if (USE_DEPTH_BUFFER) {
        glGenRenderbuffersOES(1, &depthRenderbuffer);
        glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
        glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
        glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);
    }
    
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
        NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
        return NO;
    }
    
    return YES;
}


- (void)destroyFramebuffer {
    
    glDeleteFramebuffersOES(1, &viewFramebuffer);
    viewFramebuffer = 0;
    glDeleteRenderbuffersOES(1, &viewRenderbuffer);
    viewRenderbuffer = 0;
    
    if(depthRenderbuffer) {
        glDeleteRenderbuffersOES(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
}

- (void) setContext:(EAGLContext*) ctx {
    context = ctx;
    [EAGLContext setCurrentContext:ctx];

    if ( viewFramebuffer == 0 )
    {
        [self destroyFramebuffer];
        [self createFramebuffer];        
    }

    glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
}
 
- (void) swapBuffers {
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
}

- (void)layoutSubviews {
    [EAGLContext setCurrentContext:context];
    [self destroyFramebuffer];
    [self createFramebuffer];
}

@end


WXGLContext WXGLCreateContext( WXGLPixelFormat pixelFormat, WXGLContext shareContext )
{
    WXGLContext context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    
    // [[EAGLContext alloc] initWithFormat:pixelFormat shareContext: shareContext];
    if ( !context )
        wxFAIL_MSG("NSOpenGLContext creation failed");
    return context ;
}

void WXGLDestroyContext( WXGLContext context )
{
    if ( context )
    {
        [context release];
    }
}

WXGLContext WXGLGetCurrentContext()
{
    return [EAGLContext currentContext];
}

bool WXGLSetCurrentContext(WXGLContext context)
{
    [EAGLContext setCurrentContext:context];
    return true;
}

void WXGLDestroyPixelFormat( WXGLPixelFormat pixelFormat )
{
/*
    if ( pixelFormat )
    {
        [pixelFormat release];
    }
*/
}


WXGLPixelFormat WXGLChoosePixelFormat(const int *attribList)
{
#if 0
    NSOpenGLPixelFormatAttribute data[512];
    const NSOpenGLPixelFormatAttribute defaultAttribs[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAMinimumPolicy,
        NSOpenGLPFAColorSize,(NSOpenGLPixelFormatAttribute)8,
        NSOpenGLPFAAlphaSize,(NSOpenGLPixelFormatAttribute)0,
        NSOpenGLPFADepthSize,(NSOpenGLPixelFormatAttribute)8,
        (NSOpenGLPixelFormatAttribute)nil
    };

    const NSOpenGLPixelFormatAttribute *attribs;
    if ( !attribList )
    {
        attribs = defaultAttribs;
    }
    else
    {
        unsigned p = 0;
        data[p++] = NSOpenGLPFAMinimumPolicy; // make _SIZE tags behave more like GLX

        for ( unsigned arg = 0; attribList[arg] !=0 && p < WXSIZEOF(data); )
        {
            switch ( attribList[arg++] )
            {
                case WX_GL_RGBA:
                    //data[p++] = AGL_RGBA;
                    break;

                case WX_GL_BUFFER_SIZE:
                    //data[p++] = AGL_BUFFER_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_LEVEL:
                    //data[p++]=AGL_LEVEL;
                    //data[p++]=attribList[arg++];
                    break;

                case WX_GL_DOUBLEBUFFER:
                    data[p++] = NSOpenGLPFADoubleBuffer;
                    break;

                case WX_GL_STEREO:
                    data[p++] = NSOpenGLPFAStereo;
                    break;

                case WX_GL_AUX_BUFFERS:
                    data[p++] = NSOpenGLPFAAuxBuffers;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_MIN_RED:
                    data[p++] = NSOpenGLPFAColorSize;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_MIN_GREEN:
                    //data[p++] = AGL_GREEN_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_BLUE:
                    //data[p++] = AGL_BLUE_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ALPHA:
                    data[p++] = NSOpenGLPFAAlphaSize;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_DEPTH_SIZE:
                    data[p++] = NSOpenGLPFADepthSize;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_STENCIL_SIZE:
                    data[p++] = NSOpenGLPFAStencilSize;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_RED:
                    data[p++] = NSOpenGLPFAAccumSize;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_GREEN:
                    //data[p++] = AGL_ACCUM_GREEN_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_BLUE:
                    //data[p++] = AGL_ACCUM_BLUE_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_MIN_ACCUM_ALPHA:
                    //data[p++] = AGL_ACCUM_ALPHA_SIZE;
                    //data[p++] = attribList[arg++];
                    break;

                case WX_GL_SAMPLE_BUFFERS:
                    if ( !wxGLCanvas::IsAGLMultiSampleAvailable() )
                    {
                        if ( !attribList[arg++] )
                            break;

                        return false;
                    }

                    data[p++] = NSOpenGLPFASampleBuffers;
                    if ( (data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++]) == true )
                    {
                        // don't use software fallback
                        data[p++] = NSOpenGLPFANoRecovery;
                    }
                    break;

                case WX_GL_SAMPLES:
                    if ( !wxGLCanvas::IsAGLMultiSampleAvailable() )
                    {
                        if ( !attribList[arg++] )
                            break;

                        return false;
                    }

                    data[p++] = NSOpenGLPFASamples;
                    data[p++] = (NSOpenGLPixelFormatAttribute) attribList[arg++];
                    break;
            }
        }

        data[p] = (NSOpenGLPixelFormatAttribute)nil;

        attribs = data;
    }

    return [[NSOpenGLPixelFormat alloc] initWithAttributes:(NSOpenGLPixelFormatAttribute*) attribs];
#endif
    return NULL;
}

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !m_glContext )
        return false;  

    wxUICustomOpenGLView* v = (wxUICustomOpenGLView*) win.GetPeer()->GetWXWidget();
    [v setContext:m_glContext];
    return true;
}

#define USE_SEPARATE_VIEW 1

bool wxGLCanvas::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        const wxString& name,
                        const int *attribList,
                        const wxPalette& WXUNUSED(palette))
{
/*
    m_glFormat = WXGLChoosePixelFormat(attribList);
    if ( !m_glFormat )
        return false;
*/
#if USE_SEPARATE_VIEW
    DontCreatePeer();
#endif

    if ( !wxWindow::Create(parent, id, pos, size, style, name) )
        return false;

#if USE_SEPARATE_VIEW
    CGRect r = wxOSXGetFrameForControl( this, pos , size ) ;
    wxUICustomOpenGLView* v = [[wxUICustomOpenGLView alloc] initWithFrame:r];
    CAEAGLLayer* eaglLayer = (CAEAGLLayer*) v.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking, 
            kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat, nil];
            
    SetPeer(new wxWidgetIPhoneImpl( this, v ));

    MacPostControlCreate(pos, size) ;
#endif
    return true;
}

wxGLCanvas::~wxGLCanvas()
{
    if ( m_glFormat )
        WXGLDestroyPixelFormat(m_glFormat);
}

bool wxGLCanvas::SwapBuffers()
{
    WXGLContext context = WXGLGetCurrentContext();
    wxCHECK_MSG(context, false, wxT("should have current context"));

    wxUICustomOpenGLView* v = (wxUICustomOpenGLView*) GetPeer()->GetWXWidget();
    [v swapBuffers];
    return true;
}


#endif // wxUSE_GLCANVAS
