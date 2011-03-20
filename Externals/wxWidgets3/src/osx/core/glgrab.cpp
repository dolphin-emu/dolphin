/*
 IMPORTANT: This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation or modification
 of this Apple software constitutes acceptance of these terms. If you do not agree with these terms,
 please do not use, install or modify this Apple software.

 In consideration of your agreement to abide by the following terms, and subject to these
 terms, Apple grants you a personal, non-exclusive license, under Apple\xd5s copyrights in
 this original Apple software (the "Apple Software"), to use and modify the Apple Software,
 with or without modifications, in source and/or binary forms; provided that if you redistribute
 the Apple Software in its entirety and without modifications, you must retain this notice and
 the following text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used
 to endorse or promote products derived from the Apple Software without specific prior
 written permission from Apple. Except as expressly tated in this notice, no other rights or
 licenses, express or implied, are granted by Apple herein, including but not limited
 to any patent rights that may be infringed by your derivative works or by other works
 in which the Apple Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES,
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
          OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wx/wxprec.h"

#if wxOSX_USE_COCOA_OR_CARBON

#import <CoreFoundation/CoreFoundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

#include "wx/osx/private/glgrab.h"

extern CGColorSpaceRef wxMacGetGenericRGBColorSpace();

/*
 * perform an in-place swap from Quadrant 1 to Quadrant III format
 * (upside-down PostScript/GL to right side up QD/CG raster format)
 * We do this in-place, which requires more copying, but will touch
 * only half the pages.  (Display grabs are BIG!)
 *
 * Pixel reformatting may optionally be done here if needed.
 */
static void swizzleBitmap(void * data, int rowBytes, int height)
{
    int top, bottom;
    void * buffer;
    void * topP;
    void * bottomP;
    void * base;


    top = 0;
    bottom = height - 1;
    base = data;
    buffer = malloc(rowBytes);


    while ( top < bottom )
    {
        topP = (void *)((top * rowBytes) + (intptr_t)base);
        bottomP = (void *)((bottom * rowBytes) + (intptr_t)base);


        /*
         * Save and swap scanlines.
         *
         * This code does a simple in-place exchange with a temp buffer.
         * If you need to reformat the pixels, replace the first two bcopy()
         * calls with your own custom pixel reformatter.
         */
        bcopy( topP, buffer, rowBytes );
        bcopy( bottomP, topP, rowBytes );
        bcopy( buffer, bottomP, rowBytes );

        ++top;
        --bottom;
    }
    free( buffer );
}


/*
 * Given a display ID and a rectangle on that display, generate a CGImageRef
 * containing the display contents.
 *
 * srcRect is display-origin relative.
 *
 * This function uses a full screen OpenGL read-only context.
 * By using OpenGL, we can read the screen using a DMA transfer
 * when it's in millions of colors mode, and we can correctly read
 * a microtiled full screen OpenGL context, such as a game or full
 * screen video display.
 *
 * Returns a CGImageRef. When you are done with the CGImageRef, release it
 * using CFRelease().
 * Returns NULL on an error.
 */

CGImageRef grabViaOpenGL(CGDirectDisplayID display, CGRect srcRect)
{
    CGContextRef bitmap;
    CGImageRef image;
    void * data;
    long bytewidth;
    GLint width, height;
    long bytes;

    CGLContextObj    glContextObj;
    CGLPixelFormatObj pixelFormatObj ;
    GLint numPixelFormats ;
    CGLPixelFormatAttribute attribs[] =
    {
        kCGLPFAFullScreen,
        kCGLPFADisplayMask,
        (CGLPixelFormatAttribute) 0,    /* Display mask bit goes here */
        (CGLPixelFormatAttribute) 0
    } ;


    if ( display == kCGNullDirectDisplay )
        display = CGMainDisplayID();
    attribs[2] = (CGLPixelFormatAttribute) CGDisplayIDToOpenGLDisplayMask(display);


    /* Build a full-screen GL context */
    CGLChoosePixelFormat( attribs, &pixelFormatObj, &numPixelFormats );
    if ( pixelFormatObj == NULL )    // No full screen context support
        return NULL;
    CGLCreateContext( pixelFormatObj, NULL, &glContextObj ) ;
    CGLDestroyPixelFormat( pixelFormatObj ) ;
    if ( glContextObj == NULL )
        return NULL;


    CGLSetCurrentContext( glContextObj ) ;
    CGLSetFullScreen( glContextObj ) ;


    glReadBuffer(GL_FRONT);


    width = (GLint)srcRect.size.width;
    height = (GLint)srcRect.size.height;


    bytewidth = width * 4; // Assume 4 bytes/pixel for now
    bytewidth = (bytewidth + 3) & ~3; // Align to 4 bytes
    bytes = bytewidth * height; // width * height

    /* Build bitmap context */
    data = malloc(height * bytewidth);
    if ( data == NULL )
    {
        CGLSetCurrentContext( NULL );
        CGLClearDrawable( glContextObj ); // disassociate from full screen
        CGLDestroyContext( glContextObj ); // and destroy the context
        return NULL;
    }
    bitmap = CGBitmapContextCreate(data, width, height, 8, bytewidth,
                                   wxMacGetGenericRGBColorSpace(), kCGImageAlphaNoneSkipFirst /* XRGB */);


    /* Read framebuffer into our bitmap */
    glFinish(); /* Finish all OpenGL commands */
    glPixelStorei(GL_PACK_ALIGNMENT, 4); /* Force 4-byte alignment */
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

    /*
     * Fetch the data in XRGB format, matching the bitmap context.
     */
    glReadPixels((GLint)srcRect.origin.x, (GLint)srcRect.origin.y, width, height,
                 GL_BGRA,
#ifdef __BIG_ENDIAN__
                 GL_UNSIGNED_INT_8_8_8_8_REV, // for PPC
#else
                 GL_UNSIGNED_INT_8_8_8_8, // for Intel! http://lists.apple.com/archives/quartz-dev/2006/May/msg00100.html
#endif
                 data);
    /*
     * glReadPixels generates a quadrant I raster, with origin in the lower left
     * This isn't a problem for signal processing routines such as compressors,
     * as they can simply use a negative 'advance' to move between scanlines.
     * CGImageRef and CGBitmapContext assume a quadrant III raster, though, so we need to
     * invert it. Pixel reformatting can also be done here.
     */
    swizzleBitmap(data, bytewidth, height);


    /* Make an image out of our bitmap; does a cheap vm_copy of the bitmap */
    image = CGBitmapContextCreateImage(bitmap);

    /* Get rid of bitmap */
    CFRelease(bitmap);
    free(data);


    /* Get rid of GL context */
    CGLSetCurrentContext( NULL );
    CGLClearDrawable( glContextObj ); // disassociate from full screen
    CGLDestroyContext( glContextObj ); // and destroy the context

    /* Returned image has a reference count of 1 */
    return image;
}

#endif
