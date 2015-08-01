// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fcntl.h>
#include <libudev.h>
#include <unistd.h>

#include "Common/Logging/Log.h"
#include "InputCommon/ControllerInterface/evdev/evdev.h"


namespace ciface
{
namespace evdev
{

static std::string GetName(const std::string& devnode)
{
	int fd = open(devnode.c_str(), O_RDWR|O_NONBLOCK);
	libevdev* dev = nullptr;
	int ret = libevdev_new_from_fd(fd, &dev);
	if (ret != 0)
	{
		close(fd);
		return std::string();
	}
	std::string res = libevdev_get_name(dev);
	libevdev_free(dev);
	close(fd);
	return std::move(res);
}

void Init(std::vector<Core::Device*> &controllerDevices)
{
	// this is used to number the joysticks
	// multiple joysticks with the same name shall get unique ids starting at 0
	std::map<std::string, int> name_counts;

	int num_controllers = 0;

	// We use Udev to find any devices. In the future this will allow for hotplugging.
	// But for now it is essentially iterating over /dev/input/event0 to event31. However if the
	// naming scheme is ever updated in the future, this *should* be forwards compatable.

	struct udev* udev = udev_new();
	_assert_msg_(PAD, udev != 0, "Couldn't initilize libudev.");

	// List all input devices
	udev_enumerate* enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);
	udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);

	// Iterate over all input devices
	udev_list_entry* dev_list_entry;
	udev_list_entry_foreach(dev_list_entry, devices)
	{
		const char* path = udev_list_entry_get_name(dev_list_entry);

		udev_device* dev = udev_device_new_from_syspath(udev, path);

		const char* devnode = udev_device_get_devnode(dev);
		// We only care about devices which we have read/write access to.
		if (access(devnode, W_OK) == 0)
		{
			// Unfortunately udev gives us no way to filter out the non event device interfaces.
			// So we open it and see if it works with evdev ioctls or not.
			std::string name = GetName(devnode);
			evdevDevice* input = new evdevDevice(devnode, name_counts[name]++);

			if (input->IsInteresting())
			{
				controllerDevices.push_back(input);
				num_controllers++;
			}
			else
			{
				// Either it wasn't a evdev device, or it didn't have at least 8 buttons or two axis.
				delete input;
			}
		}
		udev_device_unref(dev);
	}
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
}

evdevDevice::evdevDevice(const std::string &devnode, int id) : m_devfile(devnode), m_id(id)
{
	// The device file will be read on one of the main threads, so we open in non-blocking mode.
	m_fd = open(devnode.c_str(), O_RDWR|O_NONBLOCK);
	int ret = libevdev_new_from_fd(m_fd, &m_dev);

	if (ret != 0)
	{
		// This useally fails because the device node isn't an evdev device, such as /dev/input/js0
		m_initialized = false;
		close(m_fd);
		return;
	}

	m_name = libevdev_get_name(m_dev);

	// Controller buttons (and keyboard keys)
	int num_buttons = 0;
	for (int key = 0; key < KEY_MAX; key++)
		if (libevdev_has_event_code(m_dev, EV_KEY, key))
			AddInput(new Button(num_buttons++, key, m_dev));

	// Absolute axis (thumbsticks)
	int num_axis = 0;
	for (int axis = 0; axis < 0x100; axis++)
		if (libevdev_has_event_code(m_dev, EV_ABS, axis))
		{
			AddAnalogInputs(new Axis(num_axis, axis, false, m_dev),
			                new Axis(num_axis, axis, true, m_dev));
			num_axis++;
		}

	// Force feedback
	if (libevdev_has_event_code(m_dev, EV_FF, FF_PERIODIC))
	{
		for (auto type : {FF_SINE, FF_SQUARE, FF_TRIANGLE, FF_SAW_UP, FF_SAW_DOWN})
			if (libevdev_has_event_code(m_dev, EV_FF, type))
				AddOutput(new ForceFeedback(type, m_dev));
	}
	if (libevdev_has_event_code(m_dev, EV_FF, FF_RUMBLE))
	{
		AddOutput(new ForceFeedback(FF_RUMBLE, m_dev));
	}

	// TODO: Add leds as output devices

	m_initialized = true;
	m_interesting = num_axis >= 2 || num_buttons >= 8;
}

evdevDevice::~evdevDevice()
{
	if (m_initialized)
	{
		libevdev_free(m_dev);
		close(m_fd);
	}
}

