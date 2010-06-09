////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2009 Laurent Gomila (laurent.gom@gmail.com)
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

#ifndef SFML_SELECTORBASE_HPP
#define SFML_SELECTORBASE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <SFML/Network/SocketHelper.hpp>
#include <map>


namespace sf
{
////////////////////////////////////////////////////////////
/// Private base class for selectors.
/// As Selector is a template class, this base is needed so that
/// every system call get compiled in SFML (not inlined)
////////////////////////////////////////////////////////////
class SFML_API SelectorBase
{
public :

    ////////////////////////////////////////////////////////////
    /// Default constructor
    ///
    ////////////////////////////////////////////////////////////
    SelectorBase();

    ////////////////////////////////////////////////////////////
    /// Add a socket to watch
    ///
    /// \param Socket : Socket to add
    ///
    ////////////////////////////////////////////////////////////
    void Add(SocketHelper::SocketType Socket);

    ////////////////////////////////////////////////////////////
    /// Remove a socket
    ///
    /// \param Socket : Socket to remove
    ///
    ////////////////////////////////////////////////////////////
    void Remove(SocketHelper::SocketType Socket);

    ////////////////////////////////////////////////////////////
    /// Remove all sockets
    ///
    ////////////////////////////////////////////////////////////
    void Clear();

    ////////////////////////////////////////////////////////////
    /// Wait and collect sockets which are ready for reading.
    /// This functions will return either when at least one socket
    /// is ready, or when the given time is out
    ///
    /// \param Timeout : Timeout, in seconds (0 by default : no timeout)
    ///
    /// \return Number of sockets ready to be read
    ///
    ////////////////////////////////////////////////////////////
    unsigned int Wait(float Timeout = 0.f);

    ////////////////////////////////////////////////////////////
    /// After a call to Wait(), get the Index-th socket which is
    /// ready for reading. The total number of sockets ready
    /// is the integer returned by the previous call to Wait()
    ///
    /// \param Index : Index of the socket to get
    ///
    /// \return The Index-th socket
    ///
    ////////////////////////////////////////////////////////////
    SocketHelper::SocketType GetSocketReady(unsigned int Index);

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    fd_set mySet;       ///< Set of socket to watch
    fd_set mySetReady;  ///< Set of socket which are ready for reading
    int    myMaxSocket; ///< Maximum socket index
};

} // namespace sf


#endif // SFML_SELECTORBASE_HPP
