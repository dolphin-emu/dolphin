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
#include <SFML/Window/OSX/HIDInputManager.hpp>
#include <SFML/Window/OSX/HIDJoystickManager.hpp>

////////////////////////////////////////////////////////////
// Private data
////////////////////////////////////////////////////////////
namespace
{
    // Using a custom run loop mode solve some issues that appears when SFML
    // is used with Cocoa.
    const CFStringRef RunLoopMode = CFSTR("SFML_RUN_LOOP_MODE");
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
HIDJoystickManager& HIDJoystickManager::getInstance()
{
    static HIDJoystickManager manager;
    return manager;
}


////////////////////////////////////////////////////////////
unsigned int HIDJoystickManager::getJoystickCount()
{
    update();
    return m_joystickCount;
}


////////////////////////////////////////////////////////////
CFSetRef HIDJoystickManager::copyJoysticks()
{
    CFSetRef devices = IOHIDManagerCopyDevices(m_manager);
    return devices;
}


////////////////////////////////////////////////////////////
HIDJoystickManager::HIDJoystickManager() :
m_manager(0),
m_joystickCount(0)
{
    m_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

    CFDictionaryRef mask0 = HIDInputManager::copyDevicesMask(kHIDPage_GenericDesktop,
                                                             kHIDUsage_GD_Joystick);

    CFDictionaryRef mask1 = HIDInputManager::copyDevicesMask(kHIDPage_GenericDesktop,
                                                             kHIDUsage_GD_GamePad);

    CFDictionaryRef maskArray[2];
    maskArray[0] = mask0;
    maskArray[1] = mask1;

    CFArrayRef mask = CFArrayCreate(NULL, (const void**)maskArray, 2, NULL);

    IOHIDManagerSetDeviceMatchingMultiple(m_manager, mask);
    CFRelease(mask);
    CFRelease(mask1);
    CFRelease(mask0);


    IOHIDManagerRegisterDeviceMatchingCallback(m_manager, pluggedIn, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager, pluggedOut, this);

    IOHIDManagerScheduleWithRunLoop(m_manager,
                                    CFRunLoopGetCurrent(),
                                    RunLoopMode);

    IOHIDManagerOpen(m_manager, kIOHIDOptionsTypeNone);
}


////////////////////////////////////////////////////////////
HIDJoystickManager::~HIDJoystickManager()
{
    IOHIDManagerUnscheduleFromRunLoop(m_manager,
                                      CFRunLoopGetCurrent(),
                                      RunLoopMode);

    IOHIDManagerRegisterDeviceMatchingCallback(m_manager, NULL, 0);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager, NULL, 0);

    IOHIDManagerClose(m_manager, kIOHIDOptionsTypeNone);
}


////////////////////////////////////////////////////////////
void HIDJoystickManager::update()
{
    SInt32 status = kCFRunLoopRunHandledSource;

    while (status == kCFRunLoopRunHandledSource)
    {
        status = CFRunLoopRunInMode(RunLoopMode, 0, true);
    }
}


////////////////////////////////////////////////////////////
void HIDJoystickManager::pluggedIn(void* context, IOReturn, void*, IOHIDDeviceRef)
{
    HIDJoystickManager* manager = static_cast<HIDJoystickManager*>(context);
    manager->m_joystickCount++;
}


////////////////////////////////////////////////////////////
void HIDJoystickManager::pluggedOut(void* context, IOReturn, void*, IOHIDDeviceRef)
{
    HIDJoystickManager* manager = static_cast<HIDJoystickManager*>(context);
    manager->m_joystickCount--;
}


} // namespace priv

} // namespace sf

