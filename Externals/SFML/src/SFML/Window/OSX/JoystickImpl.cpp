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
#include <SFML/Window/JoystickImpl.hpp>
#include <SFML/Window/OSX/HIDInputManager.hpp>
#include <SFML/Window/OSX/HIDJoystickManager.hpp>
#include <SFML/System/Err.hpp>


namespace
{
    bool JoystickButtonSortPredicate(IOHIDElementRef b1, IOHIDElementRef b2)
    {
        return IOHIDElementGetUsage(b1) < IOHIDElementGetUsage(b2);
    }

    // Convert a CFStringRef to std::string
    std::string stringFromCFString(CFStringRef cfString)
    {
        CFIndex length = CFStringGetLength(cfString);
        std::vector<char> str(length);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
        CFStringGetCString(cfString, &str[0], maxSize, kCFStringEncodingUTF8);
        return &str[0];
    }

    // Get HID device property key as a string
    std::string getDeviceString(IOHIDDeviceRef ref, CFStringRef prop, unsigned int index)
    {
        CFTypeRef typeRef = IOHIDDeviceGetProperty(ref, prop);
        if (typeRef && (CFGetTypeID(typeRef) == CFStringGetTypeID()))
        {
            CFStringRef str = static_cast<CFStringRef>(typeRef);
            return stringFromCFString(str);
        }

        sf::err() << "Unable to read string value for property '" << stringFromCFString(prop) << "' for joystick at index " << index << std::endl;
        return "Unknown Joystick";
    }


