#include "ControllerInterface.h"

#ifdef CIFACE_USE_XINPUT
	#include "XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DINPUT
	#include "DInput/DInput.h"
#endif
#ifdef CIFACE_USE_XLIB
	#include "Xlib/Xlib.h"
#endif
#ifdef CIFACE_USE_OSX
	#include "OSX/OSX.h"
#endif
#ifdef CIFACE_USE_SDL
	#include "SDL/SDL.h"
#endif
#include "Thread.h"


#define INPUT_DETECT_THRESHOLD			0.85

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
	ciface::Xlib::Init(m_devices, m_hwnd);
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::Init(m_devices);
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::Init(m_devices);
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
			(*d)->SetOutputState(*o, 0);
		// update output
		(*d)->UpdateOutput();

		//delete device
		delete *d;
	}

	m_devices.clear();

#ifdef CIFACE_USE_XINPUT
	// nothing needed
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
	if (force)
		update_lock.Enter();
	else if (false == update_lock.TryEnter())
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

	update_lock.Leave();
	return (m_devices.size() == ok_count);
}

//
//		UpdateOutput
//
// update output for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateOutput(const bool force)
{
	if (force)
		update_lock.Enter();
	else if (false == update_lock.TryEnter())
		return false;

	size_t ok_count = 0;

	std::vector<Device*>::const_iterator
		d = m_devices.begin(),
		e = m_devices.end();
	for (;d != e; ++d)
		(*d)->UpdateOutput();

	update_lock.Leave();
	return (m_devices.size() == ok_count);
}

//
//		Device :: ~Device
//
// dtor, delete all inputs/outputs on device destruction
//
ControllerInterface::Device::~Device()
{
	{
	// delete inputs
	std::vector<Device::Input*>::iterator
		i = m_inputs.begin(),
		e = m_inputs.end();
	for ( ;i!=e; ++i)
		delete *i;
	}

	{
	// delete outputs
	std::vector<Device::Output*>::iterator
		o = m_outputs.begin(),
		e = m_outputs.end();
	for ( ;o!=e; ++o)
		delete *o;
	}
}

void ControllerInterface::Device::AddInput(Input* const i)
{
	m_inputs.push_back(i);
}

void ControllerInterface::Device::AddOutput(Output* const o)
{
	m_outputs.push_back(o);
}

//
//		Device :: ClearInputState
//
// device classes should override this func
// ControllerInterface will call this when the device returns failure durring UpdateInput
// used to try to set all buttons and axes to their default state when user unplugs a gamepad durring play
// buttons/axes that were held down at the time of unplugging should be seen as not pressed after unplugging
//
void ControllerInterface::Device::ClearInputState()
{
	// this is going to be called for every UpdateInput call that fails
	// kinda slow but, w/e, should only happen when user unplugs a device while playing
}

//
//		InputReference :: State
//
// get the state of an input reference
// override function for ControlReference::State ...
//
ControlState ControllerInterface::InputReference::State( const ControlState ignore )
{
	//if (NULL == device)
		//return 0;

	ControlState state = 0;

	std::vector<DeviceControl>::const_iterator
		ci = m_controls.begin(),
		ce = m_controls.end();

	// bit of hax for NOT to work at start of expression
	if (ci != ce)
		if (ci->mode == 2)
			state = 1;

	for (; ci!=ce; ++ci)
	{
		const ControlState istate = ci->device->GetInputState((Device::Input*)ci->control);

		switch (ci->mode)
		{
		// OR
		case 0 :
			state = std::max(state, istate);
			break;
		// AND
		case 1 :
			state = std::min(state, istate);
			break;
		// NOT
		case 2 :
			state = std::max(std::min(state, 1.0f - istate), 0.0f);
			break;
		// ADD
		case 3 :
			state += istate;
			break;
		}
	}

	return std::min(1.0f, state * range);
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
	const ControlState tmp_state = std::min(1.0f, state * range);

	// output ref just ignores the modes ( |&!... )

	std::vector<DeviceControl>::iterator
		ci = m_controls.begin(),
		ce = m_controls.end();
	for (; ci != ce; ++ci)
		ci->device->SetOutputState((Device::Output*)ci->control, tmp_state);
	
	return state;	// just return the output, watever
}

//
//		DeviceQualifier :: ToString
//
// get string from a device qualifier / serialize
//
std::string ControllerInterface::DeviceQualifier::ToString() const
{
	if (source.empty() && (cid < 0) && name.empty())
		return "";
	std::ostringstream ss;
	ss << source << '/';
	if ( cid > -1 )
		ss << cid;
	ss << '/' << name;
	return ss.str();
}

//
//		DeviceQualifier	::	FromString
//
// set a device qualifier from a string / unserialize
//
void ControllerInterface::DeviceQualifier::FromString(const std::string& str)
{
	std::istringstream ss(str);

	std::getline(ss, source = "", '/');

	// silly
	std::getline(ss, name, '/');
	std::istringstream(name) >> (cid = -1);

	std::getline(ss, name = "");
}

//
//		DeviceQualifier :: FromDevice
//
// set a device qualifier from a device
//
void ControllerInterface::DeviceQualifier::FromDevice(const ControllerInterface::Device* const dev)
{
	name = dev->GetName();
	cid = dev->GetId();
	source= dev->GetSource();
}

