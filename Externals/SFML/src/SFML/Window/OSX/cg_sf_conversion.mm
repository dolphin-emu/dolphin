////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Marco Antognini (antognini.marco@gmail.com),
//                         Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/OSX/cg_sf_conversion.hpp>
#include <SFML/System/Err.hpp>

#import <SFML/Window/OSX/Scaling.h>

namespace sf
{
namespace priv
{

////////////////////////////////////////////////////////////
size_t modeBitsPerPixel(CGDisplayModeRef mode)
{
    size_t bpp = 0; // no match

    // Compare encoding.
    CFStringRef pixEnc = CGDisplayModeCopyPixelEncoding(mode);
    if(CFStringCompare(pixEnc, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        bpp = 32;
    else if(CFStringCompare(pixEnc, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        bpp = 16;
    else if(CFStringCompare(pixEnc, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        bpp = 8;

    // Clean up memory.
    CFRelease(pixEnc);

    return bpp;
}


////////////////////////////////////////////////////////////
size_t displayBitsPerPixel(CGDirectDisplayID displayId)
{
    // Get the display mode.
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);

    // Get bpp for the mode.
    const size_t bpp = modeBitsPerPixel(mode);

    // Clean up Memory.
    CGDisplayModeRelease(mode);

    return bpp;
}


////////////////////////////////////////////////////////////
VideoMode convertCGModeToSFMode(CGDisplayModeRef cgmode)
{
    // The main documentation says the sizes returned by
    // CGDisplayModeGetWidth and CGDisplayModeGetHeight
    // are expressed in pixels. However, some additional
    // documentation [1] states they actually return
    // values in points starting with 10.8.
    //
    // We therefore needs to use the scaling factor to
    // convert the dimensions properly.
    //
    // [1]: "APIs for Supporting High Resolution" > "Additions and Changes for OS X v10.8"
    // https://developer.apple.com/library/mac/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/APIs/APIs.html#//apple_ref/doc/uid/TP40012302-CH5-SW27
    VideoMode mode(CGDisplayModeGetWidth(cgmode), CGDisplayModeGetHeight(cgmode), modeBitsPerPixel(cgmode));
    scaleOutWidthHeight(mode, nil);
    return mode;
}

} // namespace priv
} // namespace sf

