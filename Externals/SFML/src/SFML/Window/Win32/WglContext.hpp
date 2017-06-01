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

#ifndef SFML_WGLCONTEXT_HPP
#define SFML_WGLCONTEXT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/GlContext.hpp>
#include <SFML/Window/Win32/WglExtensions.hpp>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Windows (WGL) implementation of OpenGL contexts
///
////////////////////////////////////////////////////////////
class WglContext : public GlContext
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Create a new default context
    ///
    /// \param shared Context to share the new one with (can be NULL)
    ///
    ////////////////////////////////////////////////////////////
    WglContext(WglContext* shared);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context attached to a window
    ///
    /// \param shared       Context to share the new one with
    /// \param settings     Creation parameters
    /// \param owner        Pointer to the owner window
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    WglContext(WglContext* shared, const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context that embeds its own rendering target
    ///
    /// \param shared   Context to share the new one with
    /// \param settings Creation parameters
    /// \param width    Back buffer width, in pixels
    /// \param height   Back buffer height, in pixels
    ///
    ////////////////////////////////////////////////////////////
    WglContext(WglContext* shared, const ContextSettings& settings, unsigned int width, unsigned int height);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~WglContext();

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
    /// \brief Activate the context as the current target for rendering
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
    /// \brief Select the best pixel format for a given set of settings
    ///
    /// \param deviceContext Device context
    /// \param bitsPerPixel  Pixel depth, in bits per pixel
    /// \param settings      Requested context settings
    /// \param pbuffer       Whether the pixel format should support pbuffers
    ///
    /// \return The best pixel format
    ///
    ////////////////////////////////////////////////////////////
    static int selectBestPixelFormat(HDC deviceContext, unsigned int bitsPerPixel, const ContextSettings& settings, bool pbuffer = false);

private:

    ////////////////////////////////////////////////////////////
    /// \brief Set the pixel format of the device context
    ///
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    void setDevicePixelFormat(unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Update the context settings from the selected pixel format
    ///
    ////////////////////////////////////////////////////////////
    void updateSettingsFromPixelFormat();

    ////////////////////////////////////////////////////////////
    /// \brief Create the context's drawing surface
    ///
    /// \param shared       Shared context (can be NULL)
    /// \param width        Back buffer width, in pixels
    /// \param height       Back buffer height, in pixels
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    void createSurface(WglContext* shared, unsigned int width, unsigned int height, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create the context's drawing surface from an existing window
    ///
    /// \param window       Window handle of the owning window
    /// \param bitsPerPixel Pixel depth, in bits per pixel
    ///
    ////////////////////////////////////////////////////////////
    void createSurface(HWND window, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create the context
    ///
    /// \param shared Context to share the new one with (can be NULL)
    ///
    ////////////////////////////////////////////////////////////
    void createContext(WglContext* shared);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    HWND        m_window;        ///< Window to which the context is attached
    HPBUFFERARB m_pbuffer;       ///< Handle to a pbuffer if one was created
    HDC         m_deviceContext; ///< Device context associated to the context
    HGLRC       m_context;       ///< OpenGL context
    bool        m_ownsWindow;    ///< Do we own the target window?
};

} // namespace priv

} // namespace sf

#endif // SFML_WGLCONTEXT_HPP