    // Get HID device property key as an unsigned int
    unsigned int getDeviceUint(IOHIDDeviceRef ref, CFStringRef prop, unsigned int index)
    {
        CFTypeRef typeRef = IOHIDDeviceGetProperty(ref, prop);
        if (typeRef && (CFGetTypeID(typeRef) == CFNumberGetTypeID()))
        {
            SInt32 value;
            CFNumberGetValue((CFNumberRef)typeRef, kCFNumberSInt32Type, &value);
            return value;
        }

        sf::err() << "Unable to read uint value for property '" << stringFromCFString(prop) << "' for joystick at index " << index << std::endl;
        return 0;
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
JoystickImpl::Location JoystickImpl::m_locationIDs[sf::Joystick::Count] = { 0 };


////////////////////////////////////////////////////////////
void JoystickImpl::initialize()
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
void JoystickImpl::cleanup()
{
    // Nothing to do
}


////////////////////////////////////////////////////////////
bool JoystickImpl::isConnected(unsigned int index)
{
    bool state = false; // Is the index-th joystick connected?

    // First, let's check if the device was previously detected:
    if (m_locationIDs[index] != 0)
    {
        state = true;
    }
    else
    {
        // Otherwise, let's check if it is now connected
        // i.e., m_locationIDs[index] == 0

        // if there is more connected joystick to the HID manager than
        // opened joystick devices then we find the new one.

        unsigned int openedCount = 0;
        for (unsigned int i(0); i < sf::Joystick::Count; ++i)
        {
            if (m_locationIDs[i] != 0)
                openedCount++;
        }


        unsigned int connectedCount = HIDJoystickManager::getInstance().getJoystickCount();

        if (connectedCount > openedCount)
        {
            // Get all devices
            CFSetRef devices = HIDJoystickManager::getInstance().copyJoysticks();

            if (devices != NULL)
            {
                CFIndex size = CFSetGetCount(devices);
                if (size > 0)
                {
                    CFTypeRef array[size]; // array of IOHIDDeviceRef
                    CFSetGetValues(devices, array);

                    // If there exists a device d s.t. there is no j s.t.
                    // m_locationIDs[j] == d's location then we have a new device.

                    for (CFIndex didx(0); didx < size; ++didx)
                    {
                        IOHIDDeviceRef d = (IOHIDDeviceRef)array[didx];
                        Location dloc = HIDInputManager::getLocationID(d);

                        bool foundJ = false;
                        for (unsigned int j(0); j < Joystick::Count; ++j)
                        {
                            if (m_locationIDs[j] == dloc)
                            {
                                foundJ = true;
                                break; // no need to loop again
                            }
                        }

                        if (!foundJ) {
                            // This is a new device
                            // We set it up for Open(..)
                            m_locationIDs[index] = dloc;
                            state = true;
                            break; // We stop looking for a new device
                        }
                    }
                }

                CFRelease(devices);
            }
        }
    }

    return state;
}


////////////////////////////////////////////////////////////
bool JoystickImpl::open(unsigned int index)
{
    m_index = index;
    Location deviceLoc = m_locationIDs[index]; // The device we need to load

    // Get all devices
    CFSetRef devices = HIDJoystickManager::getInstance().copyJoysticks();
    if (devices == NULL)
        return false;

    // Get a usable copy of the joysticks devices.
    CFIndex joysticksCount = CFSetGetCount(devices);
    CFTypeRef devicesArray[joysticksCount];
    CFSetGetValues(devices, devicesArray);

    // Get the desired joystick.
    IOHIDDeviceRef self = 0;
    for (CFIndex i(0); i < joysticksCount; ++i)
    {
        IOHIDDeviceRef d = (IOHIDDeviceRef)devicesArray[i];
        if (deviceLoc == HIDInputManager::getLocationID(d))
        {
            self = d;
            break; // We found it so we stop looping.
        }
    }

    if (self == 0)
    {
        // This shouldn't happen!
        CFRelease(devices);
        return false;
    }

    m_identification.name      = getDeviceString(self, CFSTR(kIOHIDProductKey), m_index);
    m_identification.vendorId  = getDeviceUint(self, CFSTR(kIOHIDVendorIDKey), m_index);
    m_identification.productId = getDeviceUint(self, CFSTR(kIOHIDProductIDKey), m_index);

    // Get a list of all elements attached to the device.
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(self, NULL, kIOHIDOptionsTypeNone);

    if (elements == NULL)
    {
        CFRelease(devices);
        return false;
    }

    // How many elements are there?
    CFIndex elementsCount = CFArrayGetCount(elements);

    if (elementsCount == 0)
    {
        // What is a joystick with no element?
        CFRelease(elements);
        CFRelease(devices);
        return false;
    }

    // Go through all connected elements.
    for (int i = 0; i < elementsCount; ++i)
    {
        IOHIDElementRef element = (IOHIDElementRef) CFArrayGetValueAtIndex(elements, i);
        switch (IOHIDElementGetType(element))
        {
            case kIOHIDElementTypeInput_Misc:
                switch (IOHIDElementGetUsage(element))
                {
                    case kHIDUsage_GD_X:  m_axis[Joystick::X] = element; break;
                    case kHIDUsage_GD_Y:  m_axis[Joystick::Y] = element; break;
                    case kHIDUsage_GD_Z:  m_axis[Joystick::Z] = element; break;
                    case kHIDUsage_GD_Rx: m_axis[Joystick::U] = element; break;
                    case kHIDUsage_GD_Ry: m_axis[Joystick::V] = element; break;
                    case kHIDUsage_GD_Rz: m_axis[Joystick::R] = element; break;
                    default: break;
                    // kHIDUsage_GD_Vx, kHIDUsage_GD_Vy, kHIDUsage_GD_Vz are ignored.
                }
                break;

            case kIOHIDElementTypeInput_Button:
                if (m_buttons.size() < Joystick::ButtonCount) // If we have free slot...
                    m_buttons.push_back(element); // ...we add this element to the list
                // Else: too many buttons. We ignore this one.
                break;

            default: // Make compiler happy
                break;
        }
    }

    // Ensure that the buttons will be indexed in the same order as their
    // HID Usage (assigned by manufacturer and/or a driver).
    std::sort(m_buttons.begin(), m_buttons.end(), JoystickButtonSortPredicate);

    // Note: Joy::AxisPovX/Y are not supported (yet).
    // Maybe kIOHIDElementTypeInput_Axis is the corresponding type but I can't test.

    // Retain all these objects for personal use
    for (ButtonsVector::iterator it(m_buttons.begin()); it != m_buttons.end(); ++it)
        CFRetain(*it);
    for (AxisMap::iterator it(m_axis.begin()); it != m_axis.end(); ++it)
        CFRetain(it->second);

    // Note: we didn't retain element in the switch because we might have multiple
    // Axis X (for example) and we want to keep only the last one. So to prevent
    // leaking we retain objects 'only' now.

    CFRelease(devices);
    CFRelease(elements);

    return true;
}


////////////////////////////////////////////////////////////
void JoystickImpl::close()
{
    for (ButtonsVector::iterator it(m_buttons.begin()); it != m_buttons.end(); ++it)
        CFRelease(*it);
    m_buttons.clear();

    for (AxisMap::iterator it(m_axis.begin()); it != m_axis.end(); ++it)
        CFRelease(it->second);
    m_axis.clear();

    // And we unregister this joystick
    m_locationIDs[m_index] = 0;
}


////////////////////////////////////////////////////////////
JoystickCaps JoystickImpl::getCapabilities() const
{
    JoystickCaps caps;

    // Buttons:
    caps.buttonCount = m_buttons.size();

    // Axis:
    for (AxisMap::const_iterator it(m_axis.begin()); it != m_axis.end(); ++it) {
        caps.axes[it->first] = true;
    }

    return caps;
}


////////////////////////////////////////////////////////////
Joystick::Identification JoystickImpl::getIdentification() const
{
    return m_identification;
}


////////////////////////////////////////////////////////////
JoystickState JoystickImpl::update()
{
    static const JoystickState disconnectedState; // return this if joystick was disconnected
    JoystickState       state; // otherwise return that
    state.connected = true;

    // Note: free up is done in close() which is called, if required,
    //       by the joystick manager. So we don't release buttons nor axes here.

    // First, let's determine if the joystick is still connected
    Location selfLoc = m_locationIDs[m_index];

    // Get all devices
    CFSetRef devices = HIDJoystickManager::getInstance().copyJoysticks();
    if (devices == NULL)
        return disconnectedState;

    // Get a usable copy of the joysticks devices.
    CFIndex joysticksCount = CFSetGetCount(devices);
    CFTypeRef devicesArray[joysticksCount];
    CFSetGetValues(devices, devicesArray);

    // Search for it
    bool found = false;
    for (CFIndex i(0); i < joysticksCount; ++i)
    {
        IOHIDDeviceRef d = (IOHIDDeviceRef)devicesArray[i];
        if (selfLoc == HIDInputManager::getLocationID(d))
        {
            found = true;
            break; // Stop looping
        }
    }

    // Release unused stuff
    CFRelease(devices);

    // If not found we consider it disconnected
    if (!found)
        return disconnectedState;

    // Update buttons' state
    unsigned int i = 0;
    for (ButtonsVector::iterator it(m_buttons.begin()); it != m_buttons.end(); ++it, ++i)
    {
        IOHIDValueRef value = 0;
        IOHIDDeviceGetValue(IOHIDElementGetDevice(*it), *it, &value);

        // Check for plug out.
        if (!value)
        {
            // No value? Hum... Seems like the joystick is gone
            return disconnectedState;
        }

        // 1 means pressed, others mean released
        state.buttons[i] = IOHIDValueGetIntegerValue(value) == 1;
    }

    // Update axes' state
    for (AxisMap::iterator it = m_axis.begin(); it != m_axis.end(); ++it)
    {
        IOHIDValueRef value = 0;
        IOHIDDeviceGetValue(IOHIDElementGetDevice(it->second), it->second, &value);

        // Check for plug out.
        if (!value)
        {
            // No value? Hum... Seems like the joystick is gone
            return disconnectedState;
        }

        // We want to bind [physicalMin,physicalMax] to [-100=min,100=max].
        //
        // General formula to bind [a,b] to [c,d] with a linear progression:
        //
        // f: [a, b] -> [c, d]
        //        x  |->  (x-a)(d-c)/(b-a)+c
        //
        // This method might not be very accurate (the "0 position" can be
        // slightly shift with some device) but we don't care because most
        // of devices are so sensitive that this is not relevant.
        double  physicalMax   = IOHIDElementGetPhysicalMax(it->second);
        double  physicalMin   = IOHIDElementGetPhysicalMin(it->second);
        double  scaledMin     = -100;
        double  scaledMax     =  100;
        double  physicalValue = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);
        float   scaledValue   = (((physicalValue - physicalMin) * (scaledMax - scaledMin)) / (physicalMax - physicalMin)) + scaledMin;
        state.axes[it->first] = scaledValue;
    }

    return state;
}

} // namespace priv

} // namespace sf
