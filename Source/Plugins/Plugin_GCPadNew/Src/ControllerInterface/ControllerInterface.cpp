#include "ControllerInterface.h"

namespace ciface {
}
#ifdef CIFACE_USE_XINPUT
	#include "XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DIRECTINPUT
	#include "DirectInput/DirectInput.h"
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

//#define MAX_DOUBLE_TAP_TIME			400
//#define MAX_HOLD_DOWN_TIME			400
#define INPUT_DETECT_THRESHOLD			0.85

//
//		Init
//
// detect devices and inputs outputs / will make refresh function later
//
void ControllerInterface::Init()
{
	if ( m_is_init )
		return;

#ifdef CIFACE_USE_XINPUT
	ciface::XInput::Init( m_devices );
#endif
#ifdef CIFACE_USE_DIRECTINPUT
	ciface::DirectInput::Init( m_devices/*, (HWND)m_hwnd*/ );
#endif
#ifdef CIFACE_USE_XLIB
	ciface::XLIB::Init( m_devices, m_hwnd );
#endif
#ifdef CIFACE_USE_OSX
	ciface::OSX::Init( m_devices, m_hwnd );
#endif
#ifdef CIFACE_USE_SDL
	ciface::SDL::Init( m_devices );
#endif

	m_is_init = true;
}

