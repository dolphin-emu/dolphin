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
#ifdef _WIN32_WINDOWS
    #undef _WIN32_WINDOWS
#endif
#ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
#endif
#define _WIN32_WINDOWS 0x0501
#define _WIN32_WINNT   0x0501
#define WINVER         0x0501
#include <SFML/Window/Win32/WindowImplWin32.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <GL/gl.h>
#include <SFML/System/Err.hpp>
#include <SFML/System/Utf.hpp>
#include <vector>

// MinGW lacks the definition of some Win32 constants
#ifndef XBUTTON1
    #define XBUTTON1 0x0001
#endif
#ifndef XBUTTON2
    #define XBUTTON2 0x0002
#endif
#ifndef WM_MOUSEHWHEEL
    #define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef MAPVK_VK_TO_VSC
    #define MAPVK_VK_TO_VSC (0)
#endif


namespace
{
    unsigned int               windowCount      = 0;
    const wchar_t*             className        = L"SFML_Window";
    sf::priv::WindowImplWin32* fullscreenWindow = NULL;

    void setProcessDpiAware()
    {
        // Try SetProcessDpiAwareness first
        HINSTANCE shCoreDll = LoadLibrary(L"Shcore.dll");

        if (shCoreDll)
        {
            enum ProcessDpiAwareness
            {
                ProcessDpiUnaware         = 0,
                ProcessSystemDpiAware     = 1,
                ProcessPerMonitorDpiAware = 2
            };

            typedef HRESULT (WINAPI* SetProcessDpiAwarenessFuncType)(ProcessDpiAwareness);
            SetProcessDpiAwarenessFuncType SetProcessDpiAwarenessFunc = reinterpret_cast<SetProcessDpiAwarenessFuncType>(GetProcAddress(shCoreDll, "SetProcessDpiAwareness"));

            if (SetProcessDpiAwarenessFunc)
            {
                // We only check for E_INVALIDARG because we would get
                // E_ACCESSDENIED if the DPI was already set previously
                // and S_OK means the call was successful
                if (SetProcessDpiAwarenessFunc(ProcessSystemDpiAware) == E_INVALIDARG)
                {
                    sf::err() << "Failed to set process DPI awareness" << std::endl;
                }
                else
                {
                    FreeLibrary(shCoreDll);
                    return;
                }
            }

            FreeLibrary(shCoreDll);
        }

        // Fall back to SetProcessDPIAware if SetProcessDpiAwareness
        // is not available on this system
        HINSTANCE user32Dll = LoadLibrary(L"user32.dll");

        if (user32Dll)
        {
            typedef BOOL (WINAPI* SetProcessDPIAwareFuncType)(void);
            SetProcessDPIAwareFuncType SetProcessDPIAwareFunc = reinterpret_cast<SetProcessDPIAwareFuncType>(GetProcAddress(user32Dll, "SetProcessDPIAware"));

            if (SetProcessDPIAwareFunc)
            {
                if (!SetProcessDPIAwareFunc())
                    sf::err() << "Failed to set process DPI awareness" << std::endl;
            }

            FreeLibrary(user32Dll);
        }
    }
}

namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
WindowImplWin32::WindowImplWin32(WindowHandle handle) :
m_handle          (handle),
m_callback        (0),
m_cursor          (NULL),
m_icon            (NULL),
m_keyRepeatEnabled(true),
m_lastSize        (0, 0),
m_resizing        (false),
m_surrogate       (0),
m_mouseInside     (false),
m_fullscreen      (false),
m_cursorGrabbed   (false)
{
    // Set that this process is DPI aware and can handle DPI scaling
    setProcessDpiAware();

    if (m_handle)
    {
        // We change the event procedure of the control (it is important to save the old one)
        SetWindowLongPtrW(m_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_callback = SetWindowLongPtrW(m_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowImplWin32::globalOnEvent));
    }
}


