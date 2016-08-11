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
#import <AppKit/AppKit.h>

namespace sf {
    namespace priv {
        class WindowImplCocoa;
    }
}

@class SFSilentResponder;

////////////////////////////////////////////////////////////
/// \brief Specialized NSOpenGLView
///
/// Handle event and send them back to the requester.
///
/// NSTrackingArea is used to keep track of mouse events. We also
/// need to be able to ignore mouse event when exiting fullscreen.
///
/// Modifiers keys (cmd, ctrl, alt, shift) are handled by this class
/// but the actual logic is done in SFKeyboardModifiersHelper.(h|mm).
///
/// The interface is subdivided into several categories in order
/// to have multiple implementation files to divide this monolithic
/// implementation. However, all attributes are defined in the main
/// interface declaration right below.
///
/// Note about deltaXBuffer and deltaYBuffer: when grabbing the cursor
/// for the first time, either by entering fullscreen or through
/// setCursorGrabbed:, the cursor might be projected into the view.
/// Doing this will result in a big delta (relative movement) in the
/// next move event (cursorPositionFromEvent:), because no move event
/// is generated, which in turn will give the impression that the user
/// want to move the cursor by the same distance it was projected. To
/// prevent the cursor to fly twice the distance we keep track of how
/// much the cursor was projected in deltaXBuffer and deltaYBuffer. In
/// cursorPositionFromEvent: we can then reduce/augment those buffers
/// to determine when a move event should result in an actual move of
/// the cursor (that was disconnected from the system).
///
////////////////////////////////////////////////////////////
@interface SFOpenGLView : NSOpenGLView
{
    sf::priv::WindowImplCocoa*    m_requester;      ///< View's requester
    BOOL                          m_useKeyRepeat;   ///< Key repeat setting
    BOOL                          m_mouseIsIn;      ///< Mouse positional state
    NSTrackingArea*               m_trackingArea;   ///< Mouse tracking area
    BOOL                          m_fullscreen;     ///< Indicate whether the window is fullscreen or not
    CGFloat                       m_scaleFactor;    ///< Display scale factor (e.g. 1x for classic display, 2x for retina)
    BOOL                          m_cursorGrabbed;  ///< Is the mouse cursor trapped?
    CGFloat                       m_deltaXBuffer;   ///< See note about cursor grabbing above
    CGFloat                       m_deltaYBuffer;   ///< See note about cursor grabbing above

    // Hidden text view used to convert key event to actual chars.
    // We use a silent responder to prevent sound alerts.
    SFSilentResponder*  m_silentResponder;
    NSTextView*         m_hiddenTextView;
}

////////////////////////////////////////////////////////////
/// \brief Create the SFML OpenGL view
///
/// NB: -initWithFrame: is also implemented to default isFullscreen to NO
/// in case SFOpenGLView is created with the standard message.
///
/// To finish the initialization -finishInit should be called too.
///
/// \param frameRect dimension of the view
/// \param isFullscreen fullscreen flag
///
/// \return an initialized view
///
////////////////////////////////////////////////////////////
-(id)initWithFrame:(NSRect)frameRect fullscreen:(BOOL)isFullscreen;

////////////////////////////////////////////////////////////
/// \brief Finish the creation of the SFML OpenGL view
///
/// This method should be called after the view was added to a window
///
////////////////////////////////////////////////////////////
-(void)finishInit;

////////////////////////////////////////////////////////////
/// \brief Apply the given requester to the view
///
/// \param requester new 'requester' of the view
///
////////////////////////////////////////////////////////////
-(void)setRequesterTo:(sf::priv::WindowImplCocoa*)requester;

////////////////////////////////////////////////////////////
/// \brief Compute the position in global coordinate
///
/// \param point a point in SFML coordinate
///
/// \return the global coordinates of the point
///
////////////////////////////////////////////////////////////
-(NSPoint)computeGlobalPositionOfRelativePoint:(NSPoint)point;

////////////////////////////////////////////////////////////
/// \brief Get the display scale factor
///
/// \return e.g. 1.0 for classic display, 2.0 for retina display
///
////////////////////////////////////////////////////////////
-(CGFloat)displayScaleFactor;

@end

@interface SFOpenGLView (keyboard)

////////////////////////////////////////////////////////////
/// \brief Enable key repeat
///
////////////////////////////////////////////////////////////
-(void)enableKeyRepeat;

////////////////////////////////////////////////////////////
/// \brief Disable key repeat
///
////////////////////////////////////////////////////////////
-(void)disableKeyRepeat;

@end

@interface SFOpenGLView (mouse)

////////////////////////////////////////////////////////////
/// \brief Compute the position of the cursor
///
/// \param eventOrNil if nil the cursor position is the current one
///
/// \return the mouse position in SFML coord system
///
////////////////////////////////////////////////////////////
-(NSPoint)cursorPositionFromEvent:(NSEvent*)eventOrNil;

////////////////////////////////////////////////////////////
/// \brief Determine where the mouse is
///
/// \return true when the mouse is inside the OpenGL view, false otherwise
///
////////////////////////////////////////////////////////////
-(BOOL)isMouseInside;

////////////////////////////////////////////////////////////
/// Clips or releases the mouse cursor
///
/// Generate a MouseEntered event when it makes sense.
///
/// \param grabbed YES to grab, NO to release
///
////////////////////////////////////////////////////////////
-(void)setCursorGrabbed:(BOOL)grabbed;

////////////////////////////////////////////////////////////
/// Update the cursor position according to the grabbing behaviour
///
/// This function has to be called when the window's state change
///
////////////////////////////////////////////////////////////
-(void)updateCursorGrabbed;

@end
