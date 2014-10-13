// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifdef CIFACE_USE_XINPUT
	#include "InputCommon/ControllerInterface/XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DINPUT
	#include "InputCommon/ControllerInterface/DInput/DInput.h"
#endif
#ifdef CIFACE_USE_XLIB
	#include "InputCommon/ControllerInterface/Xlib/Xlib.h"
	#ifdef CIFACE_USE_X11_XINPUT2
		#include "InputCommon/ControllerInterface/Xlib/XInput2.h"
	#endif
#endif
#ifdef CIFACE_USE_OSX
	#include "InputCommon/ControllerInterface/OSX/OSX.h"
#endif
#ifdef CIFACE_USE_SDL
	#include "InputCommon/ControllerInterface/SDL/SDL.h"
#endif
#ifdef CIFACE_USE_ANDROID
	#include "InputCommon/ControllerInterface/Android/Android.h"
#endif

using namespace ciface::ExpressionParser;

namespace
{
const float INPUT_DETECT_THRESHOLD = 0.55f;
}

ControllerInterface g_controller_interface;

//
// Init
//
// Detect devices and inputs outputs / will make refresh function later
//
void ControllerInterface::Initialize(void* const hwnd)
{
	if (m_is_init)
		return;

	m_hwnd = hwnd;

#ifdef CIFACE_USE_DINPUT
	ciface::DInput::Init(m_devices, (HWND)hwnd);
#endif
#ifdef CIFACE_USE_XINPUT
	ciface::XInput::Init(m_devices);
#endif
#ifdef CIFACE_USE_XLIB
	ciface::Xlib::Init(m_devices, hwnd);
	#ifdef CIFACE_USE_X11_XINPUT2
	ciface::XInput2::Init(m_devices, hwnd);
	#endif
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::Init(m_devices, hwnd);
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::Init(m_devices);
#endif
#ifdef CIFACE_USE_ANDROID
	ciface::Android::Init(m_devices);
#endif

	m_is_init = true;
}

void ControllerInterface::Reinitialize()
{
	if (!m_is_init)
		return;

	Shutdown();
	Initialize(m_hwnd);
}

//
// DeInit
//
// Remove all devices/ call library cleanup functions
//
void ControllerInterface::Shutdown()
{
	if (!m_is_init)
		return;

	for (ciface::Core::Device* d : m_devices)
	{
		// Set outputs to ZERO before destroying device
		for (ciface::Core::Device::Output* o : d->Outputs())
			o->SetState(0);

		// Update output
		d->UpdateOutput();

		// Delete device
		delete d;
	}

	m_devices.clear();

#ifdef CIFACE_USE_XINPUT
	ciface::XInput::DeInit();
#endif
#ifdef CIFACE_USE_DINPUT
	// nothing needed
#endif
#ifdef CIFACE_USE_XLIB
	// nothing needed
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::DeInit();
#endif
#ifdef CIFACE_USE_SDL
	// TODO: there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
	SDL_Quit();
#endif
#ifdef CIFACE_USE_ANDROID
	// nothing needed
#endif

	m_is_init = false;
}

//
// UpdateInput
//
// Update input for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateInput(const bool force)
{
	std::unique_lock<std::recursive_mutex> lk(update_lock, std::defer_lock);

	if (force)
		lk.lock();
	else if (!lk.try_lock())
		return false;

	size_t ok_count = 0;

	for (ciface::Core::Device* d : m_devices)
	{
		if (d->UpdateInput())
			++ok_count;
		//else
		// disabled. it might be causing problems
			//(*d)->ClearInputState();
	}

	return (m_devices.size() == ok_count);
}

//
// UpdateOutput
//
// Update output for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateOutput(const bool force)
{
	std::unique_lock<std::recursive_mutex> lk(update_lock, std::defer_lock);

	if (force)
		lk.lock();
	else if (!lk.try_lock())
		return false;

	size_t ok_count = 0;

	for (ciface::Core::Device* d : m_devices)
	{
		if (d->UpdateOutput())
			++ok_count;
	}

	return (m_devices.size() == ok_count);
}