//
//		DeInit
//
// remove all devices/ call library cleanup functions
//
void ControllerInterface::DeInit()
{
	if ( false == m_is_init )
		return;

	std::vector<Device*>::const_iterator d = m_devices.begin(),
		Devices_end = m_devices.end();
	for ( ;d != Devices_end; ++d )
	{
		std::vector<Device::Output*>::const_iterator o = (*d)->Outputs().begin(),
		e = (*d)->Outputs().end();
		// set outputs to ZERO before destroying device
		for ( ;o!=e; ++o)
			(*d)->SetOutputState( *o, 0 );
		// update output
		(*d)->UpdateOutput();
		//delete device
		delete *d;
	}

	m_devices.clear();

#ifdef CIFACE_USE_XINPUT
	// nothing needed
#endif
#ifdef CIFACE_USE_DIRECTINPUT
	// nothing needed
#endif
#ifdef CIFACE_USE_XLIB
	// nothing needed
#endif
#ifdef CIFACE_USE_OSX
	// nothing needed
#endif
#ifdef CIFACE_USE_SDL
	// there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
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
bool ControllerInterface::UpdateInput()
{
	size_t ok_count = 0;

	std::vector<Device*>::const_iterator d = m_devices.begin(),
	e = m_devices.end();
	for ( ;d != e; ++d )
	{
		if ( (*d)->UpdateInput() )
			++ok_count;
		else
			(*d)->ClearInputState();
	}

	return ( m_devices.size() == ok_count );
}

//
//		UpdateOutput
//
// update output for all devices, return true if all devices returned successful
//
bool ControllerInterface::UpdateOutput()
{
	size_t ok_count = 0;

	std::vector<Device*>::const_iterator d = m_devices.begin(),
	e = m_devices.end();
	for ( ;d != e; ++d )
		(*d)->UpdateOutput();

	return ( m_devices.size() == ok_count );
}

//
//		Devices
//
// i dont really like this but,
// return : constant copy of the devices vector
//
const std::vector<ControllerInterface::Device*>& ControllerInterface::Devices()
{
	return m_devices;
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
	std::vector<Device::Input*>::iterator i = inputs.begin(),
	e = inputs.end();
	for ( ;i!=e; ++i)
		delete *i;
	}

	{
	// delete outputs
	std::vector<Device::Output*>::iterator o = outputs.begin(),
	e = outputs.end();
	for ( ;o!=e; ++o)
		delete *o;
	}
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
//		Device :: Inputs
//
// get a const version of the device's input vector
//
const std::vector<ControllerInterface::Device::Input*>& ControllerInterface::Device::Inputs()
{
	return inputs;
}

//
//		Device :: Outputs
//
// get a const version of the device's outputs vector
//
const std::vector<ControllerInterface::Device::Output*>& ControllerInterface::Device::Outputs()
{
	return outputs;
}

//
//		HasInit
//
// check if interface is inited
//
bool ControllerInterface::IsInit()
{
	return m_is_init;
}

//
//		InputReference :: State
//
// get the state of an input reference
// override function for ControlReference::State ...
//
ControlState ControllerInterface::InputReference::State( const ControlState ignore )
{
	if ( NULL == device )
		return 0;

	ControlState state;
	// this mode thing will be turned into a switch statement
	switch ( mode )
	{
	// OR
	case 0 :
		{
		state = 0;
		std::vector<Device::Control*>::const_iterator ci = controls.begin(),
			ce = controls.end();
		for ( ; ci != ce; ++ci )
			state = std::max( state, device->GetInputState( (Device::Input*)*ci ) );	// meh casting
		break;
		}
	// AND
	case 1 :
		{
			// TODO: i think i can remove the if here

			state = 1;
			bool is_bound = false;
			std::vector<Device::Control*>::const_iterator ci = controls.begin(),
				ce = controls.end();
			for ( ; ci != ce; ++ci )
			{
				is_bound = true;
				state = std::min( state, device->GetInputState( (Device::Input*)*ci ) );	// meh casting
			}
			if ( !is_bound )
				state = 0;
		break;
		}
	// NOT
	case 2 :
		{
		state = 0;
		std::vector<Device::Control*>::const_iterator ci = controls.begin(),
			ce = controls.end();
		for ( ; ci != ce; ++ci )
			state = std::max( state, device->GetInputState( (Device::Input*)*ci ) );	// meh casting
		state = std::max( 0.0, 1.0 - state );
		break;
		}
	}

	return std::min( 1.0f, state * range );
}

//
//		OutputReference :: State
//
// set the state of all binded outputs
// overrides ControlReference::State .. combined them so i could make the gui simple / inputs == same as outputs one list
// i was lazy and it works so watever
//
ControlState ControllerInterface::OutputReference::State( const ControlState state )
{
	std::vector<Device::Control*>::iterator ci = controls.begin(),
		ce = controls.end();
	for ( ; ci != ce; ++ci )
		device->SetOutputState( (Device::Output*)*ci, state * range );		// casting again
	
	return state;	// just return the output, watever
}

//
//		DeviceQualifier :: ToString
//
// get string from a device qualifier / serialize
//
std::string ControllerInterface::DeviceQualifier::ToString() const
{
	if ( source.empty() && (cid < 0) && name.empty() )
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

	// good
	std::getline( ss, source = "", '/' );


	// dum
	std::getline( ss, name, '/' );
	std::istringstream(name) >> (cid = -1);

	// good
	std::getline( ss, name = "");
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

//
//		DeviceQualifier = = Device*
//
// check if a device matches a device qualifier
//
bool ControllerInterface::DeviceQualifier::operator==(const ControllerInterface::Device* const dev) const
{
	if ( dev->GetName() == name )
		if ( dev->GetId() == cid )
			if ( dev->GetSource() == source )
				return true;
	return false;
}

//
//		ControlQualifier = FromControl
//
// set a control qualifier from a device control
//
void ControllerInterface::ControlQualifier::FromControl(const ControllerInterface::Device::Control* const c)
{
	// hardly needs a function for this
	name = c->GetName();
}

//
//		ControlQualifier = = Device :: Control*
//
// check if a control qualifier matches a device control
// also |control1|control2| form, || matches all
//
bool ControllerInterface::ControlQualifier::operator==(const ControllerInterface::Device::Control* const control) const
{
	if ( name.size() )
	{
		if ( '|' == name[0] && '|' == (*name.rbegin()) )		// check if using |button1|button2| format
		{
			return ( name.find( '|' + control->GetName() + '|' ) != name.npos || "||" == name );
		}
	}
	return (control->GetName() == name);
}

//
//		UpdateReference
//
// updates a controlreference's binded devices then update binded controls
// need to call this after changing a device qualifier on a control reference
// if the device qualifier hasnt changed, the below functions: "UpdateControls" can be used
//
void ControllerInterface::UpdateReference( ControllerInterface::ControlReference* ref )
{
	ref->device = NULL;
	std::vector<Device*>::const_iterator d = m_devices.begin(),
		e = m_devices.end();
	for ( ; d!=e; ++d )
		if ( ref->device_qualifier == *d )
		{
			ref->device = *d;
			break;
		}
	ref->UpdateControls();
}

//
//		InputReference :: UpdateControls
//
// after changing a control qualifier, need to call this to rebind the new matching controls
//
void ControllerInterface::InputReference::UpdateControls()
{
	controls.clear();
	if ( device )
	{
		std::vector<Device::Input*>::const_iterator i = device->Inputs().begin(),
			e = device->Inputs().end();
		for ( ;i != e; ++i )
			if ( control_qualifier == *i )
				controls.push_back( *i );
	}
}

//
//		OutputReference :: UpdateControls
//
// same as the inputRef version
// after changing a control qualifier, need to call this to rebind the new matching controls
//
void ControllerInterface::OutputReference::UpdateControls()
{
	controls.clear();
	if ( device )
	{
		std::vector<Device::Output*>::const_iterator i = device->Outputs().begin(),
			e = device->Outputs().end();
		for ( ;i != e; ++i )
			if ( control_qualifier == *i )
				controls.push_back( *i );
	}

}

//
//		InputReference :: Detect
//
// wait for input on all binded devices
// supports waiting for n number of inputs
// supports not detecting inputs that were held down at the time of Detect start,
// which is useful for those crazy flightsticks that have certain buttons that are always held down
// or some crazy axes or something
// upon input, set control qualifier, update controls and return true
// else return false
//
bool ControllerInterface::InputReference::Detect( const unsigned int ms, const unsigned int count )
{

	unsigned int time = 0;

	// don't wait if we don't have a device
	if ( device )
	{
		bool* const states = new bool[device->Inputs().size()];

		std::vector<Device::Input*>::const_iterator i = device->Inputs().begin(),
			e = device->Inputs().end();
		for ( unsigned int n=0;i != e; ++i,++n )
			states[n] = ( device->GetInputState( *i ) > INPUT_DETECT_THRESHOLD );

		std::vector<Device::Control*>	detected;

		while ( (time < ms) && (detected.size() < count) )
		{
			device->UpdateInput();
			i = device->Inputs().begin();
			e = device->Inputs().end();
			
			for ( unsigned int n=0;i != e; ++i,++n )
			{
				if ( device->GetInputState( *i ) > INPUT_DETECT_THRESHOLD )
				{
					if ( false == states[n] )
						if ( std::find( detected.begin(), detected.end(), *i ) == detected.end() )
							detected.push_back( *i );
				}
				else
					states[n] = false;
			}
			Common::SleepCurrentThread( 10 ); time += 10;
		}

		delete states;

		if ( detected.size() == count )
		{
			controls = detected;

			if ( controls.size() > 1 )
			{
				control_qualifier.name = '|';
				std::vector<Device::Control*>::const_iterator c_i = controls.begin(),
					c_e = controls.end();
				for ( ; c_i != c_e; ++c_i )
					control_qualifier.name += (*c_i)->GetName() + '|';
			}
			else
				control_qualifier.FromControl( controls[0] );

			return true;
		}
	}
	return false;
}

//
//		OutputReference :: Detect
//
// totally different from the inputReference detect / i have them combined so it was simplier to make the gui.
// the gui doesnt know the difference between an input and an output / its odd but i was lazy and it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
bool ControllerInterface::OutputReference::Detect( const unsigned int ms, const unsigned int ignored )
{
	// dont hang if we dont even have any controls mapped
	if ( controls.size() )
	{
		State( 1 );
		unsigned int slept = 0;
		// this loop is to make stuff like flashing keyboard LEDs work
		while ( ms > ( slept += 10 ) )
		{
			device->UpdateOutput();
			Common::SleepCurrentThread( 10 );
		}
		
		State( 0 );
		device->UpdateOutput();
	}
	return false;
}