////////////////////////////////////////////////////////////
WindowImplWin32::WindowImplWin32(VideoMode mode, const String& title, Uint32 style, const ContextSettings& /*settings*/) :
m_handle          (NULL),
m_callback        (0),
m_cursor          (NULL),
m_icon            (NULL),
m_keyRepeatEnabled(true),
m_lastSize        (mode.width, mode.height),
m_resizing        (false),
m_surrogate       (0),
m_mouseInside     (false),
m_fullscreen      (style & Style::Fullscreen),
m_cursorGrabbed   (m_fullscreen)
{
    // Set that this process is DPI aware and can handle DPI scaling
    setProcessDpiAware();

    // Register the window class at first call
    if (windowCount == 0)
        registerWindowClass();

    // Compute position and size
    HDC screenDC = GetDC(NULL);
    int left   = (GetDeviceCaps(screenDC, HORZRES) - static_cast<int>(mode.width))  / 2;
    int top    = (GetDeviceCaps(screenDC, VERTRES) - static_cast<int>(mode.height)) / 2;
    int width  = mode.width;
    int height = mode.height;
    ReleaseDC(NULL, screenDC);

    // Choose the window style according to the Style parameter
    DWORD win32Style = WS_VISIBLE;
    if (style == Style::None)
    {
        win32Style |= WS_POPUP;
    }
    else
    {
        if (style & Style::Titlebar) win32Style |= WS_CAPTION | WS_MINIMIZEBOX;
        if (style & Style::Resize)   win32Style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        if (style & Style::Close)    win32Style |= WS_SYSMENU;
    }

    // In windowed mode, adjust width and height so that window will have the requested client area
    if (!m_fullscreen)
    {
        RECT rectangle = {0, 0, width, height};
        AdjustWindowRect(&rectangle, win32Style, false);
        width  = rectangle.right - rectangle.left;
        height = rectangle.bottom - rectangle.top;
    }

    // Create the window
    m_handle = CreateWindowW(className, title.toWideString().c_str(), win32Style, left, top, width, height, NULL, NULL, GetModuleHandle(NULL), this);

    // By default, the OS limits the size of the window the the desktop size,
    // we have to resize it after creation to apply the real size
    setSize(Vector2u(mode.width, mode.height));

    // Switch to fullscreen if requested
    if (m_fullscreen)
        switchToFullscreen(mode);

    // Increment window count
    windowCount++;
}


////////////////////////////////////////////////////////////
WindowImplWin32::~WindowImplWin32()
{
    // Destroy the custom icon, if any
    if (m_icon)
        DestroyIcon(m_icon);

    if (!m_callback)
    {
        // Destroy the window
        if (m_handle)
            DestroyWindow(m_handle);

        // Decrement the window count
        windowCount--;

        // Unregister window class if we were the last window
        if (windowCount == 0)
            UnregisterClassW(className, GetModuleHandleW(NULL));
    }
    else
    {
        // The window is external: remove the hook on its message callback
        SetWindowLongPtrW(m_handle, GWLP_WNDPROC, m_callback);
    }
}


////////////////////////////////////////////////////////////
WindowHandle WindowImplWin32::getSystemHandle() const
{
    return m_handle;
}


