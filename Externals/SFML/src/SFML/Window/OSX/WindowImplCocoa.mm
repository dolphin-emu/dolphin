////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2016 Marco Antognini (antognini.marco@gmail.com),
//                         Laurent Gomila (laurent@sfml-dev.org)
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
#include <SFML/Window/OSX/WindowImplCocoa.hpp>
#include <SFML/System/Err.hpp>

#import <SFML/Window/OSX/AutoreleasePoolWrapper.h>
#import <SFML/Window/OSX/cpp_objc_conversion.h>
#import <SFML/Window/OSX/Scaling.h>
#import <SFML/Window/OSX/SFApplication.h>
#import <SFML/Window/OSX/SFApplicationDelegate.h>
#import <SFML/Window/OSX/SFKeyboardModifiersHelper.h>
#import <SFML/Window/OSX/SFViewController.h>
#import <SFML/Window/OSX/SFWindowController.h>

namespace sf
{
namespace priv
{

////////////////////////////////////////////////////////////
/// According to Apple's documentation, each invocation of
/// unhide must be balanced by an invocation of hide in
/// order for the cursor display to be correct.
/// So we keep track of those calls ourself.
////////////////////////////////////////////////////////////

namespace
{
    bool isCursorHidden = false; // initially, the cursor is visible
}


////////////////////////////////////////////////////////
void hideMouseCursor()
{
    if (!isCursorHidden)
    {
        [NSCursor hide];
        isCursorHidden = true;
    }
}


////////////////////////////////////////////////////////
void showMouseCursor()
{
    if (isCursorHidden)
    {
        [NSCursor unhide];
        isCursorHidden = false;
    }
}

#pragma mark
#pragma mark WindowImplCocoa's ctor/dtor

////////////////////////////////////////////////////////////
WindowImplCocoa::WindowImplCocoa(WindowHandle handle) :
m_showCursor(true)
{
    // Ask for a pool.
    ensureThreadHasPool();

    // Treat the handle as it real type
    id nsHandle = (id)handle;
    if ([nsHandle isKindOfClass:[NSWindow class]])
    {
        // We have a window.
        m_delegate = [[SFWindowController alloc] initWithWindow:nsHandle];
    }
    else if ([nsHandle isKindOfClass:[NSView class]])
    {
        // We have a view.
        m_delegate = [[SFViewController alloc] initWithView:nsHandle];
    }
    else
    {

        sf::err() << "Cannot import this Window Handle because it is neither "
                  << "a <NSWindow*> nor <NSView*> object "
                  << "(or any of their subclasses). You gave a <"
                  << [[nsHandle className] UTF8String]
                  << "> object."
                  << std::endl;
        return;

    }

    [m_delegate setRequesterTo:this];

    // Finally, set up keyboard helper
    initialiseKeyboardHelper();
}


////////////////////////////////////////////////////////////
WindowImplCocoa::WindowImplCocoa(VideoMode mode,
                                 const String& title,
                                 unsigned long style,
                                 const ContextSettings& /*settings*/) :
m_showCursor(true)
{
    // Transform the app process.
    setUpProcess();

    // Ask for a pool.
    ensureThreadHasPool();

    // Use backing size
    scaleInWidthHeight(mode, nil);

    m_delegate = [[SFWindowController alloc] initWithMode:mode andStyle:style];
    [m_delegate changeTitle:sfStringToNSString(title)];
    [m_delegate setRequesterTo:this];

    // Finally, set up keyboard helper
    initialiseKeyboardHelper();
}


////////////////////////////////////////////////////////////
WindowImplCocoa::~WindowImplCocoa()
{
    [m_delegate closeWindow];

    [m_delegate release];

    // Put the next window in front, if any.
    NSArray* windows = [NSApp orderedWindows];
    if ([windows count] > 0)
        [[windows objectAtIndex:0] makeKeyAndOrderFront:nil];

    drainThreadPool(); // Make sure everything was freed
    // This solve some issue when sf::Window::Create is called for the
    // second time (nothing was render until the function was called again)
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::applyContext(NSOpenGLContextRef context) const
{
    [m_delegate applyContext:context];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setUpProcess(void)
{
    static bool isTheProcessSetAsApplication = false;

    if (!isTheProcessSetAsApplication)
    {
        // Do it only once !
        isTheProcessSetAsApplication = true;

        // Make sure NSApp is properly initialized
        [SFApplication sharedApplication];

        // Set the process as a normal application so it can get focus
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];

        // Register an application delegate if there is none
        if (![[SFApplication sharedApplication] delegate])
            [[NSApplication sharedApplication] setDelegate:[[SFApplicationDelegate alloc] init]];

        // Create menus for the application (before finishing launching!)
        [SFApplication setUpMenuBar];

        // Tell the application to stop bouncing in the Dock.
        [[SFApplication sharedApplication] finishLaunching];
        // NOTE: This last call won't harm anything even if SFML window was
        // created with an external handle.
    }
}


#pragma mark
#pragma mark WindowImplCocoa's window-event methods


////////////////////////////////////////////////////////////
void WindowImplCocoa::windowClosed(void)
{
    Event event;
    event.type = Event::Closed;

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::windowResized(unsigned int width, unsigned int height)
{
    Event event;
    event.type = Event::Resized;
    event.size.width  = width;
    event.size.height = height;
    scaleOutWidthHeight(event.size, m_delegate);

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::windowLostFocus(void)
{
    if (!m_showCursor && [m_delegate isMouseInside])
        showMouseCursor(); // Make sure the cursor is visible

    Event event;
    event.type = Event::LostFocus;

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::windowGainedFocus(void)
{
    if (!m_showCursor && [m_delegate isMouseInside])
        hideMouseCursor(); // Restore user's setting

    Event event;
    event.type = Event::GainedFocus;

    pushEvent(event);
}

#pragma mark
#pragma mark WindowImplCocoa's mouse-event methods


////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseDownAt(Mouse::Button button, int x, int y)
{
    Event event;
    event.type = Event::MouseButtonPressed;
    event.mouseButton.button = button;
    event.mouseButton.x = x;
    event.mouseButton.y = y;
    scaleOutXY(event.mouseButton, m_delegate);

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseUpAt(Mouse::Button button, int x, int y)
{
    Event event;
    event.type = Event::MouseButtonReleased;
    event.mouseButton.button = button;
    event.mouseButton.x = x;
    event.mouseButton.y = y;
    scaleOutXY(event.mouseButton, m_delegate);

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseMovedAt(int x, int y)
{
    Event event;
    event.type = Event::MouseMoved;
    event.mouseMove.x = x;
    event.mouseMove.y = y;
    scaleOutXY(event.mouseMove, m_delegate);

    pushEvent(event);
}

////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseWheelScrolledAt(float deltaX, float deltaY, int x, int y)
{
    Event event;

    event.type = Event::MouseWheelMoved;
    event.mouseWheel.delta = deltaY;
    event.mouseWheel.x = x;
    event.mouseWheel.y = y;
    scaleOutXY(event.mouseWheel, m_delegate);
    pushEvent(event);

    event.type = Event::MouseWheelScrolled;
    event.mouseWheelScroll.wheel = Mouse::VerticalWheel;
    event.mouseWheelScroll.delta = deltaY;
    event.mouseWheelScroll.x = x;
    event.mouseWheelScroll.y = y;
    scaleOutXY(event.mouseWheelScroll, m_delegate);
    pushEvent(event);

    event.type = Event::MouseWheelScrolled;
    event.mouseWheelScroll.wheel = Mouse::HorizontalWheel;
    event.mouseWheelScroll.delta = deltaX;
    event.mouseWheelScroll.x = x;
    event.mouseWheelScroll.y = y;
    scaleOutXY(event.mouseWheelScroll, m_delegate);
    pushEvent(event);
}

////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseMovedIn(void)
{
    if (!m_showCursor)
        hideMouseCursor(); // Restore user's setting

    Event event;
    event.type = Event::MouseEntered;

    pushEvent(event);
}

////////////////////////////////////////////////////////////
void WindowImplCocoa::mouseMovedOut(void)
{
    if (!m_showCursor)
        showMouseCursor(); // Make sure the cursor is visible

    Event event;
    event.type = Event::MouseLeft;

    pushEvent(event);
}


#pragma mark
#pragma mark WindowImplCocoa's key-event methods


////////////////////////////////////////////////////////////
void WindowImplCocoa::keyDown(Event::KeyEvent key)
{
    Event event;
    event.type = Event::KeyPressed;
    event.key = key;

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::keyUp(Event::KeyEvent key)
{
    Event event;
    event.type = Event::KeyReleased;
    event.key = key;

    pushEvent(event);
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::textEntered(unichar charcode)
{
    Event event;
    event.type = Event::TextEntered;
    event.text.unicode = charcode;

    pushEvent(event);
}


#pragma mark
#pragma mark WindowImplCocoa's event-related methods

////////////////////////////////////////////////////////////
void WindowImplCocoa::processEvents()
{
    [m_delegate processEvent];
    drainThreadPool(); // Reduce memory footprint
}

#pragma mark
#pragma mark WindowImplCocoa's private methods

////////////////////////////////////////////////////////////
WindowHandle WindowImplCocoa::getSystemHandle() const
{
    return [m_delegate getSystemHandle];
}


////////////////////////////////////////////////////////////
Vector2i WindowImplCocoa::getPosition() const
{
    NSPoint pos = [m_delegate position];
    sf::Vector2i ret(pos.x, pos.y);
    scaleOutXY(ret, m_delegate);
    return ret;
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setPosition(const Vector2i& position)
{
    sf::Vector2i backingPosition = position;
    scaleInXY(backingPosition, m_delegate);
    [m_delegate setWindowPositionToX:backingPosition.x Y:backingPosition.y];
}


////////////////////////////////////////////////////////////
Vector2u WindowImplCocoa::getSize() const
{
    NSSize size = [m_delegate size];
    Vector2u ret(size.width, size.height);
    scaleOutXY(ret, m_delegate);
    return ret;
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setSize(const Vector2u& size)
{
    sf::Vector2u backingSize = size;
    scaleInXY(backingSize, m_delegate);
    [m_delegate resizeTo:backingSize.x by:backingSize.y];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setTitle(const String& title)
{
    [m_delegate changeTitle:sfStringToNSString(title)];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setIcon(unsigned int width, unsigned int height, const Uint8* pixels)
{
    [m_delegate setIconTo:width by:height with:pixels];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setVisible(bool visible)
{
    if (visible)
        [m_delegate showWindow];
    else
        [m_delegate hideWindow];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setMouseCursorVisible(bool visible)
{
    m_showCursor = visible;

    // If the mouse is over the window, we apply the new setting
    if ([m_delegate isMouseInside])
    {
        if (m_showCursor)
            showMouseCursor();
        else
            hideMouseCursor();
    }
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setMouseCursorGrabbed(bool grabbed)
{
    [m_delegate setCursorGrabbed:grabbed];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::setKeyRepeatEnabled(bool enabled)
{
    if (enabled)
        [m_delegate enableKeyRepeat];
    else
        [m_delegate disableKeyRepeat];
}


////////////////////////////////////////////////////////////
void WindowImplCocoa::requestFocus()
{
    [m_delegate requestFocus];
}


////////////////////////////////////////////////////////////
bool WindowImplCocoa::hasFocus() const
{
    return [m_delegate hasFocus];
}


} // namespace priv

} // namespace sf

