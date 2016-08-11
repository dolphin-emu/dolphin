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
#include <SFML/Window/JoystickImpl.hpp>
#include <SFML/System/Err.hpp>
#include <linux/joystick.h>
#include <libudev.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <string>
#include <cstring>

namespace
{
    udev* udevContext = 0;
    udev_monitor* udevMonitor = 0;

    struct JoystickRecord
    {
        std::string deviceNode;
        std::string systemPath;
        bool plugged;
    };

    typedef std::vector<JoystickRecord> JoystickList;
    JoystickList joystickList;

    bool isJoystick(udev_device* udevDevice)
    {
        // If anything goes wrong, we go safe and return true

        // No device to check, assume not a joystick
        if (!udevDevice)
            return false;

        const char* devnode = udev_device_get_devnode(udevDevice);

        // We only consider devices with a device node
        if (!devnode)
            return false;

        // SFML doesn't support evdev yet, so make sure we only handle /js nodes
        if (!std::strstr(devnode, "/js"))
            return false;

        // Check if this device is a joystick
        if (udev_device_get_property_value(udevDevice, "ID_INPUT_JOYSTICK"))
            return true;

        // Check if this device is something that isn't a joystick
        // We do this because the absence of any ID_INPUT_ property doesn't
        // necessarily mean that the device isn't a joystick, whereas the
        // presence of any ID_INPUT_ property that isn't ID_INPUT_JOYSTICK does
        if (udev_device_get_property_value(udevDevice, "ID_INPUT_ACCELEROMETER") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_KEY") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_KEYBOARD") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_MOUSE") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_TABLET") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHPAD") ||
            udev_device_get_property_value(udevDevice, "ID_INPUT_TOUCHSCREEN"))
            return false;

        // On some platforms (older udev), ID_INPUT_ properties are not present, instead
        // the system makes use of the ID_CLASS property to identify the device class
        const char* idClass = udev_device_get_property_value(udevDevice, "ID_CLASS");

        if (idClass)
        {
            // Check if the device class matches joystick
            if (std::strstr(idClass, "joystick"))
                return true;

            // Check if the device class matches something that isn't a joystick
            // Rationale same as above
            if (std::strstr(idClass, "accelerometer") ||
                std::strstr(idClass, "key") ||
                std::strstr(idClass, "keyboard") ||
                std::strstr(idClass, "mouse") ||
                std::strstr(idClass, "tablet") ||
                std::strstr(idClass, "touchpad") ||
                std::strstr(idClass, "touchscreen"))
                return false;
        }

        // At this point, assume it is a joystick
        return true;
    }

    void updatePluggedList(udev_device* udevDevice = NULL)
    {
        if (udevDevice)
        {
            const char* action = udev_device_get_action(udevDevice);

            if (action)
            {
                if (isJoystick(udevDevice))
                {
                    // Since isJoystick returned true, this has to succeed
                    const char* devnode = udev_device_get_devnode(udevDevice);

                    JoystickList::iterator record;

                    for (record = joystickList.begin(); record != joystickList.end(); ++record)
                    {
                        if (record->deviceNode == devnode)
                        {
                            if (std::strstr(action, "add"))
                            {
                                // The system path might have changed so update it
                                const char* syspath = udev_device_get_syspath(udevDevice);

                                record->plugged = true;
                                record->systemPath = syspath ? syspath : "";
                                break;
                            }
                            else if (std::strstr(action, "remove"))
                            {
                                record->plugged = false;
                                break;
                            }
                        }
                    }

                    if (record == joystickList.end())
                    {
                        if (std::strstr(action, "add"))
                        {
                            // If not mapped before and it got added, map it now
                            const char* syspath = udev_device_get_syspath(udevDevice);

                            JoystickRecord record;
                            record.deviceNode = devnode;
                            record.systemPath = syspath ? syspath : "";
                            record.plugged = true;

                            joystickList.push_back(record);
                        }
                        else if (std::strstr(action, "remove"))
                        {
                            // Not mapped during the initial scan, and removed (shouldn't happen)
                            sf::err() << "Trying to disconnect joystick that wasn't connected" << std::endl;
                        }
                    }
                }

                return;
            }

            // Do a full rescan if there was no action just to be sure
        }

        // Reset the plugged status of each mapping since we are doing a full rescan
        for (JoystickList::iterator record = joystickList.begin(); record != joystickList.end(); ++record)
            record->plugged = false;

        udev_enumerate* udevEnumerator = udev_enumerate_new(udevContext);

        if (!udevEnumerator)
        {
            sf::err() << "Error while creating udev enumerator" << std::endl;
            return;
        }

        int result = 0;

        result = udev_enumerate_add_match_subsystem(udevEnumerator, "input");

        if (result < 0)
        {
            sf::err() << "Error while adding udev enumerator match" << std::endl;
            return;
        }

        result = udev_enumerate_scan_devices(udevEnumerator);

        if (result < 0)
        {
            sf::err() << "Error while enumerating udev devices" << std::endl;
            return;
        }

        udev_list_entry* devices = udev_enumerate_get_list_entry(udevEnumerator);
        udev_list_entry* device;

        udev_list_entry_foreach(device, devices) {
            const char* syspath = udev_list_entry_get_name(device);
            udev_device* udevDevice = udev_device_new_from_syspath(udevContext, syspath);

            if (udevDevice && isJoystick(udevDevice))
            {
                // Since isJoystick returned true, this has to succeed
                const char* devnode = udev_device_get_devnode(udevDevice);

                JoystickList::iterator record;

                // Check if the device node has been mapped before
                for (record = joystickList.begin(); record != joystickList.end(); ++record)
                {
                    if (record->deviceNode == devnode)
                    {
                        record->plugged = true;
                        break;
                    }
                }

                // If not mapped before, map it now
                if (record == joystickList.end())
                {
                    JoystickRecord record;
                    record.deviceNode = devnode;
                    record.systemPath = syspath;
                    record.plugged = true;

                    joystickList.push_back(record);
                }
            }

            udev_device_unref(udevDevice);
        }

        udev_enumerate_unref(udevEnumerator);
    }

    bool hasMonitorEvent()
    {
        // This will not fail since we make sure udevMonitor is valid
        int monitorFd = udev_monitor_get_fd(udevMonitor);

        fd_set descriptorSet;
        FD_ZERO(&descriptorSet);
        FD_SET(monitorFd, &descriptorSet);
        timeval timeout = {0, 0};

        return (select(monitorFd + 1, &descriptorSet, NULL, NULL, &timeout) > 0) &&
               FD_ISSET(monitorFd, &descriptorSet);
    }

    // Get a property value from a udev device
    const char* getUdevAttribute(udev_device* udevDevice, const std::string& attributeName)
    {
        return udev_device_get_property_value(udevDevice, attributeName.c_str());
    }

    // Get a system attribute from a USB device
    const char* getUsbAttribute(udev_device* udevDevice, const std::string& attributeName)
    {
        udev_device* udevDeviceParent = udev_device_get_parent_with_subsystem_devtype(udevDevice, "usb", "usb_device");

        if (!udevDeviceParent)
            return NULL;

        return udev_device_get_sysattr_value(udevDeviceParent, attributeName.c_str());
    }

    // Get a USB attribute for a joystick as an unsigned int
    unsigned int getUsbAttributeUint(udev_device* udevDevice, const std::string& attributeName)
    {
        if (!udevDevice)
            return 0;

        const char* attribute = getUsbAttribute(udevDevice, attributeName);
        unsigned int value = 0;

        if (attribute)
            value = static_cast<unsigned int>(std::strtoul(attribute, NULL, 16));

        return value;
    }

    // Get a udev property value for a joystick as an unsigned int
    unsigned int getUdevAttributeUint(udev_device* udevDevice, const std::string& attributeName)
    {
        if (!udevDevice)
            return 0;

        const char* attribute = getUdevAttribute(udevDevice, attributeName);
        unsigned int value = 0;

        if (attribute)
            value = static_cast<unsigned int>(std::strtoul(attribute, NULL, 16));

        return value;
    }

    // Get the joystick vendor id
    unsigned int getJoystickVendorId(unsigned int index)
    {
        if (!udevContext)
        {
            sf::err() << "Failed to get vendor ID of joystick " << joystickList[index].deviceNode << std::endl;
            return 0;
        }

        udev_device* udevDevice = udev_device_new_from_syspath(udevContext, joystickList[index].systemPath.c_str());

        if (!udevDevice)
        {
            sf::err() << "Failed to get vendor ID of joystick " << joystickList[index].deviceNode << std::endl;
            return 0;
        }

        unsigned int id = 0;

        // First try using udev
        id = getUdevAttributeUint(udevDevice, "ID_VENDOR_ID");

        if (id)
        {
            udev_device_unref(udevDevice);
            return id;
        }

        // Fall back to using USB attribute
        id = getUsbAttributeUint(udevDevice, "idVendor");

        udev_device_unref(udevDevice);

        if (id)
            return id;

        sf::err() << "Failed to get vendor ID of joystick " << joystickList[index].deviceNode << std::endl;

        return 0;
    }

    // Get the joystick product id
    unsigned int getJoystickProductId(unsigned int index)
    {
        if (!udevContext)
        {
            sf::err() << "Failed to get product ID of joystick " << joystickList[index].deviceNode << std::endl;
            return 0;
        }

        udev_device* udevDevice = udev_device_new_from_syspath(udevContext, joystickList[index].systemPath.c_str());

        if (!udevDevice)
        {
            sf::err() << "Failed to get product ID of joystick " << joystickList[index].deviceNode << std::endl;
            return 0;
        }

        unsigned int id = 0;

        // First try using udev
        id = getUdevAttributeUint(udevDevice, "ID_MODEL_ID");

        if (id)
        {
            udev_device_unref(udevDevice);
            return id;
        }

        // Fall back to using USB attribute
        id = getUsbAttributeUint(udevDevice, "idProduct");

        udev_device_unref(udevDevice);

        if (id)
            return id;

        sf::err() << "Failed to get product ID of joystick " << joystickList[index].deviceNode << std::endl;

        return 0;
    }

    // Get the joystick name
    std::string getJoystickName(unsigned int index)
    {
        std::string devnode = joystickList[index].deviceNode;

        // First try using ioctl with JSIOCGNAME
        int fd = ::open(devnode.c_str(), O_RDONLY | O_NONBLOCK);

        if (fd >= 0)
        {
            // Get the name
            char name[128];
            std::memset(name, 0, sizeof(name));

            int result = ioctl(fd, JSIOCGNAME(sizeof(name)), name);

            ::close(fd);

            if (result >= 0)
                return std::string(name);
        }

        // Fall back to manual USB chain walk via udev
        if (udevContext)
        {
            udev_device* udevDevice = udev_device_new_from_syspath(udevContext, joystickList[index].systemPath.c_str());

            if (udevDevice)
            {
                const char* product = getUsbAttribute(udevDevice, "product");
                udev_device_unref(udevDevice);

                if (product)
                    return std::string(product);
            }
        }

        sf::err() << "Unable to get name for joystick " << devnode << std::endl;

        return std::string("Unknown Joystick");
    }
}


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
JoystickImpl::JoystickImpl() :
m_file(-1)
{
    std::fill(m_mapping, m_mapping + ABS_MAX + 1, 0);
}


