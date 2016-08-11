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
#import <SFML/Window/OSX/NSImage+raw.h>

@implementation NSImage (raw)

+(NSImage*)imageWithRawData:(const sf::Uint8*)pixels andSize:(NSSize)size
{
    // Create an empty image representation.
    NSBitmapImageRep* bitmap =
    [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:0 // if 0: only allocate memory
                                            pixelsWide:size.width
                                            pixelsHigh:size.height
                                         bitsPerSample:8 // The number of bits used to specify
                                                         // one pixel in a single component of the data.
                                       samplesPerPixel:4 // 3 if no alpha, 4 with it
                                              hasAlpha:YES
                                              isPlanar:NO   // I don't know what it is but it works
                                        colorSpaceName:NSCalibratedRGBColorSpace
                                           bytesPerRow:0    // 0 == determine automatically
                                          bitsPerPixel:0];  // 0 == determine automatically

    // Load data pixels.
    for (unsigned int y = 0; y < size.height; ++y)
    {
        for (unsigned int x = 0; x < size.width; ++x, pixels += 4)
        {
            NSUInteger pixel[4] = { pixels[0], pixels[1], pixels[2], pixels[3] };
            [bitmap setPixel:pixel atX:x y:y];
        }
    }

    // Create an image from the representation.
    NSImage* image = [[NSImage alloc] initWithSize:size];
    [image addRepresentation:bitmap];

    [bitmap release];

    return image;
}

@end
