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
#include <SFML/Window/WindowStyle.hpp> // important to be included first (conflict with None)
#include <SFML/Window/Unix/WindowImplX11.hpp>
#include <SFML/Window/Unix/Display.hpp>
#include <SFML/Window/Unix/InputImpl.hpp>
#include <SFML/Window/Unix/ScopedXcbPtr.hpp>
#include <SFML/System/Utf.hpp>
#include <SFML/System/Err.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Sleep.hpp>
#include <xcb/xcb_image.h>
#include <xcb/randr.h>
#include <X11/Xlibint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

#ifdef SFML_OPENGL_ES
    #include <SFML/Window/EglContext.hpp>
    typedef sf::priv::EglContext ContextType;
#else
    #include <SFML/Window/Unix/GlxContext.hpp>
    typedef sf::priv::GlxContext ContextType;
#endif

////////////////////////////////////////////////////////////
// Private data
////////////////////////////////////////////////////////////
namespace
{
    sf::priv::WindowImplX11*              fullscreenWindow = NULL;
    std::vector<sf::priv::WindowImplX11*> allWindows;
    sf::Mutex                             allWindowsMutex;
    sf::String                            windowManagerName;

    static const unsigned long            eventMask = XCB_EVENT_MASK_FOCUS_CHANGE   | XCB_EVENT_MASK_BUTTON_PRESS     |
                                                      XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION    |
                                                      XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_KEY_PRESS        |
                                                      XCB_EVENT_MASK_KEY_RELEASE    | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                                                      XCB_EVENT_MASK_ENTER_WINDOW   | XCB_EVENT_MASK_LEAVE_WINDOW     |
                                                      XCB_EVENT_MASK_VISIBILITY_CHANGE;

    static const unsigned int             maxTrialsCount = 5;

    // Filter the events received by windows (only allow those matching a specific window)
    Bool checkEvent(::Display*, XEvent* event, XPointer userData)
    {
        // Just check if the event matches the window
        return event->xany.window == reinterpret_cast< ::Window >(userData);
    }

    // Find the name of the current executable
    std::string findExecutableName()
    {
        // We use /proc/self/cmdline to get the command line
        // the user used to invoke this instance of the application
        int file = ::open("/proc/self/cmdline", O_RDONLY | O_NONBLOCK);

        if (file < 0)
            return "sfml";

        std::vector<char> buffer(256, 0);
        std::size_t offset = 0;
        ssize_t result = 0;

        while ((result = read(file, &buffer[offset], 256)) > 0)
        {
            buffer.resize(buffer.size() + result, 0);
            offset += result;
        }

        ::close(file);

        if (offset)
        {
            buffer[offset] = 0;

            // Remove the path to keep the executable name only
            return basename(&buffer[0]);
        }

        // Default fallback name
        return "sfml";
    }

    // Check if Extended Window Manager Hints are supported
    bool ewmhSupported()
    {
        static bool checked = false;
        static bool ewmhSupported = false;

        if (checked)
            return ewmhSupported;

        checked = true;

        xcb_connection_t* connection = sf::priv::OpenConnection();

        xcb_atom_t netSupportingWmCheck = sf::priv::getAtom("_NET_SUPPORTING_WM_CHECK", true);
        xcb_atom_t netSupported = sf::priv::getAtom("_NET_SUPPORTED", true);

        if (!netSupportingWmCheck || !netSupported)
            return false;

        sf::priv::ScopedXcbPtr<xcb_generic_error_t> error(NULL);

        sf::priv::ScopedXcbPtr<xcb_get_property_reply_t> rootSupportingWindow(xcb_get_property_reply(
            connection,
            xcb_get_property(
                connection,
                0,
                sf::priv::XCBDefaultRootWindow(connection),
                netSupportingWmCheck,
                XCB_ATOM_WINDOW,
                0,
                1
            ),
            &error
        ));

        if (!rootSupportingWindow || rootSupportingWindow->length != 1)
        {
            sf::priv::CloseConnection(connection);
            return false;
        }

        xcb_window_t* rootWindow = reinterpret_cast<xcb_window_t*>(xcb_get_property_value(rootSupportingWindow.get()));

        if (!rootWindow)
        {
            sf::priv::CloseConnection(connection);
            return false;
        }

        sf::priv::ScopedXcbPtr<xcb_get_property_reply_t> childSupportingWindow(xcb_get_property_reply(
            connection,
            xcb_get_property(
                connection,
                0,
                *rootWindow,
                netSupportingWmCheck,
                XCB_ATOM_WINDOW,
                0,
                1
            ),
            &error
        ));

        if (!childSupportingWindow || childSupportingWindow->length != 1)
        {
            sf::priv::CloseConnection(connection);
            return false;
        }

        xcb_window_t* childWindow = reinterpret_cast<xcb_window_t*>(xcb_get_property_value(childSupportingWindow.get()));

        if (!childWindow)
        {
            sf::priv::CloseConnection(connection);
            return false;
        }

        // Conforming window managers should return the same window for both queries
        if (*rootWindow != *childWindow)
        {
            sf::priv::CloseConnection(connection);
            return false;
        }

        ewmhSupported = true;

        // We try to get the name of the window manager
        // for window manager specific workarounds
        xcb_atom_t netWmName = sf::priv::getAtom("_NET_WM_NAME", true);
        xcb_atom_t utf8StringType = sf::priv::getAtom("UTF8_STRING");

        if (!utf8StringType)
            utf8StringType = XCB_ATOM_STRING;

        if (!netWmName)
        {
            sf::priv::CloseConnection(connection);
            return true;
        }

        sf::priv::ScopedXcbPtr<xcb_get_property_reply_t> wmName(xcb_get_property_reply(
            connection,
            xcb_get_property(
                connection,
                0,
                *childWindow,
                netWmName,
                utf8StringType,
                0,
                0x7fffffff
            ),
            &error
        ));

        sf::priv::CloseConnection(connection);

        // It seems the wm name string reply is not necessarily
        // null-terminated. The work around is to get its actual
        // length to build a proper string
        const char* begin = reinterpret_cast<const char*>(xcb_get_property_value(wmName.get()));
        const char* end = begin + xcb_get_property_value_length(wmName.get());
        windowManagerName = sf::String::fromUtf8(begin, end);

        return true;
    }