////////////////////////////////////////////////////////////
void JoystickImpl::initialize()
{
    udevContext = udev_new();

    if (!udevContext)
    {
        sf::err() << "Failed to create udev context, joystick support not available" << std::endl;
        return;
    }

    udevMonitor = udev_monitor_new_from_netlink(udevContext, "udev");

    if (!udevMonitor)
    {
        err() << "Failed to create udev monitor, joystick connections and disconnections won't be notified" << std::endl;
    }
    else
    {
        int error = udev_monitor_filter_add_match_subsystem_devtype(udevMonitor, "input", NULL);

        if (error < 0)
        {
            err() << "Failed to add udev monitor filter, joystick connections and disconnections won't be notified: " << error << std::endl;

            udev_monitor_unref(udevMonitor);
            udevMonitor = 0;
        }
        else
        {
            error = udev_monitor_enable_receiving(udevMonitor);

            if (error < 0)
            {
                err() << "Failed to enable udev monitor, joystick connections and disconnections won't be notified: " << error << std::endl;

                udev_monitor_unref(udevMonitor);
                udevMonitor = 0;
            }
        }
    }

    // Do an initial scan
    updatePluggedList();
}


////////////////////////////////////////////////////////////
void JoystickImpl::cleanup()
{
    // Unreference the udev monitor to destroy it
    if (udevMonitor)
    {
        udev_monitor_unref(udevMonitor);
        udevMonitor = 0;
    }

    // Unreference the udev context to destroy it
    if (udevContext)
    {
        udev_unref(udevContext);
        udevContext = 0;
    }
}


