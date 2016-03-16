/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/colour.mm
// Purpose:     Conversions between NSColor and wxColour
// Author:      Vadim Zeitlin
// Created:     2015-11-26 (completely replacing the old version of the file)
// Copyright:   (c) 2015 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/colour.h"

#include "wx/osx/private.h"

// Helper function to avoid writing too many casts in wxColour ctor.
static inline wxColour::ChannelType NSColorChannelToWX(CGFloat c)
{
    return static_cast<wxColour::ChannelType>(c * 255 + 0.5);
}

wxColour::wxColour(WX_NSColor col)
{
    // Simplest case is when we can directly get the RGBA components:
    if ( NSColor* colRGBA = [col colorUsingColorSpaceName:NSCalibratedRGBColorSpace] )
    {
        InitRGBA
        (
             NSColorChannelToWX([colRGBA redComponent]),
             NSColorChannelToWX([colRGBA greenComponent]),
             NSColorChannelToWX([colRGBA blueComponent]),
             NSColorChannelToWX([colRGBA alphaComponent])
        );
        return;
    }

    // Some colours use patterns, we can handle them with the help of CGColorRef
    if ( NSColor* colPat = [col colorUsingColorSpaceName:NSPatternColorSpace] )
    {
        NSImage* const nsimage = [colPat patternImage];
        if ( nsimage )
        {
            NSSize size = [nsimage size];
            NSRect r = NSMakeRect(0, 0, size.width, size.height);
            CGImageRef cgimage = [nsimage CGImageForProposedRect:&r context:nil hints:nil];
            if ( cgimage )
            {
                // Callbacks for CGPatternCreate()
                struct PatternCreateCallbacks
                {
                    static void Draw(void *info, CGContextRef ctx)
                    {
                        CGImageRef image = (CGImageRef) info;
                        CGContextDrawImage
                        (
                            ctx,
                            CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)),
                            image
                        );
                    }

                    static void Release(void * WXUNUSED(info))
                    {
                        // Do not release the image here, we don't own it as it
                        // comes from NSImage.
                    }
                };

                const CGPatternCallbacks callbacks =
                {
                    /* version: */ 0,
                    &PatternCreateCallbacks::Draw,
                    &PatternCreateCallbacks::Release
                };

                CGPatternRef pattern = CGPatternCreate
                                       (
                                            cgimage,
                                            CGRectMake(0, 0, size.width, size.height),
                                            CGAffineTransformMake(1, 0, 0, 1, 0, 0),
                                            size.width,
                                            size.height,
                                            kCGPatternTilingConstantSpacing,
                                            /* isColored: */ true,
                                            &callbacks
                                       );
                CGColorSpaceRef space = CGColorSpaceCreatePattern(NULL);
                CGFloat components[1] = { 1.0 };
                CGColorRef cgcolor = CGColorCreateWithPattern(space, pattern, components);
                CGColorSpaceRelease(space);
                CGPatternRelease(pattern);

                InitCGColorRef(cgcolor);
                return;
            }
        }
    }

    // Don't assert here, this will more likely than not result in a crash as
    // colours are often created in drawing code which will be called again
    // when the assert dialog is shown, resulting in a recursive assertion
    // failure and, hence, a crash.
    NSLog(@"Failed to convert NSColor \"%@\" to wxColour.", col);
}

WX_NSColor wxColour::OSXGetNSColor() const
{
    return [NSColor colorWithCalibratedRed:m_red / 255.0 green:m_green / 255.0 blue:m_blue / 255.0 alpha:m_alpha / 255.0];
}