    sf::Keyboard::Key keysymToSF(xcb_keysym_t symbol)
    {
        switch (symbol)
        {
            case XK_Shift_L:      return sf::Keyboard::LShift;
            case XK_Shift_R:      return sf::Keyboard::RShift;
            case XK_Control_L:    return sf::Keyboard::LControl;
            case XK_Control_R:    return sf::Keyboard::RControl;
            case XK_Alt_L:        return sf::Keyboard::LAlt;
            case XK_Alt_R:        return sf::Keyboard::RAlt;
            case XK_Super_L:      return sf::Keyboard::LSystem;
            case XK_Super_R:      return sf::Keyboard::RSystem;
            case XK_Menu:         return sf::Keyboard::Menu;
            case XK_Escape:       return sf::Keyboard::Escape;
            case XK_semicolon:    return sf::Keyboard::SemiColon;
            case XK_slash:        return sf::Keyboard::Slash;
            case XK_equal:        return sf::Keyboard::Equal;
            case XK_minus:        return sf::Keyboard::Dash;
            case XK_bracketleft:  return sf::Keyboard::LBracket;
            case XK_bracketright: return sf::Keyboard::RBracket;
            case XK_comma:        return sf::Keyboard::Comma;
            case XK_period:       return sf::Keyboard::Period;
            case XK_apostrophe:   return sf::Keyboard::Quote;
            case XK_backslash:    return sf::Keyboard::BackSlash;
            case XK_grave:        return sf::Keyboard::Tilde;
            case XK_space:        return sf::Keyboard::Space;
            case XK_Return:       return sf::Keyboard::Return;
            case XK_KP_Enter:     return sf::Keyboard::Return;
            case XK_BackSpace:    return sf::Keyboard::BackSpace;
            case XK_Tab:          return sf::Keyboard::Tab;
            case XK_Prior:        return sf::Keyboard::PageUp;
            case XK_Next:         return sf::Keyboard::PageDown;
            case XK_End:          return sf::Keyboard::End;
            case XK_Home:         return sf::Keyboard::Home;
            case XK_Insert:       return sf::Keyboard::Insert;
            case XK_Delete:       return sf::Keyboard::Delete;
            case XK_KP_Add:       return sf::Keyboard::Add;
            case XK_KP_Subtract:  return sf::Keyboard::Subtract;
            case XK_KP_Multiply:  return sf::Keyboard::Multiply;
            case XK_KP_Divide:    return sf::Keyboard::Divide;
            case XK_Pause:        return sf::Keyboard::Pause;
            case XK_F1:           return sf::Keyboard::F1;
            case XK_F2:           return sf::Keyboard::F2;
            case XK_F3:           return sf::Keyboard::F3;
            case XK_F4:           return sf::Keyboard::F4;
            case XK_F5:           return sf::Keyboard::F5;
            case XK_F6:           return sf::Keyboard::F6;
            case XK_F7:           return sf::Keyboard::F7;
            case XK_F8:           return sf::Keyboard::F8;
            case XK_F9:           return sf::Keyboard::F9;
            case XK_F10:          return sf::Keyboard::F10;
            case XK_F11:          return sf::Keyboard::F11;
            case XK_F12:          return sf::Keyboard::F12;
            case XK_F13:          return sf::Keyboard::F13;
            case XK_F14:          return sf::Keyboard::F14;
            case XK_F15:          return sf::Keyboard::F15;
            case XK_Left:         return sf::Keyboard::Left;
            case XK_Right:        return sf::Keyboard::Right;
            case XK_Up:           return sf::Keyboard::Up;
            case XK_Down:         return sf::Keyboard::Down;
            case XK_KP_Insert:    return sf::Keyboard::Numpad0;
            case XK_KP_End:       return sf::Keyboard::Numpad1;
            case XK_KP_Down:      return sf::Keyboard::Numpad2;
            case XK_KP_Page_Down: return sf::Keyboard::Numpad3;
            case XK_KP_Left:      return sf::Keyboard::Numpad4;
            case XK_KP_Begin:     return sf::Keyboard::Numpad5;
            case XK_KP_Right:     return sf::Keyboard::Numpad6;
            case XK_KP_Home:      return sf::Keyboard::Numpad7;
            case XK_KP_Up:        return sf::Keyboard::Numpad8;
            case XK_KP_Page_Up:   return sf::Keyboard::Numpad9;
            case XK_a:            return sf::Keyboard::A;
            case XK_b:            return sf::Keyboard::B;
            case XK_c:            return sf::Keyboard::C;
            case XK_d:            return sf::Keyboard::D;
            case XK_e:            return sf::Keyboard::E;
            case XK_f:            return sf::Keyboard::F;
            case XK_g:            return sf::Keyboard::G;
            case XK_h:            return sf::Keyboard::H;
            case XK_i:            return sf::Keyboard::I;
            case XK_j:            return sf::Keyboard::J;
            case XK_k:            return sf::Keyboard::K;
            case XK_l:            return sf::Keyboard::L;
            case XK_m:            return sf::Keyboard::M;
            case XK_n:            return sf::Keyboard::N;
            case XK_o:            return sf::Keyboard::O;
            case XK_p:            return sf::Keyboard::P;
            case XK_q:            return sf::Keyboard::Q;
            case XK_r:            return sf::Keyboard::R;
            case XK_s:            return sf::Keyboard::S;
            case XK_t:            return sf::Keyboard::T;
            case XK_u:            return sf::Keyboard::U;
            case XK_v:            return sf::Keyboard::V;
            case XK_w:            return sf::Keyboard::W;
            case XK_x:            return sf::Keyboard::X;
            case XK_y:            return sf::Keyboard::Y;
            case XK_z:            return sf::Keyboard::Z;
            case XK_0:            return sf::Keyboard::Num0;
            case XK_1:            return sf::Keyboard::Num1;
            case XK_2:            return sf::Keyboard::Num2;
            case XK_3:            return sf::Keyboard::Num3;
            case XK_4:            return sf::Keyboard::Num4;
            case XK_5:            return sf::Keyboard::Num5;
            case XK_6:            return sf::Keyboard::Num6;
            case XK_7:            return sf::Keyboard::Num7;
            case XK_8:            return sf::Keyboard::Num8;
            case XK_9:            return sf::Keyboard::Num9;
        }

        return sf::Keyboard::Unknown;
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
WindowImplX11::WindowImplX11(WindowHandle handle) :
m_window         (0),
m_screen         (NULL),
m_inputMethod    (NULL),
m_inputContext   (NULL),
m_isExternal     (true),
m_hiddenCursor   (0),
m_keyRepeat      (true),
m_previousSize   (-1, -1),
m_useSizeHints   (false),
m_fullscreen     (false),
m_cursorGrabbed  (false),
m_windowMapped   (false)
{
    // Open a connection with the X server
    m_display = OpenDisplay();
    m_connection = XGetXCBConnection(m_display);

    std::memset(&m_oldVideoMode, 0, sizeof(m_oldVideoMode));

    if (!m_connection)
    {
        err() << "Failed cast Display object to an XCB connection object" << std::endl;
        return;
    }

    // Make sure to check for EWMH support before we do anything
    ewmhSupported();

    m_screen = XCBDefaultScreen(m_connection);

    // Save the window handle
    m_window = handle;

    if (m_window)
    {
        // Make sure the window is listening to all the required events
        const uint32_t value_list[] = {static_cast<uint32_t>(eventMask)};

        xcb_change_window_attributes(
            m_connection,
            m_window,
            XCB_CW_EVENT_MASK,
            value_list
        );

        // Set the WM protocols
        setProtocols();

        // Do some common initializations
        initialize();
    }
}


////////////////////////////////////////////////////////////
WindowImplX11::WindowImplX11(VideoMode mode, const String& title, unsigned long style, const ContextSettings& settings) :
m_window         (0),
m_screen         (NULL),
m_inputMethod    (NULL),
m_inputContext   (NULL),
m_isExternal     (false),
m_hiddenCursor   (0),
m_keyRepeat      (true),
m_previousSize   (-1, -1),
m_useSizeHints   (false),
m_fullscreen     ((style & Style::Fullscreen) != 0),
m_cursorGrabbed  (m_fullscreen),
m_windowMapped   (false)
{
    // Open a connection with the X server
    m_display = OpenDisplay();
    m_connection = XGetXCBConnection(m_display);

    std::memset(&m_oldVideoMode, 0, sizeof(m_oldVideoMode));

    if (!m_connection)
    {
        err() << "Failed cast Display object to an XCB connection object" << std::endl;
        return;
    }

    // Make sure to check for EWMH support before we do anything
    ewmhSupported();

    m_screen = XCBDefaultScreen(m_connection);

    // Compute position and size
    int left = m_fullscreen ? 0 : (m_screen->width_in_pixels  - mode.width) / 2;
    int top = m_fullscreen ? 0 : (m_screen->height_in_pixels - mode.height) / 2;
    int width  = mode.width;
    int height = mode.height;

    // Choose the visual according to the context settings
    XVisualInfo visualInfo = ContextType::selectBestVisual(m_display, mode.bitsPerPixel, settings);

    // Define the window attributes
    xcb_colormap_t colormap = xcb_generate_id(m_connection);
    xcb_create_colormap(m_connection, XCB_COLORMAP_ALLOC_NONE, colormap, m_screen->root, visualInfo.visualid);
    const uint32_t value_list[] = {m_fullscreen && !ewmhSupported(), static_cast<uint32_t>(eventMask), colormap};

    // Create the window
    m_window = xcb_generate_id(m_connection);

    ScopedXcbPtr<xcb_generic_error_t> errptr(xcb_request_check(
        m_connection,
        xcb_create_window_checked(
            m_connection,
            static_cast<uint8_t>(visualInfo.depth),
            m_window,
            m_screen->root,
            left, top,
            width, height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            visualInfo.visualid,
            XCB_CW_EVENT_MASK | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_COLORMAP,
            value_list
        )
    ));

    if (errptr)
    {
        err() << "Failed to create window" << std::endl;
        return;
    }

    // Set the WM protocols
    setProtocols();

    // Set the WM initial state to the normal state
    WMHints hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.initial_state = 1;
    hints.flags |= 1 << 1;
    setWMHints(hints);

    // If not in fullscreen, set the window's style (tell the window manager to
    // change our window's decorations and functions according to the requested style)
    if (!m_fullscreen)
        setMotifHints(style);

    WMSizeHints sizeHints;
    std::memset(&sizeHints, 0, sizeof(sizeHints));

    // This is a hack to force some windows managers to disable resizing
    // Fullscreen is bugged on Openbox. Unless size hints are set, there
    // will be a region of the window that is off-screen. We try to workaround
    // this by setting size hints even in fullscreen just for Openbox.
    if ((!m_fullscreen || (windowManagerName == "Openbox")) && !(style & Style::Resize))
    {
        m_useSizeHints = true;
        sizeHints.flags     |= ((1 << 4) | (1 << 5));
        sizeHints.min_width  = width;
        sizeHints.max_width  = width;
        sizeHints.min_height = height;
        sizeHints.max_height = height;
    }

    // Set the WM hints of the normal state
    setWMSizeHints(sizeHints);

    // Set the window's WM class (this can be used by window managers)
    // The WM_CLASS property actually consists of 2 parts,
    // the instance name and the class name both of which should be
    // null terminated strings.
    // The instance name should be something unique to this invokation
    // of the application but is rarely if ever used these days.
    // For simplicity, we retrieve it via the base executable name.
    // The class name identifies a class of windows that
    // "are of the same type". We simply use the initial window name as
    // the class name.
    std::string windowClass = findExecutableName();
    windowClass += '\0'; // Important to separate instance from class
    windowClass += title.toAnsiString();

    // We add 1 to the size of the string to include the null at the end
    if (!changeWindowProperty(XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, windowClass.size() + 1, windowClass.c_str()))
        sf::err() << "Failed to set WM_CLASS property" << std::endl;

    // Set the window's name
    setTitle(title);

    // Do some common initializations
    initialize();

    // Set fullscreen video mode and switch to fullscreen if necessary
    if (m_fullscreen)
    {
        setPosition(Vector2i(0, 0));
        setVideoMode(mode);
        switchToFullscreen();
    }
}


////////////////////////////////////////////////////////////
WindowImplX11::~WindowImplX11()
{
    // Cleanup graphical resources
    cleanup();

    // Destroy the cursor
    if (m_hiddenCursor)
        xcb_free_cursor(m_connection, m_hiddenCursor);

    // Destroy the input context
    if (m_inputContext)
        XDestroyIC(m_inputContext);
    // Destroy the window
    if (m_window && !m_isExternal)
    {
        xcb_destroy_window(m_connection, m_window);
        xcb_flush(m_connection);
    }

    // Close the input method
    if (m_inputMethod)
        XCloseIM(m_inputMethod);

    // Close the connection with the X server
    CloseDisplay(m_display);

    // Remove this window from the global list of windows (required for focus request)
    Lock lock(allWindowsMutex);
    allWindows.erase(std::find(allWindows.begin(), allWindows.end(), this));
}


////////////////////////////////////////////////////////////
WindowHandle WindowImplX11::getSystemHandle() const
{
    return m_window;
}


////////////////////////////////////////////////////////////
void WindowImplX11::processEvents()
{
    XEvent event;
    while (XCheckIfEvent(m_display, &event, &checkEvent, reinterpret_cast<XPointer>(m_window)))
    {
        processEvent(event);
    }
}


////////////////////////////////////////////////////////////
Vector2i WindowImplX11::getPosition() const
{
    ::Window topLevelWindow = m_window;
    ::Window nextWindow = topLevelWindow;

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    // Get "top level" window, i.e. the window with the root window as its parent.
    while (nextWindow != m_screen->root)
    {
        topLevelWindow = nextWindow;

        ScopedXcbPtr<xcb_query_tree_reply_t> treeReply(xcb_query_tree_reply(
            m_connection,
            xcb_query_tree(
                m_connection,
                topLevelWindow
            ),
            &error
        ));

        if (error)
        {
            err() << "Failed to get window position (query_tree)" << std::endl;
            return Vector2i(0, 0);
        }

        nextWindow = treeReply->parent;
    }

    ScopedXcbPtr<xcb_get_geometry_reply_t> geometryReply(xcb_get_geometry_reply(
        m_connection,
        xcb_get_geometry(
            m_connection,
            topLevelWindow
        ),
        &error
    ));

    if (error)
    {
        err() << "Failed to get window position (get_geometry)" << std::endl;
        return Vector2i(0, 0);
    }

    return Vector2i(geometryReply->x, geometryReply->y);
}


////////////////////////////////////////////////////////////
void WindowImplX11::setPosition(const Vector2i& position)
{
    uint32_t values[] = {static_cast<uint32_t>(position.x), static_cast<uint32_t>(position.y)};
    xcb_configure_window(m_connection, m_window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                         values);
    xcb_flush(m_connection);
}


////////////////////////////////////////////////////////////
Vector2u WindowImplX11::getSize() const
{
    ScopedXcbPtr<xcb_get_geometry_reply_t> reply(xcb_get_geometry_reply(m_connection, xcb_get_geometry(m_connection, m_window), NULL));

    return Vector2u(reply->width, reply->height);
}


////////////////////////////////////////////////////////////
void WindowImplX11::setSize(const Vector2u& size)
{
    // If resizing is disable for the window we have to update the size hints (required by some window managers).
    if( m_useSizeHints ) {
        WMSizeHints sizeHints;
        std::memset(&sizeHints, 0, sizeof(sizeHints));

        sizeHints.flags     |= (1 << 4 | 1 << 5);
        sizeHints.min_width  = size.x;
        sizeHints.max_width  = size.x;
        sizeHints.min_height = size.y;
        sizeHints.max_height = size.y;

        setWMSizeHints(sizeHints);
    }

    uint32_t values[] = {size.x, size.y};

    ScopedXcbPtr<xcb_generic_error_t> configureWindowError(xcb_request_check(
        m_connection,
        xcb_configure_window(
            m_connection,
            m_window,
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
            values
        )
    ));

    if (configureWindowError)
        err() << "Failed to set window size" << std::endl;

    xcb_flush(m_connection);
}


////////////////////////////////////////////////////////////
void WindowImplX11::setTitle(const String& title)
{
    // XCB takes UTF-8-encoded strings.
    xcb_atom_t utf8StringType = getAtom("UTF8_STRING");

    if (!utf8StringType)
        utf8StringType = XCB_ATOM_STRING;

    std::string utf8String;
    Utf<32>::toUtf8(title.begin(), title.end(), std::back_inserter(utf8String));

    if (!changeWindowProperty(XCB_ATOM_WM_NAME, utf8StringType, 8, utf8String.length(), utf8String.c_str()))
        err() << "Failed to set window title" << std::endl;

    if (!changeWindowProperty(XCB_ATOM_WM_ICON_NAME, utf8StringType, 8, utf8String.length(), utf8String.c_str()))
        err() << "Failed to set WM_ICON_NAME property" << std::endl;

    if (ewmhSupported())
    {
        xcb_atom_t netWmName = getAtom("_NET_WM_NAME", true);
        xcb_atom_t netWmIconName = getAtom("_NET_WM_ICON_NAME", true);

        if (utf8StringType && netWmName)
        {
            if (!changeWindowProperty(netWmName, utf8StringType, 8, utf8String.length(), utf8String.c_str()))
                err() << "Failed to set _NET_WM_NAME property" << std::endl;
        }

        if (utf8StringType && netWmIconName)
        {
            if (!changeWindowProperty(netWmIconName, utf8StringType, 8, utf8String.length(), utf8String.c_str()))
                err() << "Failed to set _NET_WM_ICON_NAME property" << std::endl;
        }
    }

    xcb_flush(m_connection);
}


////////////////////////////////////////////////////////////
void WindowImplX11::setIcon(unsigned int width, unsigned int height, const Uint8* pixels)
{
    // X11 wants BGRA pixels: swap red and blue channels
    Uint8 iconPixels[width * height * 4];
    for (std::size_t i = 0; i < width * height; ++i)
    {
        iconPixels[i * 4 + 0] = pixels[i * 4 + 2];
        iconPixels[i * 4 + 1] = pixels[i * 4 + 1];
        iconPixels[i * 4 + 2] = pixels[i * 4 + 0];
        iconPixels[i * 4 + 3] = pixels[i * 4 + 3];
    }

    // Create the icon pixmap
    xcb_pixmap_t iconPixmap = xcb_generate_id(m_connection);

    ScopedXcbPtr<xcb_generic_error_t> createPixmapError(xcb_request_check(
        m_connection,
        xcb_create_pixmap_checked(
            m_connection,
            m_screen->root_depth,
            iconPixmap,
            m_screen->root,
            width,
            height
        )
    ));

    if (createPixmapError)
    {
        err() << "Failed to set the window's icon (create_pixmap): ";
        err() << "Error code " << static_cast<int>(createPixmapError->error_code) << std::endl;
        return;
    }

    xcb_gcontext_t iconGC = xcb_generate_id(m_connection);

    ScopedXcbPtr<xcb_generic_error_t> createGcError(xcb_request_check(
        m_connection,
        xcb_create_gc(
            m_connection,
            iconGC,
            iconPixmap,
            0,
            NULL
        )
    ));

    if (createGcError)
    {
        err() << "Failed to set the window's icon (create_gc): ";
        err() << "Error code " << static_cast<int>(createGcError->error_code) << std::endl;
        return;
    }

    ScopedXcbPtr<xcb_generic_error_t> putImageError(xcb_request_check(
        m_connection,
        xcb_put_image_checked(
            m_connection,
            XCB_IMAGE_FORMAT_Z_PIXMAP,
            iconPixmap,
            iconGC,
            width,
            height,
            0,
            0,
            0,
            m_screen->root_depth,
            sizeof(iconPixels),
            iconPixels
        )
    ));

    ScopedXcbPtr<xcb_generic_error_t> freeGcError(xcb_request_check(
        m_connection,
        xcb_free_gc(
            m_connection,
            iconGC
        )
    ));

    if (freeGcError)
    {
        err() << "Failed to free icon GC: ";
        err() << "Error code " << static_cast<int>(freeGcError->error_code) << std::endl;
    }

    if (putImageError)
    {
        err() << "Failed to set the window's icon (put_image): ";
        err() << "Error code " << static_cast<int>(putImageError->error_code) << std::endl;
        return;
    }

    // Create the mask pixmap (must have 1 bit depth)
    std::size_t pitch = (width + 7) / 8;
    static std::vector<Uint8> maskPixels(pitch * height, 0);
    for (std::size_t j = 0; j < height; ++j)
    {
        for (std::size_t i = 0; i < pitch; ++i)
        {
            for (std::size_t k = 0; k < 8; ++k)
            {
                if (i * 8 + k < width)
                {
                    Uint8 opacity = (pixels[(i * 8 + k + j * width) * 4 + 3] > 0) ? 1 : 0;
                    maskPixels[i + j * pitch] |= (opacity << k);
                }
            }
        }
    }

    xcb_pixmap_t maskPixmap = xcb_create_pixmap_from_bitmap_data(
        m_connection,
        m_window,
        reinterpret_cast<uint8_t*>(&maskPixels[0]),
        width,
        height,
        1,
        0,
        1,
        NULL
    );

    // Send our new icon to the window through the WMHints
    WMHints hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.flags      |= ((1 << 2) | (1 << 5));
    hints.icon_pixmap = iconPixmap;
    hints.icon_mask   = maskPixmap;

    setWMHints(hints);

    xcb_flush(m_connection);

    ScopedXcbPtr<xcb_generic_error_t> freePixmapError(xcb_request_check(
        m_connection,
        xcb_free_pixmap_checked(
            m_connection,
            iconPixmap
        )
    ));

    if (freePixmapError)
    {
        err() << "Failed to free icon pixmap: ";
        err() << "Error code " << static_cast<int>(freePixmapError->error_code) << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setVisible(bool visible)
{
    if (visible)
    {
        ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
            m_connection,
            xcb_map_window(
                m_connection,
                m_window
            )
        ));

        if (error)
            err() << "Failed to change window visibility" << std::endl;

        xcb_flush(m_connection);

        // Before continuing, make sure the WM has
        // internally marked the window as viewable
        while (!m_windowMapped)
            processEvents();
    }
    else
    {
        ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
            m_connection,
            xcb_unmap_window(
                m_connection,
                m_window
            )
        ));

        if (error)
            err() << "Failed to change window visibility" << std::endl;

        xcb_flush(m_connection);

        // Before continuing, make sure the WM has
        // internally marked the window as unviewable
        while (m_windowMapped)
            processEvents();
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setMouseCursorVisible(bool visible)
{
    const uint32_t values = visible ? XCB_NONE : m_hiddenCursor;

    ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
        m_connection,
        xcb_change_window_attributes(
            m_connection,
            m_window,
            XCB_CW_CURSOR,
            &values
        )
    ));