////////////////////////////////////////////////////////////
bool JoystickImpl::isConnected(unsigned int index)
{
    // See if we can skip scanning if udev monitor is available
    if (!udevMonitor)
    {
        // udev monitor is not available, perform a scan every query
        updatePluggedList();
    }
    else if (hasMonitorEvent())
    {
        // Check if new joysticks were added/removed since last update
        udev_device* udevDevice = udev_monitor_receive_device(udevMonitor);

        // If we can get the specific device, we check that,
        // otherwise just do a full scan if udevDevice == NULL
        updatePluggedList(udevDevice);

        if (udevDevice)
            udev_device_unref(udevDevice);
    }

    if (index >= joystickList.size())
        return false;

    // Then check if the joystick is connected
    return joystickList[index].plugged;
}

////////////////////////////////////////////////////////////
bool JoystickImpl::open(unsigned int index)
{
    if (index >= joystickList.size())
        return false;

    if (joystickList[index].plugged)
    {
        std::string devnode = joystickList[index].deviceNode;

        // Open the joystick's file descriptor (read-only and non-blocking)
        m_file = ::open(devnode.c_str(), O_RDONLY | O_NONBLOCK);
        if (m_file >= 0)
        {
            // Retrieve the axes mapping
            ioctl(m_file, JSIOCGAXMAP, m_mapping);

            // Get info
            m_identification.name = getJoystickName(index);

            if (udevContext)
            {
                m_identification.vendorId  = getJoystickVendorId(index);
                m_identification.productId = getJoystickProductId(index);
            }

            // Reset the joystick state
            m_state = JoystickState();

            return true;
        }
        else
        {
            err() << "Failed to open joystick " << devnode << ": " << errno << std::endl;
        }
    }

    return false;
}


