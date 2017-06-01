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

#ifndef SFML_WINDOWIMPLANDROID_HPP
#define SFML_WINDOWIMPLANDROID_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Event.hpp>
#include <SFML/Window/WindowImpl.hpp>
#include <SFML/Window/EglContext.hpp>
#include <SFML/System/Android/Activity.hpp>
#include <android/input.h>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Android implementation of WindowImpl
///
////////////////////////////////////////////////////////////
class WindowImplAndroid : public WindowImpl
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Construct the window implementation from an existing control
    ///
    /// \param handle Platform-specific handle of the control
    ///
    ////////////////////////////////////////////////////////////
    WindowImplAndroid(WindowHandle handle);

    ////////////////////////////////////////////////////////////
    /// \brief Create the window implementation
    ///
    /// \param mode     Video mode to use
    /// \param title    Title of the window
    /// \param style    Window style (resizable, fixed, or fullscren)
    /// \param settings Additional settings for the underlying OpenGL context
    ///
    ////////////////////////////////////////////////////////////
    WindowImplAndroid(VideoMode mode, const String& title, unsigned long style, const ContextSettings& settings);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~WindowImplAndroid();

    ////////////////////////////////////////////////////////////
    /// \brief Get the OS-specific handle of the window
    ///
    /// \return Handle of the window
    ///
    ////////////////////////////////////////////////////////////
    virtual WindowHandle getSystemHandle() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the position of the window
    ///
    /// \return Position of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual Vector2i getPosition() const;

    ////////////////////////////////////////////////////////////
    /// \brief Change the position of the window on screen
    ///
    /// \param position New position of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual void setPosition(const Vector2i& position);

    ////////////////////////////////////////////////////////////
    /// \brief Get the client size of the window
    ///
    /// \return Size of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual Vector2u getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Change the size of the rendering region of the window
    ///
    /// \param size New size, in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual void setSize(const Vector2u& size);

    ////////////////////////////////////////////////////////////
    /// \brief Change the title of the window
    ///
    /// \param title New title
    ///
    ////////////////////////////////////////////////////////////
    virtual void setTitle(const String& title);

    ////////////////////////////////////////////////////////////
    /// \brief Change the window's icon
    ///
    /// \param width  Icon's width, in pixels
    /// \param height Icon's height, in pixels
    /// \param pixels Pointer to the pixels in memory, format must be RGBA 32 bits
    ///
    ////////////////////////////////////////////////////////////
    virtual void setIcon(unsigned int width, unsigned int height, const Uint8* pixels);

    ////////////////////////////////////////////////////////////
    /// \brief Show or hide the window
    ///
    /// \param visible True to show, false to hide
    ///
    ////////////////////////////////////////////////////////////
    virtual void setVisible(bool visible);

    ////////////////////////////////////////////////////////////
    /// \brief Show or hide the mouse cursor
    ///
    /// \param visible True to show, false to hide
    ///
    ////////////////////////////////////////////////////////////
    virtual void setMouseCursorVisible(bool visible);

    ////////////////////////////////////////////////////////////
    /// \brief Clips or releases the mouse cursor
    ///
    /// \param grabbed True to enable, false to disable
    ///
    ////////////////////////////////////////////////////////////
    virtual void setMouseCursorGrabbed(bool grabbed);

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable automatic key-repeat
    ///
    /// \param enabled True to enable, false to disable
    ///
    ////////////////////////////////////////////////////////////
    virtual void setKeyRepeatEnabled(bool enabled);

    ////////////////////////////////////////////////////////////
    /// \brief Request the current window to be made the active
    ///        foreground window
    ///
    ////////////////////////////////////////////////////////////
    virtual void requestFocus();

    ////////////////////////////////////////////////////////////
    /// \brief Check whether the window has the input focus
    ///
    /// \return True if window has focus, false otherwise
    ///
    ////////////////////////////////////////////////////////////
    virtual bool hasFocus() const;

    static void forwardEvent(const Event& event);
    static WindowImplAndroid* singleInstance;

protected:

    ////////////////////////////////////////////////////////////
    /// \brief Process incoming events from the operating system
    ///
    ////////////////////////////////////////////////////////////
    virtual void processEvents();

private:

    ////////////////////////////////////////////////////////////
    /// \brief Process messages from the looper associated with the main thread
    ///
    /// \param fd     File descriptor
    /// \param events Bitmask of the poll events that were triggered
    /// \param data   Data pointer supplied
    ///
    /// \return Whether it should continue (1) or unregister the callback (0)
    ///
    ////////////////////////////////////////////////////////////
    static int processEvent(int fd, int events, void* data);

    static int processScrollEvent(AInputEvent* _event, ActivityStates* states);
    static int processKeyEvent(AInputEvent* _event, ActivityStates* states);
    static int processMotionEvent(AInputEvent* _event, ActivityStates* states);
    static int processPointerEvent(bool isDown, AInputEvent* event, ActivityStates* states);

    ////////////////////////////////////////////////////////////
    /// \brief Convert a Android key to SFML key code
    ///
    /// \param symbol Android key to convert
    ///
    /// \return Corresponding SFML key code
    ///
    ////////////////////////////////////////////////////////////
    static Keyboard::Key androidKeyToSF(int32_t key);

    ////////////////////////////////////////////////////////////
    /// \brief Get Unicode decoded from the input event
    ///
    /// \param Event Input event
    ///
    /// \return The Unicode value
    ///
    ////////////////////////////////////////////////////////////
    static int getUnicode(AInputEvent* event);

    Vector2u m_size;
    bool m_windowBeingCreated;
    bool m_windowBeingDestroyed;
    bool m_hasFocus;
};

} // namespace priv

} // namespace sf


#endif // SFML_WINDOWIMPLANDROID_HPP