    if (error)
        err() << "Failed to change mouse cursor visibility" << std::endl;

    xcb_flush(m_connection);
}


////////////////////////////////////////////////////////////
void WindowImplX11::setMouseCursorGrabbed(bool grabbed)
{
    // This has no effect in fullscreen mode
    if (m_fullscreen || (m_cursorGrabbed == grabbed))
        return;

    if (grabbed)
    {
        // Try multiple times to grab the cursor
        for (unsigned int trial = 0; trial < maxTrialsCount; ++trial)
        {
            sf::priv::ScopedXcbPtr<xcb_generic_error_t> error(NULL);

            sf::priv::ScopedXcbPtr<xcb_grab_pointer_reply_t> grabPointerReply(xcb_grab_pointer_reply(
                m_connection,
                xcb_grab_pointer(
                    m_connection,
                    true,
                    m_window,
                    XCB_NONE,
                    XCB_GRAB_MODE_ASYNC,
                    XCB_GRAB_MODE_ASYNC,
                    m_window,
                    XCB_NONE,
                    XCB_CURRENT_TIME
                ),
                &error
            ));

            if (!error && grabPointerReply && (grabPointerReply->status == XCB_GRAB_STATUS_SUCCESS))
            {
                m_cursorGrabbed = true;
                break;
            }

            // The cursor grab failed, trying again after a small sleep
            sf::sleep(sf::milliseconds(50));
        }

        if (!m_cursorGrabbed)
            err() << "Failed to grab mouse cursor" << std::endl;
    }
    else
    {
        ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
            m_connection,
            xcb_ungrab_pointer_checked(
                m_connection,
                XCB_CURRENT_TIME
            )
        ));

        if (!error)
        {
            m_cursorGrabbed = false;
        }
        else
        {
            err() << "Failed to ungrab mouse cursor" << std::endl;
        }
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setKeyRepeatEnabled(bool enabled)
{
    m_keyRepeat = enabled;
}