//
// InputReference :: State
//
// Gets the state of an input reference
// override function for ControlReference::State ...
//
ControlState ControllerInterface::InputReference::State( const ControlState ignore )
{
	if (parsed_expression)
		return parsed_expression->GetValue() * range;
	else
		return 0.0f;
}

//
// OutputReference :: State
//
// Set the state of all binded outputs
// overrides ControlReference::State .. combined them so I could make the GUI simple / inputs == same as outputs one list
// I was lazy and it works so watever
//
ControlState ControllerInterface::OutputReference::State(const ControlState state)
{
	if (parsed_expression)
		parsed_expression->SetValue(state);
	return 0.0f;
}

//
// UpdateReference
//
// Updates a controlreference's binded devices/controls
// need to call this to re-parse a control reference's expression after changing it
//
void ControllerInterface::UpdateReference(ControllerInterface::ControlReference* ref
	, const ciface::Core::DeviceQualifier& default_device) const
{
	delete ref->parsed_expression;
	ref->parsed_expression = nullptr;

	ControlFinder finder(*this, default_device, ref->is_input);
	ref->parse_error = ParseExpression(ref->expression, finder, &ref->parsed_expression);
}

//
// InputReference :: Detect
//
// Wait for input on all binded devices
// supports not detecting inputs that were held down at the time of Detect start,
// which is useful for those crazy flightsticks that have certain buttons that are always held down
// or some crazy axes or something
// upon input, return pointer to detected Control
// else return nullptr
//
ciface::Core::Device::Control* ControllerInterface::InputReference::Detect(const unsigned int ms, ciface::Core::Device* const device)
{
	unsigned int time = 0;
	std::vector<bool> states(device->Inputs().size());

	if (device->Inputs().size() == 0)
		return nullptr;

	// get starting state of all inputs,
	// so we can ignore those that were activated at time of Detect start
	std::vector<ciface::Core::Device::Input*>::const_iterator
		i = device->Inputs().begin(),
		e = device->Inputs().end();
	for (std::vector<bool>::iterator state = states.begin(); i != e; ++i)
		*state++ = ((*i)->GetState() > (1 - INPUT_DETECT_THRESHOLD));

	while (time < ms)
	{
		device->UpdateInput();
		i = device->Inputs().begin();
		for (std::vector<bool>::iterator state = states.begin(); i != e; ++i,++state)
		{
			// detected an input
			if ((*i)->IsDetectable() && (*i)->GetState() > INPUT_DETECT_THRESHOLD)
			{
				// input was released at some point during Detect call
				// return the detected input
				if (false == *state)
					return *i;
			}
			else if ((*i)->GetState() < (1 - INPUT_DETECT_THRESHOLD))
			{
				*state = false;
			}
		}
		Common::SleepCurrentThread(10); time += 10;
	}

	// no input was detected
	return nullptr;
}

//
// OutputReference :: Detect
//
// Totally different from the inputReference detect / I have them combined so it was simpler to make the GUI.
// The GUI doesn't know the difference between an input and an output / it's odd but I was lazy and it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
ciface::Core::Device::Control* ControllerInterface::OutputReference::Detect(const unsigned int ms, ciface::Core::Device* const device)
{
	// ignore device

	// don't hang if we don't even have any controls mapped
	if (BoundCount() > 0)
	{
		State(1);
		unsigned int slept = 0;

		// this loop is to make stuff like flashing keyboard LEDs work
		while (ms > (slept += 10))
		{
			// TODO: improve this to update more than just the default device's output
			device->UpdateOutput();
			Common::SleepCurrentThread(10);
		}

		State(0);
		device->UpdateOutput();
	}
	return nullptr;
}