////////////////////////////////////////////////////////////
void JoystickImpl::close()
{
    ::close(m_file);
    m_file = -1;
}


////////////////////////////////////////////////////////////
JoystickCaps JoystickImpl::getCapabilities() const
{
    JoystickCaps caps;

    if (m_file < 0)
        return caps;

    // Get the number of buttons
    char buttonCount;
    ioctl(m_file, JSIOCGBUTTONS, &buttonCount);
    caps.buttonCount = buttonCount;
    if (caps.buttonCount > Joystick::ButtonCount)
        caps.buttonCount = Joystick::ButtonCount;

    // Get the supported axes
    char axesCount;
    ioctl(m_file, JSIOCGAXES, &axesCount);
    for (int i = 0; i < axesCount; ++i)
    {
        switch (m_mapping[i])
        {
            case ABS_X:        caps.axes[Joystick::X]    = true; break;
            case ABS_Y:        caps.axes[Joystick::Y]    = true; break;
            case ABS_Z:
            case ABS_THROTTLE: caps.axes[Joystick::Z]    = true; break;
            case ABS_RZ:
            case ABS_RUDDER:   caps.axes[Joystick::R]    = true; break;
            case ABS_RX:       caps.axes[Joystick::U]    = true; break;
            case ABS_RY:       caps.axes[Joystick::V]    = true; break;
            case ABS_HAT0X:    caps.axes[Joystick::PovX] = true; break;
            case ABS_HAT0Y:    caps.axes[Joystick::PovY] = true; break;
            default:           break;
        }
    }

    return caps;
}


////////////////////////////////////////////////////////////
Joystick::Identification JoystickImpl::getIdentification() const
{
    return m_identification;
}


////////////////////////////////////////////////////////////
JoystickState JoystickImpl::JoystickImpl::update()
{
    if (m_file < 0)
    {
        m_state = JoystickState();
        return m_state;
    }

    // pop events from the joystick file
    js_event joyState;
    int result = read(m_file, &joyState, sizeof(joyState));
    while (result > 0)
    {
        switch (joyState.type & ~JS_EVENT_INIT)
        {
            // An axis was moved
            case JS_EVENT_AXIS:
            {
                float value = joyState.value * 100.f / 32767.f;

                if (joyState.number < ABS_MAX + 1)
                {
                    switch (m_mapping[joyState.number])
                    {
                        case ABS_X:        m_state.axes[Joystick::X]    = value; break;
                        case ABS_Y:        m_state.axes[Joystick::Y]    = value; break;
                        case ABS_Z:
                        case ABS_THROTTLE: m_state.axes[Joystick::Z]    = value; break;
                        case ABS_RZ:
                        case ABS_RUDDER:   m_state.axes[Joystick::R]    = value; break;
                        case ABS_RX:       m_state.axes[Joystick::U]    = value; break;
                        case ABS_RY:       m_state.axes[Joystick::V]    = value; break;
                        case ABS_HAT0X:    m_state.axes[Joystick::PovX] = value; break;
                        case ABS_HAT0Y:    m_state.axes[Joystick::PovY] = value; break;
                        default:           break;
                    }
                }
                break;
            }

            // A button was pressed
            case JS_EVENT_BUTTON:
            {
                if (joyState.number < Joystick::ButtonCount)
                    m_state.buttons[joyState.number] = (joyState.value != 0);
                break;
            }
        }

        result = read(m_file, &joyState, sizeof(joyState));
    }

    // Check the connection state of the joystick
    // read() returns -1 and errno != EGAIN if it's no longer connected
    // We need to check the result of read() as well, since errno could
    // have been previously set by some other function call that failed
    // result can be either negative or 0 at this point
    // If result is 0, assume the joystick is still connected
    // If result is negative, check errno and disconnect if it is not EAGAIN
    m_state.connected = (!result || (errno == EAGAIN));

    return m_state;
}

} // namespace priv

} // namespace sf