////////////////////////////////////////////////////////////
void WindowImplX11::requestFocus()
{
    // Focus is only stolen among SFML windows, not between applications
    // Check the global list of windows to find out whether an SFML window has the focus
    // Note: can't handle console and other non-SFML windows belonging to the application.
    bool sfmlWindowFocused = false;

    {
        Lock lock(allWindowsMutex);
        for (std::vector<WindowImplX11*>::iterator itr = allWindows.begin(); itr != allWindows.end(); ++itr)
        {
            if ((*itr)->hasFocus())
            {
                sfmlWindowFocused = true;
                break;
            }
        }
    }

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    // Check if window is viewable (not on other desktop, ...)
    // TODO: Check also if minimized
    ScopedXcbPtr<xcb_get_window_attributes_reply_t> attributes(xcb_get_window_attributes_reply(
        m_connection,
        xcb_get_window_attributes(
            m_connection,
            m_window
        ),
        &error
    ));

    if (error || !attributes)
    {
        err() << "Failed to check if window is viewable while requesting focus" << std::endl;
        return; // error getting attribute
    }

    bool windowViewable = (attributes->map_state == XCB_MAP_STATE_VIEWABLE);

    if (sfmlWindowFocused && windowViewable)
    {
        // Another SFML window of this application has the focus and the current window is viewable:
        // steal focus (i.e. bring window to the front and give it input focus)
        grabFocus();
    }
    else
    {
        // Get current WM hints.
        ScopedXcbPtr<xcb_get_property_reply_t> hintsReply(xcb_get_property_reply(
            m_connection,
            xcb_get_property(m_connection, 0, m_window, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 0, 9),
            &error
        ));

        if (error || !hintsReply)
        {
            err() << "Failed to get WM hints while requesting focus" << std::endl;
            return;
        }

        WMHints* hints = reinterpret_cast<WMHints*>(xcb_get_property_value(hintsReply.get()));

        if (!hints)
        {
            err() << "Failed to get WM hints while requesting focus" << std::endl;
            return;
        }

        // Even if no hints were returned, we can simply set the proper flags we need and go on. This is
        // different from Xlib where XAllocWMHints() has to be called.
        hints->flags |= (1 << 8);
        setWMHints(*hints);
    }
}


