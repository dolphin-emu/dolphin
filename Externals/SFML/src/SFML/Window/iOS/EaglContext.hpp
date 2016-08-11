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

#ifndef SFML_EAGLCONTEXT_HPP
#define SFML_EAGLCONTEXT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/GlContext.hpp>
#include <SFML/Window/iOS/ObjCType.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Clock.hpp>
#include <OpenGLES/ES1/gl.h>


SFML_DECLARE_OBJC_CLASS(EAGLContext);
SFML_DECLARE_OBJC_CLASS(SFView);

namespace sf
{
namespace priv
{
class WindowImplUIKit;

////////////////////////////////////////////////////////////
/// \brief iOS (EAGL) implementation of OpenGL contexts
///
////////////////////////////////////////////////////////////
class EaglContext : public GlContext
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context, not associated to a window
    ///
    /// \param shared Context to share the new one with (can be NULL)
    ///
    ////////////////////////////////////////////////////////////
    EaglContext(EaglContext* shared);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context attached to a window
    ///
    /// \param shared       Context to share the new one with
    /// \param settings     Creation parameters
    /// \param owner        Pointer to the owner window
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    EaglContext(EaglContext* shared, const ContextSettings& settings,
                const WindowImpl* owner, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context that embeds its own rendering target
    ///
    /// \param shared   Context to share the new one with
    /// \param settings Creation parameters
    /// \param width    Back buffer width, in pixels
    /// \param height   Back buffer height, in pixels
    ///
    ////////////////////////////////////////////////////////////
    EaglContext(EaglContext* shared, const ContextSettings& settings,
                unsigned int width, unsigned int height);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~EaglContext();

    ////////////////////////////////////////////////////////////
    /// \brief Recreate the render buffers of the context
    ///
    /// This function must be called whenever the containing view
    /// changes (typically after an orientation change)
    ///
    /// \param glView: Container of the context
    ///
    ////////////////////////////////////////////////////////////
    void recreateRenderBuffers(SFView* glView);

    ////////////////////////////////////////////////////////////
    /// \brief Display what has been rendered to the context so far
    ///
    ////////////////////////////////////////////////////////////
    virtual void display();

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable vertical synchronization
    ///
    /// Activating vertical synchronization will limit the number
    /// of frames displayed to the refresh rate of the monitor.
    /// This can avoid some visual artifacts, and limit the framerate
    /// to a good value (but not constant across different computers).
    ///
    /// \param enabled: True to enable v-sync, false to deactivate
    ///
    ////////////////////////////////////////////////////////////
    virtual void setVerticalSyncEnabled(bool enabled);

protected:

    ////////////////////////////////////////////////////////////
    /// \brief Activate the context as the current target
    ///        for rendering
    ///
    /// \return True on success, false if any error happened
    ///
    ////////////////////////////////////////////////////////////
    virtual bool makeCurrent();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Create the context
    ///
    /// \param shared       Context to share the new one with (can be NULL)
    /// \param window       Window to attach the context to (can be NULL)
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    /// \param settings     Creation parameters
    ///
    ////////////////////////////////////////////////////////////
    void createContext(EaglContext* shared,
                       const WindowImplUIKit* window,
                       unsigned int bitsPerPixel,
                       const ContextSettings& settings);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    EAGLContext* m_context; ///< The internal context
    GLuint m_framebuffer;   ///< Frame buffer associated to the context
    GLuint m_colorbuffer;   ///< Color render buffer
    GLuint m_depthbuffer;   ///< Depth render buffer
    bool m_vsyncEnabled;    ///< Vertical sync activation flag
    Clock m_clock;          ///< Measures the elapsed time for the fake v-sync implementation
};

} // namespace priv

} // namespace sf

#endif // SFML_EAGLCONTEXT_HPP