void evdevDevice::UpdateInput()
{
	// Run through all evdev events
	// libevdev will keep track of the actual controller state internally which can be queried
	// later with libevdev_fetch_event_value()
	input_event ev;
	int rc = LIBEVDEV_READ_STATUS_SUCCESS;
	do
	{
		if (rc == LIBEVDEV_READ_STATUS_SYNC)
			rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
		else
			rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	} while (rc >= 0);
}


std::string evdevDevice::Button::GetName() const
{
	// Buttons below 0x100 are mostly keyboard keys, and the names make sense
	if (m_code < 0x100)
	{
		const char* name = libevdev_event_code_get_name(EV_KEY, m_code);
		if (name)
			return std::string(name);
	}
	// But controllers use codes above 0x100, and the standard label often doesn't match.
	// We are better off with Button 0 and so on.
	return "Button " + std::to_string(m_index);
}

ControlState evdevDevice::Button::GetState() const
{
	int value = 0;
	libevdev_fetch_event_value(m_dev, EV_KEY, m_code, &value);
	return value;
}

evdevDevice::Axis::Axis(u8 index, u16 code, bool upper, libevdev* dev) :
	m_code(code), m_index(index), m_upper(upper), m_dev(dev)
{
	m_min = libevdev_get_abs_minimum(m_dev, m_code);
	m_range = libevdev_get_abs_maximum(m_dev, m_code) + abs(m_min);
}

std::string evdevDevice::Axis::GetName() const
{
	return "Axis " + std::to_string(m_index) + (m_upper ? "+" : "-");
}

ControlState evdevDevice::Axis::GetState() const
{
	int value = 0;
	libevdev_fetch_event_value(m_dev, EV_ABS, m_code, &value);

	// Value from 0.0 to 1.0
	ControlState fvalue = double(value - m_min) / double(m_range);

	// Split into two axis, each covering half the range from 0.0 to 1.0
	if (m_upper)
		return std::max(0.0, fvalue - 0.5) * 2.0;
	else
		return (0.5 - std::min(0.5, fvalue)) * 2.0;
}

std::string evdevDevice::ForceFeedback::GetName() const
{
	// We have some default names.
	switch (m_type)
	{
	case FF_SINE:
		return "Sine";
	case FF_TRIANGLE:
		return "Triangle";
	case FF_SQUARE:
		return "Square";
	case FF_RUMBLE:
		return "LeftRight";
	default:
		{
			const char* name = libevdev_event_code_get_name(EV_FF, m_type);
			if (name)
				return std::string(name);
			return "Unknown";
		}
	}
}

void evdevDevice::ForceFeedback::SetState(ControlState state)
{
	// libevdev doesn't have nice helpers for forcefeedback
	// we will use the file descriptors directly.

	if (m_id != -1)  // delete the previous effect (which also stops it)
	{
		ioctl(m_fd, EVIOCRMFF, m_id);
		m_id = -1;
	}

	if (state > 0) // Upload and start an effect.
	{
		ff_effect effect;

		effect.id = -1;
		effect.direction = 0; // down
		effect.replay.length = 500; // 500ms
		effect.replay.delay = 0;
		effect.trigger.button = 0; // don't trigger on button press
		effect.trigger.interval = 0;

		// This is the the interface that XInput uses, with 2 motors of differing sizes/frequencies that
		// are controlled seperatally
		if (m_type == FF_RUMBLE)
		{
			effect.type = FF_RUMBLE;
			// max ranges tuned to 'feel' similar in magnitude to triangle/sine on xbox360 controller
			effect.u.rumble.strong_magnitude = u16(state * 0x4000);
			effect.u.rumble.weak_magnitude = u16(state * 0xFFFF);
		}
		else // FF_PERIODIC, a more generic interface.
		{
			effect.type = FF_PERIODIC;
			effect.u.periodic.waveform = m_type;
			effect.u.periodic.phase = 0x7fff; // 180 degrees
			effect.u.periodic.offset = 0;
			effect.u.periodic.period = 10;
			effect.u.periodic.magnitude = s16(state * 0x7FFF);
			effect.u.periodic.envelope.attack_length = 0; // no attack
			effect.u.periodic.envelope.attack_level = 0;
			effect.u.periodic.envelope.fade_length = 0;
			effect.u.periodic.envelope.fade_level = 0;
		}

		ioctl(m_fd, EVIOCSFF, &effect);
		m_id = effect.id;

		input_event play;
		play.type = EV_FF;
		play.code = m_id;
		play.value = 1;

		write(m_fd, (const void*) &play, sizeof(play));
	}
}

evdevDevice::ForceFeedback::~ForceFeedback()
{
	// delete the uploaded effect, so we don't leak it.
	if (m_id != -1)
	{
		ioctl(m_fd, EVIOCRMFF, m_id);
	}
}

}
}