////////////////////////////////////////////////////////////
bool WindowImplX11::hasFocus() const
{
    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    ScopedXcbPtr<xcb_get_input_focus_reply_t> reply(xcb_get_input_focus_reply(
        m_connection,
        xcb_get_input_focus_unchecked(
            m_connection
        ),
        &error
    ));

    if (error)
        err() << "Failed to check if window has focus" << std::endl;

    return (reply->focus == m_window);
}


////////////////////////////////////////////////////////////
void WindowImplX11::grabFocus()
{
    xcb_atom_t netActiveWindow = XCB_ATOM_NONE;

    if (ewmhSupported())
        netActiveWindow = getAtom("_NET_ACTIVE_WINDOW");

    if (netActiveWindow)
    {
        xcb_client_message_event_t event;
        std::memset(&event, 0, sizeof(event));

        event.response_type = XCB_CLIENT_MESSAGE;
        event.window = m_window;
        event.format = 32;
        event.sequence = 0;
        event.type = netActiveWindow;
        event.data.data32[0] = 1; // Normal application
        event.data.data32[1] = XCB_CURRENT_TIME;
        event.data.data32[2] = 0; // We don't know the currently active window

        ScopedXcbPtr<xcb_generic_error_t> activeWindowError(xcb_request_check(
            m_connection,
            xcb_send_event_checked(
                m_connection,
                0,
                XCBDefaultRootWindow(m_connection),
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                reinterpret_cast<char*>(&event)
            )
        ));

        if (activeWindowError)
            err() << "Setting fullscreen failed, could not send \"_NET_ACTIVE_WINDOW\" event" << std::endl;
    }
    else
    {
        ScopedXcbPtr<xcb_generic_error_t> setInputFocusError(xcb_request_check(
            m_connection,
            xcb_set_input_focus(
                m_connection,
                XCB_INPUT_FOCUS_POINTER_ROOT,
                m_window,
                XCB_CURRENT_TIME
            )
        ));

        if (setInputFocusError)
        {
            err() << "Failed to change active window (set_input_focus)" << std::endl;
            return;
        }

        const uint32_t values[] = {XCB_STACK_MODE_ABOVE};

        ScopedXcbPtr<xcb_generic_error_t> configureWindowError(xcb_request_check(
            m_connection,
            xcb_configure_window(
                m_connection,
                m_window,
                XCB_CONFIG_WINDOW_STACK_MODE,
                values
            )
        ));

        if (configureWindowError)
            err() << "Failed to change active window (configure_window)" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setVideoMode(const VideoMode& mode)
{
    // Skip mode switching if the new mode is equal to the desktop mode
    if (mode == VideoMode::getDesktopMode())
        return;

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    // Check if the RandR extension is present
    const xcb_query_extension_reply_t* randrExt = xcb_get_extension_data(m_connection, &xcb_randr_id);

    if (!randrExt || !randrExt->present)
    {
        // RandR extension is not supported: we cannot use fullscreen mode
        err() << "Fullscreen is not supported, switching to window mode" << std::endl;
        return;
    }

    // Load RandR and check its version
    ScopedXcbPtr<xcb_randr_query_version_reply_t> randrVersion(xcb_randr_query_version_reply(
        m_connection,
        xcb_randr_query_version(
            m_connection,
            1,
            1
        ),
        &error
    ));

    if (error)
    {
        err() << "Failed to load RandR, switching to window mode" << std::endl;
        return;
    }

    // Get the current configuration
    ScopedXcbPtr<xcb_randr_get_screen_info_reply_t> config(xcb_randr_get_screen_info_reply(
        m_connection,
        xcb_randr_get_screen_info(
            m_connection,
            m_screen->root
        ),
        &error
    ));

    if (error || !config)
    {
        // Failed to get the screen configuration
        err() << "Failed to get the current screen configuration for fullscreen mode, switching to window mode" << std::endl;
        return;
    }

    // Save the current video mode before we switch to fullscreen
    m_oldVideoMode = *config.get();

    // Get the available screen sizes
    xcb_randr_screen_size_t* sizes = xcb_randr_get_screen_info_sizes(config.get());

    if (!sizes || !config->nSizes)
    {
        err() << "Failed to get the fullscreen sizes, switching to window mode" << std::endl;
        return;
    }

    // Search for a matching size
    for (int i = 0; i < config->nSizes; ++i)
    {
        if (config->rotation == XCB_RANDR_ROTATION_ROTATE_90 ||
            config->rotation == XCB_RANDR_ROTATION_ROTATE_270)
            std::swap(sizes[i].height, sizes[i].width);

        if ((sizes[i].width  == static_cast<int>(mode.width)) &&
            (sizes[i].height == static_cast<int>(mode.height)))
        {
            // Switch to fullscreen mode
            ScopedXcbPtr<xcb_randr_set_screen_config_reply_t> setScreenConfig(xcb_randr_set_screen_config_reply(
                m_connection,
                xcb_randr_set_screen_config(
                    m_connection,
                    config->root,
                    XCB_CURRENT_TIME,
                    config->config_timestamp,
                    i,
                    config->rotation,
                    config->rate
                ),
                &error
            ));

            if (error)
                err() << "Failed to set new screen configuration" << std::endl;

            // Set "this" as the current fullscreen window
            fullscreenWindow = this;
            return;
        }
    }

    err() << "Failed to find matching fullscreen size, switching to window mode" << std::endl;
}


////////////////////////////////////////////////////////////
void WindowImplX11::resetVideoMode()
{
    if (fullscreenWindow == this)
    {
        // Get current screen info
        ScopedXcbPtr<xcb_generic_error_t> error(NULL);

        // Reset the video mode
        ScopedXcbPtr<xcb_randr_set_screen_config_reply_t> setScreenConfig(xcb_randr_set_screen_config_reply(
            m_connection,
            xcb_randr_set_screen_config(
                m_connection,
                m_oldVideoMode.root,
                XCB_CURRENT_TIME,
                m_oldVideoMode.config_timestamp,
                m_oldVideoMode.sizeID,
                m_oldVideoMode.rotation,
                m_oldVideoMode.rate
            ),
            &error
        ));

        if (error)
            err() << "Failed to reset old screen configuration" << std::endl;

        // Reset the fullscreen window
        fullscreenWindow = NULL;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::switchToFullscreen()
{
    grabFocus();

    if (ewmhSupported())
    {
        xcb_atom_t netWmBypassCompositor = getAtom("_NET_WM_BYPASS_COMPOSITOR");

        if (netWmBypassCompositor)
        {
            static const Uint32 bypassCompositor = 1;

            // Not being able to bypass the compositor is not a fatal error
            if (!changeWindowProperty(netWmBypassCompositor, XCB_ATOM_CARDINAL, 32, 1, &bypassCompositor))
                err() << "xcb_change_property failed, unable to set _NET_WM_BYPASS_COMPOSITOR" << std::endl;
        }

        xcb_atom_t netWmState = getAtom("_NET_WM_STATE", true);
        xcb_atom_t netWmStateFullscreen = getAtom("_NET_WM_STATE_FULLSCREEN", true);

        if (!netWmState || !netWmStateFullscreen)
        {
            err() << "Setting fullscreen failed. Could not get required atoms" << std::endl;
            return;
        }

        xcb_client_message_event_t event;
        std::memset(&event, 0, sizeof(event));

        event.response_type = XCB_CLIENT_MESSAGE;
        event.window = m_window;
        event.format = 32;
        event.sequence = 0;
        event.type = netWmState;
        event.data.data32[0] = 1; // _NET_WM_STATE_ADD
        event.data.data32[1] = netWmStateFullscreen;
        event.data.data32[2] = 0; // No second property
        event.data.data32[3] = 1; // Normal window

        ScopedXcbPtr<xcb_generic_error_t> wmStateError(xcb_request_check(
            m_connection,
            xcb_send_event_checked(
                m_connection,
                0,
                XCBDefaultRootWindow(m_connection),
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                reinterpret_cast<char*>(&event)
            )
        ));

        if (wmStateError)
            err() << "Setting fullscreen failed. Could not send \"_NET_WM_STATE\" event" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setProtocols()
{
    xcb_atom_t wmProtocols = getAtom("WM_PROTOCOLS");
    xcb_atom_t wmDeleteWindow = getAtom("WM_DELETE_WINDOW");

    if (!wmProtocols)
    {
        err() << "Failed to request WM_PROTOCOLS atom." << std::endl;
        return;
    }

    std::vector<xcb_atom_t> atoms;

    if (wmDeleteWindow)
    {
        atoms.push_back(wmDeleteWindow);
    }
    else
    {
        err() << "Failed to request WM_DELETE_WINDOW atom." << std::endl;
    }

    xcb_atom_t netWmPing = XCB_ATOM_NONE;
    xcb_atom_t netWmPid = XCB_ATOM_NONE;

    if (ewmhSupported())
    {
        netWmPing = getAtom("_NET_WM_PING", true);
        netWmPid = getAtom("_NET_WM_PID", true);
    }

    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    if (netWmPing && netWmPid)
    {
        uint32_t pid = getpid();

        if (changeWindowProperty(netWmPid, XCB_ATOM_CARDINAL, 32, 1, &pid))
            atoms.push_back(netWmPing);
    }

    if (!atoms.empty())
    {
        if (!changeWindowProperty(wmProtocols, XCB_ATOM_ATOM, 32, atoms.size(), &atoms[0]))
            err() << "Failed to set window protocols" << std::endl;
    }
    else
    {
        err() << "Didn't set any window protocols" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setMotifHints(unsigned long style)
{
    ScopedXcbPtr<xcb_generic_error_t> error(NULL);

    static const std::string MOTIF_WM_HINTS = "_MOTIF_WM_HINTS";
    ScopedXcbPtr<xcb_intern_atom_reply_t> hintsAtomReply(xcb_intern_atom_reply(
        m_connection,
        xcb_intern_atom(
            m_connection,
            0,
            MOTIF_WM_HINTS.size(),
            MOTIF_WM_HINTS.c_str()
        ),
        &error
    ));

    if (!error && hintsAtomReply)
    {
        static const unsigned long MWM_HINTS_FUNCTIONS   = 1 << 0;
        static const unsigned long MWM_HINTS_DECORATIONS = 1 << 1;

        //static const unsigned long MWM_DECOR_ALL         = 1 << 0;
        static const unsigned long MWM_DECOR_BORDER      = 1 << 1;
        static const unsigned long MWM_DECOR_RESIZEH     = 1 << 2;
        static const unsigned long MWM_DECOR_TITLE       = 1 << 3;
        static const unsigned long MWM_DECOR_MENU        = 1 << 4;
        static const unsigned long MWM_DECOR_MINIMIZE    = 1 << 5;
        static const unsigned long MWM_DECOR_MAXIMIZE    = 1 << 6;

        //static const unsigned long MWM_FUNC_ALL          = 1 << 0;
        static const unsigned long MWM_FUNC_RESIZE       = 1 << 1;
        static const unsigned long MWM_FUNC_MOVE         = 1 << 2;
        static const unsigned long MWM_FUNC_MINIMIZE     = 1 << 3;
        static const unsigned long MWM_FUNC_MAXIMIZE     = 1 << 4;
        static const unsigned long MWM_FUNC_CLOSE        = 1 << 5;

        struct MotifWMHints
        {
            uint32_t flags;
            uint32_t functions;
            uint32_t decorations;
            int32_t  inputMode;
            uint32_t state;
        };

        MotifWMHints hints;
        hints.flags       = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
        hints.decorations = 0;
        hints.functions   = 0;
        hints.inputMode   = 0;
        hints.state       = 0;

        if (style & Style::Titlebar)
        {
            hints.decorations |= MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MINIMIZE | MWM_DECOR_MENU;
            hints.functions   |= MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE;
        }
        if (style & Style::Resize)
        {
            hints.decorations |= MWM_DECOR_MAXIMIZE | MWM_DECOR_RESIZEH;
            hints.functions   |= MWM_FUNC_MAXIMIZE | MWM_FUNC_RESIZE;
        }
        if (style & Style::Close)
        {
            hints.decorations |= 0;
            hints.functions   |= MWM_FUNC_CLOSE;
        }

        if (!changeWindowProperty(hintsAtomReply->atom, hintsAtomReply->atom, 32, 5, &hints))
            err() << "xcb_change_property failed, could not set window hints" << std::endl;
    }
    else
    {
        err() << "Failed to request _MOTIF_WM_HINTS atom." << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplX11::setWMHints(const WMHints& hints)
{
    if (!changeWindowProperty(XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32, sizeof(hints) / 4, &hints))
        sf::err() << "Failed to set WM_HINTS property" << std::endl;
}


////////////////////////////////////////////////////////////
void WindowImplX11::setWMSizeHints(const WMSizeHints& hints)
{
    if (!changeWindowProperty(XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(hints) / 4, &hints))
        sf::err() << "Failed to set XCB_ATOM_WM_NORMAL_HINTS property" << std::endl;
}


////////////////////////////////////////////////////////////
bool WindowImplX11::changeWindowProperty(xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t length, const void* data)
{
    ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
        m_connection,
        xcb_change_property_checked(
            m_connection,
            XCB_PROP_MODE_REPLACE,
            m_window,
            property,
            type,
            format,
            length,
            data
        )
    ));

    return !error;
}


////////////////////////////////////////////////////////////
void WindowImplX11::initialize()
{
    // Create the input context
    m_inputMethod = XOpenIM(m_display, NULL, NULL, NULL);

    if (m_inputMethod)
    {
        m_inputContext = XCreateIC(
            m_inputMethod,
            XNClientWindow,
            m_window,
            XNFocusWindow,
            m_window,
            XNInputStyle,
            XIMPreeditNothing | XIMStatusNothing,
            reinterpret_cast<void*>(NULL)
        );
    }
    else
    {
        m_inputContext = NULL;
    }

    if (!m_inputContext)
        err() << "Failed to create input context for window -- TextEntered event won't be able to return unicode" << std::endl;

    // Show the window
    setVisible(true);

    // Raise the window and grab input focus
    grabFocus();

    // Create the hidden cursor
    createHiddenCursor();

    // Flush the commands queue
    xcb_flush(m_connection);

    // Add this window to the global list of windows (required for focus request)
    Lock lock(allWindowsMutex);
    allWindows.push_back(this);
}


////////////////////////////////////////////////////////////
void WindowImplX11::createHiddenCursor()
{
    xcb_pixmap_t cursorPixmap = xcb_generate_id(m_connection);

    // Create the cursor's pixmap (1x1 pixels)
    ScopedXcbPtr<xcb_generic_error_t> createPixmapError(xcb_request_check(
        m_connection,
        xcb_create_pixmap(
            m_connection,
            1,
            cursorPixmap,
            m_window,
            1,
            1
        )
    ));

    if (createPixmapError)
    {
        err() << "Failed to create pixmap for hidden cursor" << std::endl;
        return;
    }

    m_hiddenCursor = xcb_generate_id(m_connection);

    // Create the cursor, using the pixmap as both the shape and the mask of the cursor
    ScopedXcbPtr<xcb_generic_error_t> createCursorError(xcb_request_check(
        m_connection,
        xcb_create_cursor(
            m_connection,
            m_hiddenCursor,
            cursorPixmap,
            cursorPixmap,
            0, 0, 0, // Foreground RGB color
            0, 0, 0, // Background RGB color
            0,       // X
            0        // Y
        )
    ));

    if (createCursorError)
        err() << "Failed to create hidden cursor" << std::endl;

    // We don't need the pixmap any longer, free it
    ScopedXcbPtr<xcb_generic_error_t> freePixmapError(xcb_request_check(
        m_connection,
        xcb_free_pixmap(
            m_connection,
            cursorPixmap
        )
    ));

    if (freePixmapError)
        err() << "Failed to free pixmap for hidden cursor" << std::endl;
}


////////////////////////////////////////////////////////////
void WindowImplX11::cleanup()
{
    // Restore the previous video mode (in case we were running in fullscreen)
    resetVideoMode();

    // Unhide the mouse cursor (in case it was hidden)
    setMouseCursorVisible(true);
}


////////////////////////////////////////////////////////////
bool WindowImplX11::processEvent(XEvent& windowEvent)
{
    // This function implements a workaround to properly discard
    // repeated key events when necessary. The problem is that the
    // system's key events policy doesn't match SFML's one: X server will generate
    // both repeated KeyPress and KeyRelease events when maintaining a key down, while
    // SFML only wants repeated KeyPress events. Thus, we have to:
    // - Discard duplicated KeyRelease events when KeyRepeatEnabled is true
    // - Discard both duplicated KeyPress and KeyRelease events when KeyRepeatEnabled is false

    // Detect repeated key events
    // (code shamelessly taken from SDL)
    if (windowEvent.type == KeyRelease)
    {
        // Check if there's a matching KeyPress event in the queue
        XEvent nextEvent;
        if (XPending(m_display))
        {
            // Grab it but don't remove it from the queue, it still needs to be processed :)
            XPeekEvent(m_display, &nextEvent);
            if (nextEvent.type == KeyPress)
            {
                // Check if it is a duplicated event (same timestamp as the KeyRelease event)
                if ((nextEvent.xkey.keycode == windowEvent.xkey.keycode) &&
                    (nextEvent.xkey.time - windowEvent.xkey.time < 2))
                {
                    // If we don't want repeated events, remove the next KeyPress from the queue
                    if (!m_keyRepeat)
                        XNextEvent(m_display, &nextEvent);

                    // This KeyRelease is a repeated event and we don't want it
                    return false;
                }
            }
        }
    }

    // Convert the X11 event to a sf::Event
    switch (windowEvent.type)
    {
        // Destroy event
        case DestroyNotify:
        {
            // The window is about to be destroyed: we must cleanup resources
            cleanup();
            break;
        }

        // Gain focus event
        case FocusIn:
        {
            // Update the input context
            if (m_inputContext)
                XSetICFocus(m_inputContext);

            // Grab cursor
            if (m_cursorGrabbed)
            {
                // Try multiple times to grab the cursor
                for (unsigned int trial = 0; trial < maxTrialsCount; ++trial)
                {
                    sf::priv::ScopedXcbPtr<xcb_generic_error_t> error(NULL);

                    sf::priv::ScopedXcbPtr<xcb_grab_pointer_reply_t> grabPointerReply(xcb_grab_pointer_reply(
                        m_connection,
                        xcb_grab_pointer(
                            m_connection,
                            true,
                            m_window,
                            XCB_NONE,
                            XCB_GRAB_MODE_ASYNC,
                            XCB_GRAB_MODE_ASYNC,
                            m_window,
                            XCB_NONE,
                            XCB_CURRENT_TIME
                        ),
                        &error
                    ));

                    if (!error && grabPointerReply && (grabPointerReply->status == XCB_GRAB_STATUS_SUCCESS))
                    {
                        m_cursorGrabbed = true;
                        break;
                    }

                    // The cursor grab failed, trying again after a small sleep
                    sf::sleep(sf::milliseconds(50));
                }

                if (!m_cursorGrabbed)
                    err() << "Failed to grab mouse cursor" << std::endl;
            }

            Event event;
            event.type = Event::GainedFocus;
            pushEvent(event);

            // If the window has been previously marked urgent (notification) as a result of a focus request, undo that
            ScopedXcbPtr<xcb_generic_error_t> error(NULL);

            ScopedXcbPtr<xcb_get_property_reply_t> hintsReply(xcb_get_property_reply(
                m_connection,
                xcb_get_property(m_connection, 0, m_window, XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 0, 9),
                &error
            ));

            if (error || !hintsReply)
            {
                err() << "Failed to get WM hints in XCB_FOCUS_IN" << std::endl;
                break;
            }

            WMHints* hints = reinterpret_cast<WMHints*>(xcb_get_property_value(hintsReply.get()));

            if (!hints)
            {
                err() << "Failed to get WM hints in XCB_FOCUS_IN" << std::endl;
                break;
            }

            // Remove urgency (notification) flag from hints
            hints->flags &= ~(1 << 8);

            setWMHints(*hints);

            break;
        }

        // Lost focus event
        case FocusOut:
        {
            // Update the input context
            if (m_inputContext)
                XUnsetICFocus(m_inputContext);

            // Release cursor
            if (m_cursorGrabbed)
            {
                ScopedXcbPtr<xcb_generic_error_t> error(xcb_request_check(
                    m_connection,
                    xcb_ungrab_pointer_checked(
                        m_connection,
                        XCB_CURRENT_TIME
                    )
                ));

                if (error)
                    err() << "Failed to ungrab mouse cursor" << std::endl;
            }

            Event event;
            event.type = Event::LostFocus;
            pushEvent(event);
            break;
        }

        // Resize event
        case ConfigureNotify:
        {
            // ConfigureNotify can be triggered for other reasons, check if the size has actually changed
            if ((windowEvent.xconfigure.width != m_previousSize.x) || (windowEvent.xconfigure.height != m_previousSize.y))
            {
                Event event;
                event.type        = Event::Resized;
                event.size.width  = windowEvent.xconfigure.width;
                event.size.height = windowEvent.xconfigure.height;
                pushEvent(event);

                m_previousSize.x = windowEvent.xconfigure.width;
                m_previousSize.y = windowEvent.xconfigure.height;
            }
            break;
        }

        // Close event
        case ClientMessage:
        {
            static xcb_atom_t wmProtocols = getAtom("WM_PROTOCOLS");

            // Handle window manager protocol messages we support
            if (windowEvent.xclient.message_type == wmProtocols)
            {
                static xcb_atom_t wmDeleteWindow = getAtom("WM_DELETE_WINDOW");
                static xcb_atom_t netWmPing = ewmhSupported() ? getAtom("_NET_WM_PING", true) : XCB_ATOM_NONE;

                if ((windowEvent.xclient.format == 32) && (windowEvent.xclient.data.l[0]) == static_cast<long>(wmDeleteWindow))
                {
                    // Handle the WM_DELETE_WINDOW message
                    Event event;
                    event.type = Event::Closed;
                    pushEvent(event);
                }
                else if (netWmPing && (windowEvent.xclient.format == 32) && (windowEvent.xclient.data.l[0]) == static_cast<long>(netWmPing))
                {
                    // Handle the _NET_WM_PING message, send pong back to WM to show that we are responsive
                    windowEvent.xclient.window = XCBDefaultRootWindow(m_connection);

                    XSendEvent(m_display, windowEvent.xclient.window, False, SubstructureNotifyMask | SubstructureRedirectMask, &windowEvent);
                }
            }
            break;
        }

        // Key down event
        case KeyPress:
        {
            Keyboard::Key key = Keyboard::Unknown;

            // Try each KeySym index (modifier group) until we get a match
            for (int i = 0; i < 4; ++i)
            {
                // Get the SFML keyboard code from the keysym of the key that has been pressed
                key = keysymToSF(XLookupKeysym(&windowEvent.xkey, i));

                if (key != Keyboard::Unknown)
                    break;
            }

            // Fill the event parameters
            // TODO: if modifiers are wrong, use XGetModifierMapping to retrieve the actual modifiers mapping
            Event event;
            event.type        = Event::KeyPressed;
            event.key.code    = key;
            event.key.alt     = windowEvent.xkey.state & Mod1Mask;
            event.key.control = windowEvent.xkey.state & ControlMask;
            event.key.shift   = windowEvent.xkey.state & ShiftMask;
            event.key.system  = windowEvent.xkey.state & Mod4Mask;
            pushEvent(event);

            // Generate a TextEntered event
            if (!XFilterEvent(&windowEvent, None))
            {
                #ifdef X_HAVE_UTF8_STRING
                if (m_inputContext)
                {
                    Status status;
                    Uint8  keyBuffer[16];

                    int length = Xutf8LookupString(
                        m_inputContext,
                        &windowEvent.xkey,
                        reinterpret_cast<char*>(keyBuffer),
                        sizeof(keyBuffer),
                        NULL,
                        &status
                    );

                    if (length > 0)
                    {
                        Uint32 unicode = 0;
                        Utf8::decode(keyBuffer, keyBuffer + length, unicode, 0);
                        if (unicode != 0)
                        {
                            Event textEvent;
                            textEvent.type         = Event::TextEntered;
                            textEvent.text.unicode = unicode;
                            pushEvent(textEvent);
                        }
                    }
                }
                else
                #endif
                {
                    static XComposeStatus status;
                    char keyBuffer[16];
                    if (XLookupString(&windowEvent.xkey, keyBuffer, sizeof(keyBuffer), NULL, &status))
                    {
                        Event textEvent;
                        textEvent.type         = Event::TextEntered;
                        textEvent.text.unicode = static_cast<Uint32>(keyBuffer[0]);
                        pushEvent(textEvent);
                    }
                }
            }

            break;
        }

        // Key up event
        case KeyRelease:
        {
            Keyboard::Key key = Keyboard::Unknown;

            // Try each KeySym index (modifier group) until we get a match
            for (int i = 0; i < 4; ++i)
            {
                // Get the SFML keyboard code from the keysym of the key that has been released
                key = keysymToSF(XLookupKeysym(&windowEvent.xkey, i));

                if (key != Keyboard::Unknown)
                    break;
            }

            // Fill the event parameters
            Event event;
            event.type        = Event::KeyReleased;
            event.key.code    = key;
            event.key.alt     = windowEvent.xkey.state & Mod1Mask;
            event.key.control = windowEvent.xkey.state & ControlMask;
            event.key.shift   = windowEvent.xkey.state & ShiftMask;
            event.key.system  = windowEvent.xkey.state & Mod4Mask;
            pushEvent(event);

            break;
        }

        // Mouse button pressed
        case ButtonPress:
        {
            // XXX: Why button 8 and 9?
            // Because 4 and 5 are the vertical wheel and 6 and 7 are horizontal wheel ;)
            unsigned int button = windowEvent.xbutton.button;
            if ((button == Button1) ||
                (button == Button2) ||
                (button == Button3) ||
                (button == 8) ||
                (button == 9))
            {
                Event event;
                event.type          = Event::MouseButtonPressed;
                event.mouseButton.x = windowEvent.xbutton.x;
                event.mouseButton.y = windowEvent.xbutton.y;
                switch(button)
                {
                    case Button1: event.mouseButton.button = Mouse::Left;     break;
                    case Button2: event.mouseButton.button = Mouse::Middle;   break;
                    case Button3: event.mouseButton.button = Mouse::Right;    break;
                    case 8:       event.mouseButton.button = Mouse::XButton1; break;
                    case 9:       event.mouseButton.button = Mouse::XButton2; break;
                }
                pushEvent(event);
            }
            break;
        }

        // Mouse button released
        case ButtonRelease:
        {
            unsigned int button = windowEvent.xbutton.button;
            if ((button == Button1) ||
                (button == Button2) ||
                (button == Button3) ||
                (button == 8) ||
                (button == 9))
            {
                Event event;
                event.type          = Event::MouseButtonReleased;
                event.mouseButton.x = windowEvent.xbutton.x;
                event.mouseButton.y = windowEvent.xbutton.y;
                switch(button)
                {
                    case Button1: event.mouseButton.button = Mouse::Left;     break;
                    case Button2: event.mouseButton.button = Mouse::Middle;   break;
                    case Button3: event.mouseButton.button = Mouse::Right;    break;
                    case 8:       event.mouseButton.button = Mouse::XButton1; break;
                    case 9:       event.mouseButton.button = Mouse::XButton2; break;
                }
                pushEvent(event);
            }
            else if ((button == Button4) || (button == Button5))
            {
                Event event;

                event.type             = Event::MouseWheelMoved;
                event.mouseWheel.delta = (button == Button4) ? 1 : -1;
                event.mouseWheel.x     = windowEvent.xbutton.x;
                event.mouseWheel.y     = windowEvent.xbutton.y;
                pushEvent(event);

                event.type                   = Event::MouseWheelScrolled;
                event.mouseWheelScroll.wheel = Mouse::VerticalWheel;
                event.mouseWheelScroll.delta = (button == Button4) ? 1 : -1;
                event.mouseWheelScroll.x     = windowEvent.xbutton.x;
                event.mouseWheelScroll.y     = windowEvent.xbutton.y;
                pushEvent(event);
            }
            else if ((button == 6) || (button == 7))
            {
                Event event;
                event.type                   = Event::MouseWheelScrolled;
                event.mouseWheelScroll.wheel = Mouse::HorizontalWheel;
                event.mouseWheelScroll.delta = (button == 6) ? 1 : -1;
                event.mouseWheelScroll.x     = windowEvent.xbutton.x;
                event.mouseWheelScroll.y     = windowEvent.xbutton.y;
                pushEvent(event);
            }
            break;
        }

        // Mouse moved
        case MotionNotify:
        {
            Event event;
            event.type        = Event::MouseMoved;
            event.mouseMove.x = windowEvent.xmotion.x;
            event.mouseMove.y = windowEvent.xmotion.y;
            pushEvent(event);
            break;
        }

        // Mouse entered
        case EnterNotify:
        {
            if (windowEvent.xcrossing.mode == NotifyNormal)
            {
                Event event;
                event.type = Event::MouseEntered;
                pushEvent(event);
            }
            break;
        }

        // Mouse left
        case LeaveNotify:
        {
            if (windowEvent.xcrossing.mode == NotifyNormal)
            {
                Event event;
                event.type = Event::MouseLeft;
                pushEvent(event);
            }
            break;
        }

        // Window unmapped
        case UnmapNotify:
        {
            if (windowEvent.xunmap.window == m_window)
                m_windowMapped = false;

            break;
        }

        // Window visibility change
        case VisibilityNotify:
        {
            // We prefer using VisibilityNotify over MapNotify because
            // some window managers like awesome don't internally flag a
            // window as viewable even after it is mapped but before it
            // is visible leading to certain function calls failing with
            // an unviewable error if called before VisibilityNotify arrives

            // Empirical testing on most widely used window managers shows
            // that mapping a window will always lead to a VisibilityNotify
            // event that is not VisibilityFullyObscured
            if (windowEvent.xvisibility.window == m_window)
            {
                if (windowEvent.xvisibility.state != VisibilityFullyObscured)
                    m_windowMapped = true;
            }

            break;
        }
    }

    return true;
}

} // namespace priv

} // namespace sf
