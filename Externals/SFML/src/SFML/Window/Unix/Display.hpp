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

#ifndef SFML_SHAREDDISPLAY_HPP
#define SFML_SHAREDDISPLAY_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <X11/Xlib-xcb.h>
#include <string>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Get the shared Display
///
/// This function increments the reference count of the display,
/// it must be matched with a call to CloseDisplay.
///
/// \return Pointer to the shared display
///
////////////////////////////////////////////////////////////
Display* OpenDisplay();

////////////////////////////////////////////////////////////
/// \brief Get the xcb connection of the shared Display
///
/// This function increments the reference count of the display,
/// it must be matched with a call to CloseConnection.
///
/// \return Pointer to the shared connection
///
////////////////////////////////////////////////////////////
xcb_connection_t* OpenConnection();

////////////////////////////////////////////////////////////
/// \brief Release a reference to the shared display
///
/// \param display Display to release
///
////////////////////////////////////////////////////////////
void CloseDisplay(Display* display);

////////////////////////////////////////////////////////////
/// \brief Release a reference to the shared display
///
/// \param connection Connection of display to release
///
////////////////////////////////////////////////////////////
void CloseConnection(xcb_connection_t* connection);

////////////////////////////////////////////////////////////
/// \brief Get screen of a display by index (equivalent to XScreenOfDisplay)
///
/// \param connection Connection of display
/// \param screen_nbr The index of the screen
///
/// \return Pointer to the screen
///
////////////////////////////////////////////////////////////
xcb_screen_t* XCBScreenOfDisplay(xcb_connection_t* connection, int screen_nbr);

////////////////////////////////////////////////////////////
/// \brief Get default screen of a display (equivalent to XDefaultScreen)
///
/// \param connection Connection of display
///
/// \return Pointer to the default screen of the display
///
////////////////////////////////////////////////////////////
xcb_screen_t* XCBDefaultScreen(xcb_connection_t* connection);

////////////////////////////////////////////////////////////
/// \brief Get default root window of a display (equivalent to XDefaultRootWindow)
///
/// \param connection Connection of display
///
/// \return Root window of the display
///
////////////////////////////////////////////////////////////
xcb_window_t XCBDefaultRootWindow(xcb_connection_t* connection);

////////////////////////////////////////////////////////////
/// \brief Get the atom with the specified name
///
/// \param name         Name of the atom
/// \param onlyIfExists Don't try to create the atom if it doesn't already exist
///
/// \return Atom if it exists or XCB_ATOM_NONE (0) if it doesn't
///
////////////////////////////////////////////////////////////
xcb_atom_t getAtom(const std::string& name, bool onlyIfExists = false);

} // namespace priv

} // namespace sf


#endif // SFML_SHAREDDISPLAY_HPP
