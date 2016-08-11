////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Laurent Gomila (laurent@sfml-dev.prg)
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
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
// iOS specific: SFML needs to hook the main function, to
// launch the iOS application (event loop), and then call the
// user main from inside it.
//
// Our strategy is to rename the user main to 'sfmlMain' with
// a macro (see Main.hpp), and call this modified main ourselves.
//
// Note that half of this trick (the sfmlMain placeholders and
// the application delegate) is defined sfml-window; see there
// for the full implementation.
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>

#ifdef SFML_SYSTEM_IOS

#include <UIKit/UIKit.h>


////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
    // Note: we intentionally drop command line arguments,
    // there's no such thing as a command line on an iOS device :)

    // Important: "SFAppDelegate" must always match the name of the
    // application delegate class defined in sfml-window

    return UIApplicationMain(argc, argv, nil, @"SFAppDelegate");
}

#endif // SFML_SYSTEM_IOS