bool ControllerInterface::DeviceQualifier::operator==(const ControllerInterface::Device* const dev) const
{
	if (dev->GetId() == cid)
		if (dev->GetName() == name)
			if (dev->GetSource() == source)
				return true;
	return false;
}

bool ControllerInterface::Device::Control::operator==(const std::string& name) const
{
	return GetName() == name;
}

bool ControllerInterface::DeviceQualifier::operator==(const ControllerInterface::DeviceQualifier& devq) const
{
	if (cid == devq.cid)
		if (name == devq.name)
			if (source == devq.source)
				return true;
	return false;	
}

//
//		UpdateReference
//
// updates a controlreference's binded devices/controls
// need to call this to re-parse a control reference's expression after changing it
//
void ControllerInterface::UpdateReference(ControllerInterface::ControlReference* ref
	, const ControllerInterface::DeviceQualifier& default_device) const
{
	ref->m_controls.clear();

	// adding | to parse the last item, silly
	std::istringstream ss(ref->expression + '|');

	const std::string mode_chars("|&!^");

	ControlReference::DeviceControl	devc;

	std::string	dev_str;
	std::string ctrl_str;

	char c = 0;
	while (ss.read(&c, 1))
	{
		const size_t f = mode_chars.find(c);

		if (mode_chars.npos != f)
		{
			// add ctrl
			if (ctrl_str.size())
			{
				DeviceQualifier	devq;

				// using default device or alterate device inside `backticks`
				if (dev_str.empty())
					devq = default_device;
				else
					devq.FromString(dev_str);

				// find device
				devc.device = FindDevice(devq);

				if (devc.device)
				{
					// control

					// inputs or outputs, i don't like this
					if (ref->is_input)
					{
						std::vector<Device::Input*>::const_iterator i;
						
						// FIXME: Use std::find instead of a for loop
						// i = std::find(devc.device->Inputs().begin(), devc.device->Inputs().end(), ctrl_str);
						for(i = devc.device->Inputs().begin(); i < devc.device->Inputs().end(); ++i)
							if(*(*i) == ctrl_str)
								break;

						if (devc.device->Inputs().end() != i)
						{
							devc.control = *i;
							ref->m_controls.push_back(devc);
						}
						else
						{
							// the input wasn't found, look through all the other devices

							std::vector<Device*>::const_iterator
								deviter = m_devices.begin(),
								devend = m_devices.end();

							for(; deviter != devend; ++deviter)
							{
								for(i = (*deviter)->Inputs().begin(); i < (*deviter)->Inputs().end(); ++i)
									if(*(*i) == ctrl_str)
										break;

								if ((*deviter)->Inputs().end() != i)
								{
									devc.device = *deviter;
									devc.control = *i;
									ref->m_controls.push_back(devc);
									break;
								}
							}
						}
					}
					else
					{
						std::vector<Device::Output*>::const_iterator i;
						
						// FIXME: Use std::find instead of a for loop
						// i = std::find(devc.device->Outputs().begin(), devc.device->Outputs().end(), ctrl_str);
						for(i = devc.device->Outputs().begin(); i < devc.device->Outputs().end(); ++i)
							if(*(*i) == ctrl_str)
								break;

						if (devc.device->Outputs().end() != i)
						{
							devc.control = *i;
							ref->m_controls.push_back(devc);
						}
					}

				}
			}
			// reset stuff for next ctrl
			devc.mode = (int)f;
			devc.device = NULL;
			ctrl_str.clear();
		}
		else if ('`' == c)
		{
			// different device
			if (false == /*XXX*/(bool)std::getline(ss, dev_str, '`'))
				break;
		}
		else
			ctrl_str += c;
	}
}

ControllerInterface::Device* ControllerInterface::FindDevice(const ControllerInterface::DeviceQualifier& devq) const
{
	std::vector<ControllerInterface::Device*>::const_iterator
		di = m_devices.begin(),
		de = m_devices.end();
	for (; di!=de; ++di)
		if (devq == *di)
			return *di;

	return NULL;
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
ControllerInterface::Device::Control* ControllerInterface::InputReference::Detect(const unsigned int ms, Device* const device)
{
	unsigned int time = 0;
	bool* const states = new bool[device->Inputs().size()];

	// get starting state of all inputs, 
	// so we can ignore those that were activated at time of Detect start
	std::vector<Device::Input*>::const_iterator
		i = device->Inputs().begin(),
		e = device->Inputs().end();
	for (bool* state=states; i != e; ++i)
		*state++ = (device->GetInputState(*i) > INPUT_DETECT_THRESHOLD);

	while (time < ms)
	{
		device->UpdateInput();
		i = device->Inputs().begin();
		for (bool* state=states; i != e; ++i,++state)
		{
			// detected an input
			if ((*i)->IsDetectable() && device->GetInputState(*i) > INPUT_DETECT_THRESHOLD)
			{
				// input was released at some point during Detect call
				// return the detected input
				if (false == *state)
					return *i;
			}
			else
				*state = false;
		}
		Common::SleepCurrentThread(10); time += 10;
	}

	delete[] states;

	// no input was detected
	return NULL;
}

//
//		OutputReference :: Detect
//
// totally different from the inputReference detect / i have them combined so it was simplier to make the gui.
// the gui doesnt know the difference between an input and an output / its odd but i was lazy and it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
ControllerInterface::Device::Control* ControllerInterface::OutputReference::Detect(const unsigned int ms, Device* const device)
{
	// ignore device

	// dont hang if we dont even have any controls mapped
	if (m_controls.size())
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
