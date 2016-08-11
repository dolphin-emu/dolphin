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

#ifndef SFML_GLCONTEXT_HPP
#define SFML_GLCONTEXT_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Config.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/System/NonCopyable.hpp>


namespace sf
{
namespace priv
{
class WindowImpl;

////////////////////////////////////////////////////////////
/// \brief Abstract class representing an OpenGL context
///
////////////////////////////////////////////////////////////
class GlContext : NonCopyable
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Perform the global initialization
    ///
    /// This function is called once, before the very first OpenGL
    /// resource is created. It makes sure that everything is ready
    /// for contexts to work properly.
    /// Note: this function doesn't need to be thread-safe, as it
    /// can be called only once.
    ///
    ////////////////////////////////////////////////////////////
    static void globalInit();

    ////////////////////////////////////////////////////////////
    /// \brief Perform the global cleanup
    ///
    /// This function is called after the very last OpenGL resource
    /// is destroyed. It makes sure that everything that was
    /// created by initialize() is properly released.
    /// Note: this function doesn't need to be thread-safe, as it
    /// can be called only once.
    ///
    ////////////////////////////////////////////////////////////
    static void globalCleanup();

    ////////////////////////////////////////////////////////////
    /// \brief Ensures that an OpenGL context is active in the current thread
    ///
    ////////////////////////////////////////////////////////////
    static void ensureContext();

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context, not associated to a window
    ///
    /// This function automatically chooses the specialized class
    /// to use according to the OS.
    ///
    /// \return Pointer to the created context (don't forget to delete it)
    ///
    ////////////////////////////////////////////////////////////
    static GlContext* create();

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context attached to a window
    ///
    /// This function automatically chooses the specialized class
    /// to use according to the OS.
    ///
    /// \param settings     Creation parameters
    /// \param owner        Pointer to the owner window
    /// \param bitsPerPixel Pixel depth (in bits per pixel)
    ///
    /// \return Pointer to the created context
    ///
    ////////////////////////////////////////////////////////////
    static GlContext* create(const ContextSettings& settings, const WindowImpl* owner, unsigned int bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Create a new context that embeds its own rendering target
    ///
    /// This function automatically chooses the specialized class
    /// to use according to the OS.
    ///
    /// \param settings Creation parameters
    /// \param width    Back buffer width
    /// \param height   Back buffer height
    ///
    /// \return Pointer to the created context
    ///
    ////////////////////////////////////////////////////////////
    static GlContext* create(const ContextSettings& settings, unsigned int width, unsigned int height);

public:
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
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~GlContext();

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
    /// \brief Activate or deactivate the context as the current target for rendering
    ///
    /// A context is active only on the current thread, if you want to
    /// make it active on another thread you have to deactivate it
    /// on the previous thread first if it was active.
    /// Only one context can be active on a thread at a time, thus
    /// the context previously active (if any) automatically gets deactivated.
    ///
    /// \param active True to activate, false to deactivate
    ///
    /// \return True if operation was successful, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    bool setActive(bool active);

    ////////////////////////////////////////////////////////////
    /// \brief Display what has been rendered to the context so far
    ///
    ////////////////////////////////////////////////////////////
    virtual void display() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable vertical synchronization
    ///
    /// Activating vertical synchronization will limit the number
    /// of frames displayed to the refresh rate of the monitor.
    /// This can avoid some visual artifacts, and limit the framerate
    /// to a good value (but not constant across different computers).
    ///
    /// \param enabled True to enable v-sync, false to deactivate
    ///
    ////////////////////////////////////////////////////////////
    virtual void setVerticalSyncEnabled(bool enabled) = 0;

protected:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// This constructor is meant for derived classes only.
    ///
    ////////////////////////////////////////////////////////////
    GlContext();

    ////////////////////////////////////////////////////////////
    /// \brief Activate the context as the current target
    ///        for rendering
    ///
    /// \return True on success, false if any error happened
    ///
    ////////////////////////////////////////////////////////////
    virtual bool makeCurrent() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Evaluate a pixel format configuration
    ///
    /// This functions can be used by implementations that have
    /// several valid formats and want to get the best one.
    /// A score is returned for the given configuration: the
    /// lower the score is, the better the configuration is.
    ///
    /// \param bitsPerPixel Requested pixel depth (bits per pixel)
    /// \param settings     Requested additional settings
    /// \param colorBits    Color bits of the configuration to evaluate
    /// \param depthBits    Depth bits of the configuration to evaluate
    /// \param stencilBits  Stencil bits of the configuration to evaluate
    /// \param antialiasing Antialiasing level of the configuration to evaluate
    /// \param accelerated  Whether the pixel format is hardware accelerated
    /// \param sRgb         Whether the pixel format is sRGB capable
    ///
    /// \return Score of the configuration
    ///
    ////////////////////////////////////////////////////////////
    static int evaluateFormat(unsigned int bitsPerPixel, const ContextSettings& settings, int colorBits, int depthBits, int stencilBits, int antialiasing, bool accelerated, bool sRgb);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    ContextSettings m_settings; ///< Creation settings of the context

private:

    ////////////////////////////////////////////////////////////
    /// \brief Perform various initializations after the context construction
    /// \param requestedSettings Requested settings during context creation
    ///
    ////////////////////////////////////////////////////////////
    void initialize(const ContextSettings& requestedSettings);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether the context is compatible with the requested settings
    /// \param requestedSettings Requested settings during context creation
    ///
    ////////////////////////////////////////////////////////////
    void checkSettings(const ContextSettings& requestedSettings);
};

} // namespace priv

} // namespace sf


#endif // SFML_GLCONTEXT_HPP
