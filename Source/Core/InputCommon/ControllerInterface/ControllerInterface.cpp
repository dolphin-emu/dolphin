#include "ControllerInterface.h"

#if USE_EGL
#include "GLInterface/Platform.h"
#endif

#ifdef CIFACE_USE_XINPUT
	#include "XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DINPUT
	#include "DInput/DInput.h"
#endif
#ifdef CIFACE_USE_XLIB
	#include "Xlib/Xlib.h"
	#ifdef CIFACE_USE_X11_XINPUT2
		#include "Xlib/XInput2.h"
	#endif
#endif
#ifdef CIFACE_USE_OSX
	#include "OSX/OSX.h"
#endif
#ifdef CIFACE_USE_SDL
	#include "SDL/SDL.h"
#endif
#ifdef CIFACE_USE_ANDROID
	#include "Android/Android.h"
#endif

#include "Thread.h"

using namespace ciface::ExpressionParser;

namespace
{
const float INPUT_DETECT_THRESHOLD = 0.55f;
}

ControllerInterface g_controller_interface;

//
//		Init
//
// detect devices and inputs outputs / will make refresh function later
//
void ControllerInterface::Initialize()
{
	if (m_is_init)
		return;

#ifdef CIFACE_USE_DINPUT
	ciface::DInput::Init(m_devices, (HWND)m_hwnd);
#endif
#ifdef CIFACE_USE_XINPUT
	ciface::XInput::Init(m_devices);
#endif
#ifdef CIFACE_USE_XLIB
#if USE_EGL
if (GLWin.platform == EGL_PLATFORM_X11) {
#endif
	ciface::Xlib::Init(m_devices, m_hwnd);
	#ifdef CIFACE_USE_X11_XINPUT2
		ciface::XInput2::Init(m_devices, m_hwnd);
	#endif
#if USE_EGL
}
#endif
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::Init(m_devices, m_hwnd);
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::Init(m_devices);
#endif
#ifdef CIFACE_USE_ANDROID
	ciface::Android::Init(m_devices);
#endif

	m_is_init = true;
}

//
//		DeInit
//
// remove all devices/ call library cleanup functions
//
void ControllerInterface::Shutdown()
{
	if (false == m_is_init)
		return;

	std::vector<Device*>::const_iterator
		d = m_devices.begin(),
		de = m_devices.end();
	for ( ;d != de; ++d )
	{
		std::vector<Device::Output*>::const_iterator
			o = (*d)->Outputs().begin(),
			oe = (*d)->Outputs().end();
		// set outputs to ZERO before destroying device
		for ( ;o!=oe; ++o)
			(*o)->SetState(0);
		// update output
		(*d)->UpdateOutput();

		//delete device
		delete *d;
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
//		SetHwnd
//
// sets the hwnd used for some crap when initializing, use before calling Init
//
void ControllerInterface::SetHwnd( void* const hwnd )
{
	m_hwnd = hwnd;
}

//
//		UpdateInput
//
// update input for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateInput(const bool force)
{
	std::unique_lock<std::recursive_mutex> lk(update_lock, std::defer_lock);

	if (force)
		lk.lock();
	else if (!lk.try_lock())
			return false;

	size_t ok_count = 0;

	std::vector<Device*>::const_iterator
		d = m_devices.begin(),
		e = m_devices.end();
	for ( ;d != e; ++d )
	{
		if ((*d)->UpdateInput())
			++ok_count;
		//else
		// disabled. it might be causing problems
			//(*d)->ClearInputState();
	}

	return (m_devices.size() == ok_count);
}

//
//		UpdateOutput
//
// update output for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateOutput(const bool force)
{
	std::unique_lock<std::recursive_mutex> lk(update_lock, std::defer_lock);

	if (force)
		lk.lock();
	else if (!lk.try_lock())
		return false;

	size_t ok_count = 0;

	for (auto d = m_devices.cbegin(); d != m_devices.cend(); ++d)
	{
		if ((*d)->UpdateOutput())
			++ok_count;
	}

	return (m_devices.size() == ok_count);
}

//
//		InputReference :: State
//
// get the state of an input reference
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
//		OutputReference :: State
//
// set the state of all binded outputs
// overrides ControlReference::State .. combined them so i could make the gui simple / inputs == same as outputs one list
// i was lazy and it works so watever
//
ControlState ControllerInterface::OutputReference::State(const ControlState state)
{
	if (parsed_expression)
		parsed_expression->SetValue(state);
	return 0.0f;
}

//
//		UpdateReference
//
// updates a controlreference's binded devices/controls
// need to call this to re-parse a control reference's expression after changing it
//
void ControllerInterface::UpdateReference(ControllerInterface::ControlReference* ref
	, const DeviceQualifier& default_device) const
{
	delete ref->parsed_expression;
	ref->parsed_expression = NULL;

	ControlFinder finder(*this, default_device, ref->is_input);
	ref->parse_error = ParseExpression(ref->expression, finder, &ref->parsed_expression);
}

//
//		InputReference :: Detect
//
// wait for input on all binded devices
// supports not detecting inputs that were held down at the time of Detect start,
// which is useful for those crazy flightsticks that have certain buttons that are always held down
// or some crazy axes or something
// upon input, return pointer to detected Control
// else return NULL
//
Device::Control* ControllerInterface::InputReference::Detect(const unsigned int ms, Device* const device)
{
	unsigned int time = 0;
	std::vector<bool> states(device->Inputs().size());

	if (device->Inputs().size() == 0)
		return NULL;

	// get starting state of all inputs,
	// so we can ignore those that were activated at time of Detect start
	std::vector<Device::Input*>::const_iterator
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
	return NULL;
}

//
//		OutputReference :: Detect
//
// Totally different from the inputReference detect / I have them combined so it was simpler to make the GUI.
// The GUI doesn't know the difference between an input and an output / it's odd but I was lazy and it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
Device::Control* ControllerInterface::OutputReference::Detect(const unsigned int ms, Device* const device)
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
	return NULL;
}
