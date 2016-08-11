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

#ifndef SFML_SFAPPDELEGATE_HPP
#define SFML_SFAPPDELEGATE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/iOS/WindowImplUIKit.hpp>
#include <UIKit/UIKit.h>
#include <CoreMotion/CoreMotion.h>


////////////////////////////////////////////////////////////
/// \brief Our custom application delegate
///
/// This class handles global application events.
///
////////////////////////////////////////////////////////////
@interface SFAppDelegate : NSObject<UIApplicationDelegate>

////////////////////////////////////////////////////////////
/// \brief Return the instance of the application delegate
///
////////////////////////////////////////////////////////////
+(SFAppDelegate*)getInstance;

////////////////////////////////////////////////////////////
/// \brief Show or hide the virtual keyboard
///
/// \param visible True to show, false to hide
///
////////////////////////////////////////////////////////////
- (void)setVirtualKeyboardVisible:(bool)visible;

////////////////////////////////////////////////////////////
/// \brief Get the current touch position for a given finger
///
/// \param index Finger index
///
/// \return Current touch position, or (-1, -1) if no touch
///
////////////////////////////////////////////////////////////
- (sf::Vector2i)getTouchPosition:(unsigned int)index;

////////////////////////////////////////////////////////////
/// \brief Receive an external touch begin notification
///
/// \param index    Finger index
/// \param position Position of the touch
///
////////////////////////////////////////////////////////////
- (void)notifyTouchBegin:(unsigned int)index atPosition:(sf::Vector2i)position;

////////////////////////////////////////////////////////////
/// \brief Receive an external touch move notification
///
/// \param index    Finger index
/// \param position Position of the touch
///
////////////////////////////////////////////////////////////
- (void)notifyTouchMove:(unsigned int)index atPosition:(sf::Vector2i)position;

////////////////////////////////////////////////////////////
/// \brief Receive an external touch end notification
///
/// \param index    Finger index
/// \param position Position of the touch
///
////////////////////////////////////////////////////////////
- (void)notifyTouchEnd:(unsigned int)index atPosition:(sf::Vector2i)position;

////////////////////////////////////////////////////////////
/// \brief Receive an external character notification
///
/// \param character The typed character
///
////////////////////////////////////////////////////////////
- (void)notifyCharacter:(sf::Uint32)character;

////////////////////////////////////////////////////////////
/// \brief Tells if the dimensions of the current window must be flipped when switching to a given orientation
///
/// \param orientation the device has changed to
///
////////////////////////////////////////////////////////////
- (bool)needsToFlipFrameForOrientation:(UIDeviceOrientation)orientation;

////////////////////////////////////////////////////////////
/// \brief Tells if app and view support a requested device orientation or not
///
/// \param orientation the device has changed to
///
////////////////////////////////////////////////////////////
- (bool)supportsOrientation:(UIDeviceOrientation)orientation;

////////////////////////////////////////////////////////////
/// \brief Initializes the factor which is required to convert from points to pixels and back
///
////////////////////////////////////////////////////////////
- (void)initBackingScale;

////////////////////////////////////////////////////////////
// Member data
////////////////////////////////////////////////////////////
@property (nonatomic) sf::priv::WindowImplUIKit* sfWindow; ///< Main window of the application
@property (readonly, nonatomic) CMMotionManager* motionManager; ///< Instance of the motion manager
@property (nonatomic) CGFloat backingScaleFactor;

@end

#endif // SFML_SFAPPDELEGATE_HPP

