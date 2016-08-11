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
#include <SFML/Window/iOS/SFMain.hpp>


// sfmlMain is called by the application delegate (SFAppDelegate).
//
// Since we don't know which prototype of main the user
// defines, we declare both versions of sfmlMain, but with
// the 'weak' attribute (GCC extension) so that the
// user-declared one will replace SFML's one at linking stage.
//
// If user defines main(argc, argv) then it will be called
// directly, if he defines main() then it will be called by
// our placeholder.
//
// The sfmlMain() version is never called, it is just defined
// to avoid a linker error if the user directly defines the
// version with arguments.
//
// See the sfml-main module for the other half of this
// initialization trick.


////////////////////////////////////////////////////////////
__attribute__((weak)) int sfmlMain(int, char**)
{
    return sfmlMain();
}


////////////////////////////////////////////////////////////
__attribute__((weak)) int sfmlMain()
{
    return 0;
}
