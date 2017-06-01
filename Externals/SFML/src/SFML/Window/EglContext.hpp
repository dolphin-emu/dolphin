////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
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

#ifndef SFML_EGLCONTEXT_HPP
#define SFML_EGLCONTEXT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/EGLCheck.hpp>
#include <SFML/Window/GlContext.hpp>
#include <SFML/OpenGL.hpp>


namespace sf
{
namespace priv
{
class EglContext : public GlContext
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context, not associated to a window
    ///
    /// \param shared Context to share the new one with (can be NULL)
    ///
    ////////////////////////////////////////////////////////////
    EglContext(EglContext* shared);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context attached to a window
    ///
    /// \param shared       Context to share the new one with
    /// \param settings     Creation parameters
    /// \param owner        Pointer to the owner window
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    EglContext(EglContext* shared, const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context that embeds its own rendering target
    ///
    /// \param shared   Context to share the new one with
    /// \param settings Creation parameters
    /// \param width    Back buffer width, in pixels
    /// \param height   Back buffer height, in pixels
    ///
    ////////////////////////////////////////////////////////////
    EglContext(EglContext* shared, const ContextSettings& settings, unsigned int width, unsigned int height);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~EglContext();

    ////////////////////////////////////////////////////////////
    /// \brief Activate the context as the current target
    ///        for rendering
    ///
    /// \return True on success, false if any error happened
    ///
    ////////////////////////////////////////////////////////////
    virtual bool makeCurrent();

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

    ////////////////////////////////////////////////////////////
    /// \brief Create the context
    ///
    /// \param shared       Context to share the new one with (can be NULL)
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    /// \param settings     Creation parameters
    ///
    ////////////////////////////////////////////////////////////
    void createContext(EglContext* shared);

    ////////////////////////////////////////////////////////////
    /// \brief Create the EGL surface
    ///
    /// This function must be called when the activity (re)start, or
    /// when the orientation change.
    ///
    /// \param window: The native window type
    ///
    ////////////////////////////////////////////////////////////
    void createSurface(EGLNativeWindowType window);

    ////////////////////////////////////////////////////////////
    /// \brief Destroy the EGL surface
    ///
    /// This function must be called when the activity is stopped, or
    /// when the orientation change.
    ///
    ////////////////////////////////////////////////////////////
    void destroySurface();

    ////////////////////////////////////////////////////////////
    /// \brief Get the best EGL visual for a given set of video settings
    ///
    /// \param display      EGL display
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    /// \param settings     Requested context settings
    ///
    /// \return The best EGL config
    ///
    ////////////////////////////////////////////////////////////
    static EGLConfig getBestConfig(EGLDisplay display, unsigned int bitsPerPixel, const ContextSettings& settings);

#ifdef SFML_SYSTEM_LINUX
    ////////////////////////////////////////////////////////////
    /// \brief Select the best EGL visual for a given set of settings
    ///
    /// \param display      X display
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    /// \param settings     Requested context settings
    ///
    /// \return The best visual
    ///
    ////////////////////////////////////////////////////////////
    static XVisualInfo selectBestVisual(::Display* display, unsigned int bitsPerPixel, const ContextSettings& settings);
#endif

private:

    ////////////////////////////////////////////////////////////
    /// \brief Helper to copy the picked EGL configuration 
    ////////////////////////////////////////////////////////////
    void updateSettings();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    EGLDisplay  m_display; ///< The internal EGL display
    EGLContext  m_context; ///< The internal EGL context
    EGLSurface  m_surface; ///< The internal EGL surface
    EGLConfig   m_config;  ///< The internal EGL config

};

} // namespace priv

} // namespace sf


#endif // SFML_EGLCONTEXT_HPP
