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

#ifndef SFML_CONTEXT_HPP
#define SFML_CONTEXT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Export.hpp>
#include <SFML/Window/GlResource.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
namespace priv
{
    class GlContext;
}

typedef void (*GlFunctionPointer)();

////////////////////////////////////////////////////////////
/// \brief Class holding a valid drawing context
///
////////////////////////////////////////////////////////////
class SFML_WINDOW_API Context : GlResource, NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// The constructor creates and activates the context
    ///
    ////////////////////////////////////////////////////////////
    Context();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    /// The destructor deactivates and destroys the context
    ///
    ////////////////////////////////////////////////////////////
    ~Context();

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate explicitly the context
    ///
    /// \param active True to activate, false to deactivate
    ///
    /// \return True on success, false on failure
    ///
    ////////////////////////////////////////////////////////////
    bool setActive(bool active);

    ////////////////////////////////////////////////////////////
    /// \brief Get the settings of the context
    ///
    /// Note that these settings may be different than the ones
    /// passed to the constructor; they are indeed adjusted if the
    /// original settings are not directly supported by the system.
    ///
    /// \return Structure containing the settings
    ///
    ////////////////////////////////////////////////////////////
    const ContextSettings& getSettings() const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a given OpenGL extension is available
    ///
    /// \param name Name of the extension to check for
    ///
    /// \return True if available, false if unavailable
    ///
    ////////////////////////////////////////////////////////////
    static bool isExtensionAvailable(const char* name);

    ////////////////////////////////////////////////////////////
    /// \brief Get the address of an OpenGL function
    ///
    /// \param name Name of the function to get the address of
    ///
    /// \return Address of the OpenGL function, 0 on failure
    ///
    ////////////////////////////////////////////////////////////
    static GlFunctionPointer getFunction(const char* name);

    ////////////////////////////////////////////////////////////
    /// \brief Get the currently active context
    ///
    /// \return The currently active context or NULL if none is active
    ///
    ////////////////////////////////////////////////////////////
    static const Context* getActiveContext();

    ////////////////////////////////////////////////////////////
    /// \brief Construct a in-memory context
    ///
    /// This constructor is for internal use, you don't need
    /// to bother with it.
    ///
    /// \param settings Creation parameters
    /// \param width    Back buffer width
    /// \param height   Back buffer height
    ///
    ////////////////////////////////////////////////////////////
    Context(const ContextSettings& settings, unsigned int width, unsigned int height);

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    priv::GlContext* m_context; ///< Internal OpenGL context
};

} // namespace sf


#endif // SFML_CONTEXT_HPP

////////////////////////////////////////////////////////////
/// \class sf::Context
/// \ingroup window
///
/// If you need to make OpenGL calls without having an
/// active window (like in a thread), you can use an
/// instance of this class to get a valid context.
///
/// Having a valid context is necessary for *every* OpenGL call.
///
/// Note that a context is only active in its current thread,
/// if you create a new thread it will have no valid context
/// by default.
///
/// To use a sf::Context instance, just construct it and let it
/// live as long as you need a valid context. No explicit activation
/// is needed, all it has to do is to exist. Its destructor
/// will take care of deactivating and freeing all the attached
/// resources.
///
/// Usage example:
/// \code
/// void threadFunction(void*)
/// {
///    sf::Context context;
///    // from now on, you have a valid context
///
///    // you can make OpenGL calls
///    glClear(GL_DEPTH_BUFFER_BIT);
/// }
/// // the context is automatically deactivated and destroyed
/// // by the sf::Context destructor
/// \endcode
///
////////////////////////////////////////////////////////////