////////////////////////////////////////////////////////////
void WindowImplWin32::processEvents()
{
    // We process the window events only if we own it
    if (!m_callback)
    {
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}


////////////////////////////////////////////////////////////
Vector2i WindowImplWin32::getPosition() const
{
    RECT rect;
    GetWindowRect(m_handle, &rect);

    return Vector2i(rect.left, rect.top);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setPosition(const Vector2i& position)
{
    SetWindowPos(m_handle, NULL, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    if(m_cursorGrabbed)
        grabCursor(true);
}


////////////////////////////////////////////////////////////
Vector2u WindowImplWin32::getSize() const
{
    RECT rect;
    GetClientRect(m_handle, &rect);

    return Vector2u(rect.right - rect.left, rect.bottom - rect.top);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setSize(const Vector2u& size)
{
    // SetWindowPos wants the total size of the window (including title bar and borders),
    // so we have to compute it
    RECT rectangle = {0, 0, static_cast<long>(size.x), static_cast<long>(size.y)};
    AdjustWindowRect(&rectangle, GetWindowLong(m_handle, GWL_STYLE), false);
    int width  = rectangle.right - rectangle.left;
    int height = rectangle.bottom - rectangle.top;

    SetWindowPos(m_handle, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setTitle(const String& title)
{
    SetWindowTextW(m_handle, title.toWideString().c_str());
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setIcon(unsigned int width, unsigned int height, const Uint8* pixels)
{
    // First destroy the previous one
    if (m_icon)
        DestroyIcon(m_icon);

    // Windows wants BGRA pixels: swap red and blue channels
    std::vector<Uint8> iconPixels(width * height * 4);
    for (std::size_t i = 0; i < iconPixels.size() / 4; ++i)
    {
        iconPixels[i * 4 + 0] = pixels[i * 4 + 2];
        iconPixels[i * 4 + 1] = pixels[i * 4 + 1];
        iconPixels[i * 4 + 2] = pixels[i * 4 + 0];
        iconPixels[i * 4 + 3] = pixels[i * 4 + 3];
    }

    // Create the icon from the pixel array
    m_icon = CreateIcon(GetModuleHandleW(NULL), width, height, 1, 32, NULL, &iconPixels[0]);

    // Set it as both big and small icon of the window
    if (m_icon)
    {
        SendMessageW(m_handle, WM_SETICON, ICON_BIG,   (LPARAM)m_icon);
        SendMessageW(m_handle, WM_SETICON, ICON_SMALL, (LPARAM)m_icon);
    }
    else
    {
        err() << "Failed to set the window's icon" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setVisible(bool visible)
{
    ShowWindow(m_handle, visible ? SW_SHOW : SW_HIDE);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setMouseCursorVisible(bool visible)
{
    if (visible)
        m_cursor = LoadCursorW(NULL, IDC_ARROW);
    else
        m_cursor = NULL;

    SetCursor(m_cursor);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setMouseCursorGrabbed(bool grabbed)
{
    m_cursorGrabbed = grabbed;
    grabCursor(m_cursorGrabbed);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setKeyRepeatEnabled(bool enabled)
{
    m_keyRepeatEnabled = enabled;
}


////////////////////////////////////////////////////////////
void WindowImplWin32::requestFocus()
{
    // Allow focus stealing only within the same process; compare PIDs of current and foreground window
    DWORD thisPid       = GetWindowThreadProcessId(m_handle, NULL);
    DWORD foregroundPid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);

    if (thisPid == foregroundPid)
    {
        // The window requesting focus belongs to the same process as the current window: steal focus
        SetForegroundWindow(m_handle);
    }
    else
    {
        // Different process: don't steal focus, but create a taskbar notification ("flash")
        FLASHWINFO info;
        info.cbSize    = sizeof(info);
        info.hwnd      = m_handle;
        info.dwFlags   = FLASHW_TRAY;
        info.dwTimeout = 0;
        info.uCount    = 3;

        FlashWindowEx(&info);
    }
}


////////////////////////////////////////////////////////////
bool WindowImplWin32::hasFocus() const
{
    return m_handle == GetForegroundWindow();
}


////////////////////////////////////////////////////////////
void WindowImplWin32::registerWindowClass()
{
    WNDCLASSW windowClass;
    windowClass.style         = 0;
    windowClass.lpfnWndProc   = &WindowImplWin32::globalOnEvent;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = GetModuleHandleW(NULL);
    windowClass.hIcon         = NULL;
    windowClass.hCursor       = 0;
    windowClass.hbrBackground = 0;
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::switchToFullscreen(const VideoMode& mode)
{
    DEVMODE devMode;
    devMode.dmSize       = sizeof(devMode);
    devMode.dmPelsWidth  = mode.width;
    devMode.dmPelsHeight = mode.height;
    devMode.dmBitsPerPel = mode.bitsPerPixel;
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

    // Apply fullscreen mode
    if (ChangeDisplaySettingsW(&devMode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
    {
        err() << "Failed to change display mode for fullscreen" << std::endl;
        return;
    }

    // Make the window flags compatible with fullscreen mode
    SetWindowLongW(m_handle, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    SetWindowLongW(m_handle, GWL_EXSTYLE, WS_EX_APPWINDOW);

    // Resize the window so that it fits the entire screen
    SetWindowPos(m_handle, HWND_TOP, 0, 0, mode.width, mode.height, SWP_FRAMECHANGED);
    ShowWindow(m_handle, SW_SHOW);

    // Set "this" as the current fullscreen window
    fullscreenWindow = this;
}


////////////////////////////////////////////////////////////
void WindowImplWin32::cleanup()
{
    // Restore the previous video mode (in case we were running in fullscreen)
    if (fullscreenWindow == this)
    {
        ChangeDisplaySettingsW(NULL, 0);
        fullscreenWindow = NULL;
    }

    // Unhide the mouse cursor (in case it was hidden)
    setMouseCursorVisible(true);

    // No longer track the cursor
    setTracking(false);

    // No longer capture the cursor
    ReleaseCapture();
}


////////////////////////////////////////////////////////////
void WindowImplWin32::setTracking(bool track)
{
    TRACKMOUSEEVENT mouseEvent;
    mouseEvent.cbSize = sizeof(TRACKMOUSEEVENT);
    mouseEvent.dwFlags = track ? TME_LEAVE : TME_CANCEL;
    mouseEvent.hwndTrack = m_handle;
    mouseEvent.dwHoverTime = HOVER_DEFAULT;
    TrackMouseEvent(&mouseEvent);
}


////////////////////////////////////////////////////////////
void WindowImplWin32::grabCursor(bool grabbed)
{
    if (grabbed)
    {
        RECT rect;
        GetClientRect(m_handle, &rect);
        MapWindowPoints(m_handle, NULL, reinterpret_cast<LPPOINT>(&rect), 2);
        ClipCursor(&rect);
    }
    else
    {
        ClipCursor(NULL);
    }
}


////////////////////////////////////////////////////////////
void WindowImplWin32::processEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    // Don't process any message until window is created
    if (m_handle == NULL)
        return;

    switch (message)
    {
        // Destroy event
        case WM_DESTROY:
        {
            // Here we must cleanup resources !
            cleanup();
            break;
        }

        // Set cursor event
        case WM_SETCURSOR:
        {
            // The mouse has moved, if the cursor is in our window we must refresh the cursor
            if (LOWORD(lParam) == HTCLIENT)
                SetCursor(m_cursor);

            break;
        }

        // Close event
        case WM_CLOSE:
        {
            Event event;
            event.type = Event::Closed;
            pushEvent(event);
            break;
        }

        // Resize event
        case WM_SIZE:
        {
            // Consider only events triggered by a maximize or a un-maximize
            if (wParam != SIZE_MINIMIZED && !m_resizing && m_lastSize != getSize())
            {
                // Update the last handled size
                m_lastSize = getSize();

                // Push a resize event
                Event event;
                event.type        = Event::Resized;
                event.size.width  = m_lastSize.x;
                event.size.height = m_lastSize.y;
                pushEvent(event);

                // Restore/update cursor grabbing
                grabCursor(m_cursorGrabbed);
            }
            break;
        }

        // Start resizing
        case WM_ENTERSIZEMOVE:
        {
            m_resizing = true;
            grabCursor(false);
            break;
        }

        // Stop resizing
        case WM_EXITSIZEMOVE:
        {
            m_resizing = false;

            // Ignore cases where the window has only been moved
            if(m_lastSize != getSize())
            {
                // Update the last handled size
                m_lastSize = getSize();

                // Push a resize event
                Event event;
                event.type        = Event::Resized;
                event.size.width  = m_lastSize.x;
                event.size.height = m_lastSize.y;
                pushEvent(event);
            }

            // Restore/update cursor grabbing
            grabCursor(m_cursorGrabbed);
            break;
        }

        // The system request the min/max window size and position
        case WM_GETMINMAXINFO:
        {
            // We override the returned information to remove the default limit
            // (the OS doesn't allow windows bigger than the desktop by default)
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMaxTrackSize.x = 50000;
            info->ptMaxTrackSize.y = 50000;
            break;
        }

        // Gain focus event
        case WM_SETFOCUS:
        {
            // Restore cursor grabbing
            grabCursor(m_cursorGrabbed);

            Event event;
            event.type = Event::GainedFocus;
            pushEvent(event);
            break;
        }

        // Lost focus event
        case WM_KILLFOCUS:
        {
            // Ungrab the cursor
            grabCursor(false);

            Event event;
            event.type = Event::LostFocus;
            pushEvent(event);
            break;
        }

        // Text event
        case WM_CHAR:
        {
            if (m_keyRepeatEnabled || ((lParam & (1 << 30)) == 0))
            {
                // Get the code of the typed character
                Uint32 character = static_cast<Uint32>(wParam);

                // Check if it is the first part of a surrogate pair, or a regular character
                if ((character >= 0xD800) && (character <= 0xDBFF))
                {
                    // First part of a surrogate pair: store it and wait for the second one
                    m_surrogate = static_cast<Uint16>(character);
                }
                else
                {
                    // Check if it is the second part of a surrogate pair, or a regular character
                    if ((character >= 0xDC00) && (character <= 0xDFFF))
                    {
                        // Convert the UTF-16 surrogate pair to a single UTF-32 value
                        Uint16 utf16[] = {m_surrogate, static_cast<Uint16>(character)};
                        sf::Utf16::toUtf32(utf16, utf16 + 2, &character);
                        m_surrogate = 0;
                    }

                    // Send a TextEntered event
                    Event event;
                    event.type = Event::TextEntered;
                    event.text.unicode = character;
                    pushEvent(event);
                }
            }
            break;
        }

        // Keydown event
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            if (m_keyRepeatEnabled || ((HIWORD(lParam) & KF_REPEAT) == 0))
            {
                Event event;
                event.type        = Event::KeyPressed;
                event.key.alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
                event.key.control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
                event.key.shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
                event.key.system  = HIWORD(GetAsyncKeyState(VK_LWIN)) || HIWORD(GetAsyncKeyState(VK_RWIN));
                event.key.code    = virtualKeyCodeToSF(wParam, lParam);
                pushEvent(event);
            }
            break;
        }

        // Keyup event
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            Event event;
            event.type        = Event::KeyReleased;
            event.key.alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
            event.key.control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
            event.key.shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
            event.key.system  = HIWORD(GetAsyncKeyState(VK_LWIN)) || HIWORD(GetAsyncKeyState(VK_RWIN));
            event.key.code    = virtualKeyCodeToSF(wParam, lParam);
            pushEvent(event);
            break;
        }

        // Vertical mouse wheel event
        case WM_MOUSEWHEEL:
        {
            // Mouse position is in screen coordinates, convert it to window coordinates
            POINT position;
            position.x = static_cast<Int16>(LOWORD(lParam));
            position.y = static_cast<Int16>(HIWORD(lParam));
            ScreenToClient(m_handle, &position);

            Int16 delta = static_cast<Int16>(HIWORD(wParam));

            Event event;

            event.type             = Event::MouseWheelMoved;
            event.mouseWheel.delta = delta / 120;
            event.mouseWheel.x     = position.x;
            event.mouseWheel.y     = position.y;
            pushEvent(event);

            event.type                   = Event::MouseWheelScrolled;
            event.mouseWheelScroll.wheel = Mouse::VerticalWheel;
            event.mouseWheelScroll.delta = static_cast<float>(delta) / 120.f;
            event.mouseWheelScroll.x     = position.x;
            event.mouseWheelScroll.y     = position.y;
            pushEvent(event);
            break;
        }

        // Horizontal mouse wheel event
        case WM_MOUSEHWHEEL:
        {
            // Mouse position is in screen coordinates, convert it to window coordinates
            POINT position;
            position.x = static_cast<Int16>(LOWORD(lParam));
            position.y = static_cast<Int16>(HIWORD(lParam));
            ScreenToClient(m_handle, &position);

            Int16 delta = static_cast<Int16>(HIWORD(wParam));

            Event event;
            event.type                   = Event::MouseWheelScrolled;
            event.mouseWheelScroll.wheel = Mouse::HorizontalWheel;
            event.mouseWheelScroll.delta = -static_cast<float>(delta) / 120.f;
            event.mouseWheelScroll.x     = position.x;
            event.mouseWheelScroll.y     = position.y;
            pushEvent(event);
            break;
        }

        // Mouse left button down event
        case WM_LBUTTONDOWN:
        {
            Event event;
            event.type               = Event::MouseButtonPressed;
            event.mouseButton.button = Mouse::Left;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse left button up event
        case WM_LBUTTONUP:
        {
            Event event;
            event.type               = Event::MouseButtonReleased;
            event.mouseButton.button = Mouse::Left;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse right button down event
        case WM_RBUTTONDOWN:
        {
            Event event;
            event.type               = Event::MouseButtonPressed;
            event.mouseButton.button = Mouse::Right;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse right button up event
        case WM_RBUTTONUP:
        {
            Event event;
            event.type               = Event::MouseButtonReleased;
            event.mouseButton.button = Mouse::Right;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse wheel button down event
        case WM_MBUTTONDOWN:
        {
            Event event;
            event.type               = Event::MouseButtonPressed;
            event.mouseButton.button = Mouse::Middle;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse wheel button up event
        case WM_MBUTTONUP:
        {
            Event event;
            event.type               = Event::MouseButtonReleased;
            event.mouseButton.button = Mouse::Middle;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse X button down event
        case WM_XBUTTONDOWN:
        {
            Event event;
            event.type               = Event::MouseButtonPressed;
            event.mouseButton.button = HIWORD(wParam) == XBUTTON1 ? Mouse::XButton1 : Mouse::XButton2;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse X button up event
        case WM_XBUTTONUP:
        {
            Event event;
            event.type               = Event::MouseButtonReleased;
            event.mouseButton.button = HIWORD(wParam) == XBUTTON1 ? Mouse::XButton1 : Mouse::XButton2;
            event.mouseButton.x      = static_cast<Int16>(LOWORD(lParam));
            event.mouseButton.y      = static_cast<Int16>(HIWORD(lParam));
            pushEvent(event);
            break;
        }

        // Mouse leave event
        case WM_MOUSELEAVE:
        {
            // Avoid this firing a second time in case the cursor is dragged outside
            if (m_mouseInside)
            {
                m_mouseInside = false;

                // Generate a MouseLeft event
                Event event;
                event.type = Event::MouseLeft;
                pushEvent(event);
            }
            break;
        }

        // Mouse move event
        case WM_MOUSEMOVE:
        {
            // Extract the mouse local coordinates
            int x = static_cast<Int16>(LOWORD(lParam));
            int y = static_cast<Int16>(HIWORD(lParam));

            // Get the client area of the window
            RECT area;
            GetClientRect(m_handle, &area);

            // Capture the mouse in case the user wants to drag it outside
            if ((wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) == 0)
            {
                // Only release the capture if we really have it
                if (GetCapture() == m_handle)
                    ReleaseCapture();
            }
            else if (GetCapture() != m_handle)
            {
                // Set the capture to continue receiving mouse events
                SetCapture(m_handle);
            }

            // If the cursor is outside the client area...
            if ((x < area.left) || (x > area.right) || (y < area.top) || (y > area.bottom))
            {
                // and it used to be inside, the mouse left it.
                if (m_mouseInside)
                {
                    m_mouseInside = false;

                    // No longer care for the mouse leaving the window
                    setTracking(false);

                    // Generate a MouseLeft event
                    Event event;
                    event.type = Event::MouseLeft;
                    pushEvent(event);
                }
            }
            else
            {
                // and vice-versa
                if (!m_mouseInside)
                {
                    m_mouseInside = true;

                    // Look for the mouse leaving the window
                    setTracking(true);

                    // Generate a MouseEntered event
                    Event event;
                    event.type = Event::MouseEntered;
                    pushEvent(event);
                }
            }

            // Generate a MouseMove event
            Event event;
            event.type        = Event::MouseMoved;
            event.mouseMove.x = x;
            event.mouseMove.y = y;
            pushEvent(event);
            break;
        }
    }
}


////////////////////////////////////////////////////////////
Keyboard::Key WindowImplWin32::virtualKeyCodeToSF(WPARAM key, LPARAM flags)
{
    switch (key)
    {
        // Check the scancode to distinguish between left and right shift
        case VK_SHIFT:
        {
            static UINT lShift = MapVirtualKeyW(VK_LSHIFT, MAPVK_VK_TO_VSC);
            UINT scancode = static_cast<UINT>((flags & (0xFF << 16)) >> 16);
            return scancode == lShift ? Keyboard::LShift : Keyboard::RShift;
        }

        // Check the "extended" flag to distinguish between left and right alt
        case VK_MENU : return (HIWORD(flags) & KF_EXTENDED) ? Keyboard::RAlt : Keyboard::LAlt;

        // Check the "extended" flag to distinguish between left and right control
        case VK_CONTROL : return (HIWORD(flags) & KF_EXTENDED) ? Keyboard::RControl : Keyboard::LControl;

        // Other keys are reported properly
        case VK_LWIN:       return Keyboard::LSystem;
        case VK_RWIN:       return Keyboard::RSystem;
        case VK_APPS:       return Keyboard::Menu;
        case VK_OEM_1:      return Keyboard::SemiColon;
        case VK_OEM_2:      return Keyboard::Slash;
        case VK_OEM_PLUS:   return Keyboard::Equal;
        case VK_OEM_MINUS:  return Keyboard::Dash;
        case VK_OEM_4:      return Keyboard::LBracket;
        case VK_OEM_6:      return Keyboard::RBracket;
        case VK_OEM_COMMA:  return Keyboard::Comma;
        case VK_OEM_PERIOD: return Keyboard::Period;
        case VK_OEM_7:      return Keyboard::Quote;
        case VK_OEM_5:      return Keyboard::BackSlash;
        case VK_OEM_3:      return Keyboard::Tilde;
        case VK_ESCAPE:     return Keyboard::Escape;
        case VK_SPACE:      return Keyboard::Space;
        case VK_RETURN:     return Keyboard::Return;
        case VK_BACK:       return Keyboard::BackSpace;
        case VK_TAB:        return Keyboard::Tab;
        case VK_PRIOR:      return Keyboard::PageUp;
        case VK_NEXT:       return Keyboard::PageDown;
        case VK_END:        return Keyboard::End;
        case VK_HOME:       return Keyboard::Home;
        case VK_INSERT:     return Keyboard::Insert;
        case VK_DELETE:     return Keyboard::Delete;
        case VK_ADD:        return Keyboard::Add;
        case VK_SUBTRACT:   return Keyboard::Subtract;
        case VK_MULTIPLY:   return Keyboard::Multiply;
        case VK_DIVIDE:     return Keyboard::Divide;
        case VK_PAUSE:      return Keyboard::Pause;
        case VK_F1:         return Keyboard::F1;
        case VK_F2:         return Keyboard::F2;
        case VK_F3:         return Keyboard::F3;
        case VK_F4:         return Keyboard::F4;
        case VK_F5:         return Keyboard::F5;
        case VK_F6:         return Keyboard::F6;
        case VK_F7:         return Keyboard::F7;
        case VK_F8:         return Keyboard::F8;
        case VK_F9:         return Keyboard::F9;
        case VK_F10:        return Keyboard::F10;
        case VK_F11:        return Keyboard::F11;
        case VK_F12:        return Keyboard::F12;
        case VK_F13:        return Keyboard::F13;
        case VK_F14:        return Keyboard::F14;
        case VK_F15:        return Keyboard::F15;
        case VK_LEFT:       return Keyboard::Left;
        case VK_RIGHT:      return Keyboard::Right;
        case VK_UP:         return Keyboard::Up;
        case VK_DOWN:       return Keyboard::Down;
        case VK_NUMPAD0:    return Keyboard::Numpad0;
        case VK_NUMPAD1:    return Keyboard::Numpad1;
        case VK_NUMPAD2:    return Keyboard::Numpad2;
        case VK_NUMPAD3:    return Keyboard::Numpad3;
        case VK_NUMPAD4:    return Keyboard::Numpad4;
        case VK_NUMPAD5:    return Keyboard::Numpad5;
        case VK_NUMPAD6:    return Keyboard::Numpad6;
        case VK_NUMPAD7:    return Keyboard::Numpad7;
        case VK_NUMPAD8:    return Keyboard::Numpad8;
        case VK_NUMPAD9:    return Keyboard::Numpad9;
        case 'A':           return Keyboard::A;
        case 'Z':           return Keyboard::Z;
        case 'E':           return Keyboard::E;
        case 'R':           return Keyboard::R;
        case 'T':           return Keyboard::T;
        case 'Y':           return Keyboard::Y;
        case 'U':           return Keyboard::U;
        case 'I':           return Keyboard::I;
        case 'O':           return Keyboard::O;
        case 'P':           return Keyboard::P;
        case 'Q':           return Keyboard::Q;
        case 'S':           return Keyboard::S;
        case 'D':           return Keyboard::D;
        case 'F':           return Keyboard::F;
        case 'G':           return Keyboard::G;
        case 'H':           return Keyboard::H;
        case 'J':           return Keyboard::J;
        case 'K':           return Keyboard::K;
        case 'L':           return Keyboard::L;
        case 'M':           return Keyboard::M;
        case 'W':           return Keyboard::W;
        case 'X':           return Keyboard::X;
        case 'C':           return Keyboard::C;
        case 'V':           return Keyboard::V;
        case 'B':           return Keyboard::B;
        case 'N':           return Keyboard::N;
        case '0':           return Keyboard::Num0;
        case '1':           return Keyboard::Num1;
        case '2':           return Keyboard::Num2;
        case '3':           return Keyboard::Num3;
        case '4':           return Keyboard::Num4;
        case '5':           return Keyboard::Num5;
        case '6':           return Keyboard::Num6;
        case '7':           return Keyboard::Num7;
        case '8':           return Keyboard::Num8;
        case '9':           return Keyboard::Num9;
    }

    return Keyboard::Unknown;
}


////////////////////////////////////////////////////////////
LRESULT CALLBACK WindowImplWin32::globalOnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Associate handle and Window instance when the creation message is received
    if (message == WM_CREATE)
    {
        // Get WindowImplWin32 instance (it was passed as the last argument of CreateWindow)
        LONG_PTR window = (LONG_PTR)reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams;

        // Set as the "user data" parameter of the window
        SetWindowLongPtrW(handle, GWLP_USERDATA, window);
    }

    // Get the WindowImpl instance corresponding to the window handle
    WindowImplWin32* window = handle ? reinterpret_cast<WindowImplWin32*>(GetWindowLongPtr(handle, GWLP_USERDATA)) : NULL;

    // Forward the event to the appropriate function
    if (window)
    {
        window->processEvent(message, wParam, lParam);

        if (window->m_callback)
            return CallWindowProcW(reinterpret_cast<WNDPROC>(window->m_callback), handle, message, wParam, lParam);
    }

    // We don't forward the WM_CLOSE message to prevent the OS from automatically destroying the window
    if (message == WM_CLOSE)
        return 0;

    // Don't forward the menu system command, so that pressing ALT or F10 doesn't steal the focus
    if ((message == WM_SYSCOMMAND) && (wParam == SC_KEYMENU))
        return 0;

    return DefWindowProcW(handle, message, wParam, lParam);
}

} // namespace priv

} // namespace sf

