////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.org)
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
#include <SFML/Window/iOS/SFView.hpp>
#include <SFML/Window/iOS/SFAppDelegate.hpp>
#include <SFML/System/Utf.hpp>
#include <QuartzCore/CAEAGLLayer.h>
#include <cstring>

@interface SFView()

@property (nonatomic) NSMutableArray* touches;

@end


@implementation SFView

@synthesize context;


////////////////////////////////////////////////////////////
-(BOOL)canBecomeFirstResponder
{
    return true;
}


////////////////////////////////////////////////////////////
- (BOOL)hasText
{
    return true;
}


////////////////////////////////////////////////////////////
- (void)deleteBackward
{
    [[SFAppDelegate getInstance] notifyCharacter:'\b'];
}


////////////////////////////////////////////////////////////
- (void)insertText:(NSString*)text
{
    // Convert the NSString to UTF-8
    const char* utf8 = [text UTF8String];

    // Then convert to UTF-32 and notify the application delegate of each new character
    const char* end = utf8 + std::strlen(utf8);
    while (utf8 < end)
    {
        sf::Uint32 character;
        utf8 = sf::Utf8::decode(utf8, end, character);
        [[SFAppDelegate getInstance] notifyCharacter:character];
    }
}


////////////////////////////////////////////////////////////
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        // find an empty slot for the new touch
        NSUInteger index = [self.touches indexOfObject:[NSNull null]];
        if (index != NSNotFound)
        {
            [self.touches replaceObjectAtIndex:index withObject:touch];
        }
        else
        {
            [self.touches addObject:touch];
            index = [self.touches count] - 1;
        }

        // get the touch position
        CGPoint point = [touch locationInView:self];
        sf::Vector2i position(static_cast<int>(point.x), static_cast<int>(point.y));

        // notify the application delegate
        [[SFAppDelegate getInstance] notifyTouchBegin:index atPosition:position];
    }
}


////////////////////////////////////////////////////////////
- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        // find the touch
        NSUInteger index = [self.touches indexOfObject:touch];
        if (index != NSNotFound)
        {
            // get the touch position
            CGPoint point = [touch locationInView:self];
            sf::Vector2i position(static_cast<int>(point.x), static_cast<int>(point.y));

            // notify the application delegate
            [[SFAppDelegate getInstance] notifyTouchMove:index atPosition:position];
        }
    }
}


////////////////////////////////////////////////////////////
- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event
{
    for (UITouch* touch in touches)
    {
        // find the touch
        NSUInteger index = [self.touches indexOfObject:touch];
        if (index != NSNotFound)
        {
            // get the touch position
            CGPoint point = [touch locationInView:self];
            sf::Vector2i position(static_cast<int>(point.x), static_cast<int>(point.y));

            // notify the application delegate
            [[SFAppDelegate getInstance] notifyTouchEnd:index atPosition:position];

            // remove the touch
            [self.touches replaceObjectAtIndex:index withObject:[NSNull null]];
        }
    }
}


////////////////////////////////////////////////////////////
- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event
{
    // Treat touch cancel events the same way as touch end
    [self touchesEnded:touches withEvent:event];
}


////////////////////////////////////////////////////////////
- (void)layoutSubviews
{
    // update the attached context's buffers
    if (self.context)
        self.context->recreateRenderBuffers(self);
}


////////////////////////////////////////////////////////////
+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

////////////////////////////////////////////////////////////
- (id)initWithFrame:(CGRect)frame andContentScaleFactor:(CGFloat)factor
{
    self = [super initWithFrame:frame];

    self.contentScaleFactor = factor;

    if (self)
    {
        self.context = NULL;
        self.touches = [NSMutableArray array];

        // Configure the EAGL layer
        CAEAGLLayer* eaglLayer = (CAEAGLLayer*)self.layer;
        eaglLayer.opaque = YES;
        eaglLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithBool:FALSE], kEAGLDrawablePropertyRetainedBacking,
                                        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                                        nil];

        // Enable user interactions on the view (multi-touch events)
        self.userInteractionEnabled = true;
        self.multipleTouchEnabled = true;
    }

    return self;
}


@end
