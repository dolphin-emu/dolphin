#include "../ControllerInterface.h"

#ifdef CIFACE_USE_DIRECTINPUT_JOYSTICK

#include "DirectInputJoystick.h"
#include <StringUtil.h>

inline bool operator<(const GUID & lhs, const GUID & rhs)
{
	return memcmp(&lhs, &rhs, sizeof(GUID)) < 0;
}

namespace ciface
{
namespace DirectInput
{

#define DATA_BUFFER_SIZE	32

#ifdef NO_DUPLICATE_DINPUT_XINPUT
//-----------------------------------------------------------------------------
// Modified some MSDN code to get all the XInput device GUID.Data1 values in a vector,
// faster than checking all the devices for each DirectInput device, like MSDN says to do
//-----------------------------------------------------------------------------
void GetXInputGUIDS( std::vector<DWORD>& guids )
{

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

	IWbemLocator*			pIWbemLocator  = NULL;
	IEnumWbemClassObject*   pEnumDevices   = NULL;
	IWbemClassObject*		pDevices[20]   = {0};
	IWbemServices*			pIWbemServices = NULL;
	BSTR					bstrNamespace  = NULL;
	BSTR					bstrDeviceID   = NULL;
	BSTR					bstrClassName  = NULL;
	DWORD					uReturned	   = 0;
	VARIANT					var;
	HRESULT					hr;

	// CoInit if needed
	hr = CoInitialize(NULL);
	bool bCleanupCOM = SUCCEEDED(hr);

	// Create WMI
	hr = CoCreateInstance( __uuidof(WbemLocator),
						   NULL,
						   CLSCTX_INPROC_SERVER,
						   __uuidof(IWbemLocator),
						   (LPVOID*) &pIWbemLocator);
	if( FAILED(hr) || pIWbemLocator == NULL )
		goto LCleanup;

	bstrNamespace = SysAllocString( L"\\\\.\\root\\cimv2" );if( bstrNamespace == NULL ) goto LCleanup;		
	bstrClassName = SysAllocString( L"Win32_PNPEntity" );   if( bstrClassName == NULL ) goto LCleanup;		
	bstrDeviceID  = SysAllocString( L"DeviceID" );			if( bstrDeviceID == NULL )  goto LCleanup;		
	
	// Connect to WMI 
	hr = pIWbemLocator->ConnectServer( bstrNamespace, NULL, NULL, 0L, 0L, NULL, NULL, &pIWbemServices );
	if( FAILED(hr) || pIWbemServices == NULL )
		goto LCleanup;

	// Switch security level to IMPERSONATE. 
	CoSetProxyBlanket( pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
					   RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE );					

	hr = pIWbemServices->CreateInstanceEnum( bstrClassName, 0, NULL, &pEnumDevices ); 
	if( FAILED(hr) || pEnumDevices == NULL )
		goto LCleanup;

	// Loop over all devices
	while( true )
	{
		// Get 20 at a time
		hr = pEnumDevices->Next( 10000, 20, pDevices, &uReturned );
		if( FAILED(hr) || uReturned == 0 )
			break;

		for( UINT iDevice=0; iDevice<uReturned; ++iDevice )
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get( bstrDeviceID, 0L, &var, NULL, NULL );
			if( SUCCEEDED( hr ) && var.vt == VT_BSTR && var.bstrVal != NULL )
			{
				// Check if the device ID contains "IG_".  If it does, then it's an XInput device
					// This information can not be found from DirectInput 
				if( wcsstr( var.bstrVal, L"IG_" ) )
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR* strVid = wcsstr( var.bstrVal, L"VID_" );
					if( strVid && swscanf( strVid, L"VID_%4X", &dwVid ) != 1 )
						dwVid = 0;
					WCHAR* strPid = wcsstr( var.bstrVal, L"PID_" );
					if( strPid && swscanf( strPid, L"PID_%4X", &dwPid ) != 1 )
						dwPid = 0;

					// Compare the VID/PID to the DInput device
					DWORD dwVidPid = MAKELONG( dwVid, dwPid );
					guids.push_back( dwVidPid );
					//bIsXinputDevice = true;
				}
			} 
			SAFE_RELEASE( pDevices[iDevice] );
		}
	}

LCleanup:
	if(bstrNamespace)
		SysFreeString(bstrNamespace);
	if(bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if(bstrClassName)
		SysFreeString(bstrClassName);
	for( UINT iDevice=0; iDevice<20; iDevice++ )
		SAFE_RELEASE( pDevices[iDevice] );
	SAFE_RELEASE( pEnumDevices );
	SAFE_RELEASE( pIWbemLocator );
	SAFE_RELEASE( pIWbemServices );

	if( bCleanupCOM )
		CoUninitialize();
}
#endif

std::string TStringToString( const std::basic_string<TCHAR>& in )
{
	const int size = WideCharToMultiByte( CP_UTF8, 0, in.data(), int(in.length()), NULL, 0, NULL, NULL );
	
	if ( 0 == size )
		return "";

	char* const data = new char[size];
	WideCharToMultiByte( CP_UTF8, 0, in.data(), int(in.length()), data, size, NULL, NULL );
	const std::string out( data, size );
	delete[] data;
	return out;
}

//BOOL CALLBACK DIEnumEffectsCallback( LPCDIEFFECTINFO pdei, LPVOID pvRef )
//{
//	((std::vector<DIEFFECTINFO>*)pvRef)->push_back( *pdei );
//	return DIENUM_CONTINUE;
//}

BOOL CALLBACK DIEnumDeviceObjectsCallback( LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef )
{
	((std::vector<DIDEVICEOBJECTINSTANCE>*)pvRef)->push_back( *lpddoi );
	return DIENUM_CONTINUE;
}

BOOL CALLBACK DIEnumDevicesCallback( LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef )
{
	((std::vector<DIDEVICEINSTANCE>*)pvRef)->push_back( *lpddi );
	return DIENUM_CONTINUE;
}

void InitJoystick( IDirectInput8* const idi8, std::vector<ControllerInterface::Device*>& devices/*, HWND hwnd*/ )
{
	std::vector<DIDEVICEINSTANCE> joysticks;
	idi8->EnumDevices( DI8DEVCLASS_GAMECTRL, DIEnumDevicesCallback, (LPVOID)&joysticks, DIEDFL_ATTACHEDONLY );

	// this is used to number the joysticks
	// multiple joysticks with the same name shall get unique ids starting at 0
	std::map< std::basic_string<TCHAR>, int>	name_counts;

#ifdef NO_DUPLICATE_DINPUT_XINPUT
	std::vector<DWORD> xinput_guids;
	GetXInputGUIDS( xinput_guids );
#endif

	std::vector<DIDEVICEINSTANCE>::iterator i = joysticks.begin(),
		e = joysticks.end();
	for ( ; i!=e; ++i )
	{
#ifdef NO_DUPLICATE_DINPUT_XINPUT
		// skip XInput Devices
		if ( std::find( xinput_guids.begin(), xinput_guids.end(), i->guidProduct.Data1 ) != xinput_guids.end() )
			continue;
#endif
		LPDIRECTINPUTDEVICE8 js_device;
		if (SUCCEEDED(idi8->CreateDevice(i->guidInstance, &js_device, NULL)))
		{
			if (SUCCEEDED(js_device->SetDataFormat(&c_dfDIJoystick)))
			{
				// using foregroundwindow seems like a hack
				if (FAILED(js_device->SetCooperativeLevel(GetForegroundWindow(), DISCL_BACKGROUND | DISCL_EXCLUSIVE)))
				{
					//PanicAlert("SetCooperativeLevel(DISCL_EXCLUSIVE) failed!");
					// fall back to non-exclusive mode, with no rumble
					if (FAILED(js_device->SetCooperativeLevel(NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
					{
						//PanicAlert("SetCooperativeLevel failed!");
						js_device->Release();
						continue;
					}
				}

				Joystick* js = new Joystick(/*&*i, */js_device, name_counts[i->tszInstanceName]++);
				// only add if it has some inputs/outpus
				if (js->Inputs().size() || js->Outputs().size())
					devices.push_back(js);
				else
					delete js;
			}
			else
			{
				//PanicAlert("SetDataFormat failed!");
				js_device->Release();
			}
		}
		
	}
}

Joystick::Joystick( /*const LPCDIDEVICEINSTANCE lpddi, */const LPDIRECTINPUTDEVICE8 device, const unsigned int index )
	: m_device(device)
	, m_index(index)
	//, m_name(TStringToString(lpddi->tszInstanceName))
{
	// get joystick caps
	DIDEVCAPS js_caps;
	ZeroMemory( &js_caps, sizeof(js_caps) );
	js_caps.dwSize = sizeof(js_caps);
	m_device->GetCapabilities(&js_caps);

	// max of 32 buttons and 4 hats / the limit of the data format i am using
	js_caps.dwButtons = std::min((DWORD)32, js_caps.dwButtons);
	js_caps.dwPOVs = std::min((DWORD)4, js_caps.dwPOVs);

	// polled or buffered data
	{
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = DATA_BUFFER_SIZE;

    // set the buffer size,
	// if we can't set the property, we can't use buffered data,
	// must use polling, which apparently doesn't work as well
	m_must_poll = (DI_OK != m_device->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph));
	}

	m_device->Acquire();

	// buttons
	for ( unsigned int i = 0; i < js_caps.dwButtons; ++i )
		inputs.push_back( new Button( i ) );
	// hats
	for ( unsigned int i = 0; i < js_caps.dwPOVs; ++i )
	{
		// each hat gets 4 input instances associated with it, (up down left right)
		for ( unsigned int d = 0; d<4; ++d )
			inputs.push_back( new Hat( i, d ) );
	}
	// get up to 6 axes and 2 sliders
	std::vector<DIDEVICEOBJECTINSTANCE> axes;
	m_device->EnumObjects( DIEnumDeviceObjectsCallback, (LPVOID)&axes, DIDFT_ABSAXIS );

	unsigned int cur_slider = 0;

	// map of axis offsets in joystate dataformat based on axis guidType
	std::map<GUID,int> types;
	types[GUID_XAxis] = 0;
	types[GUID_YAxis] = 1;
	types[GUID_ZAxis] = 2;
	types[GUID_RxAxis] = 3;
	types[GUID_RyAxis] = 4;
	types[GUID_RzAxis] = 5;

	// going in reverse leaves the list more organized in the end for me :/
	std::vector<DIDEVICEOBJECTINSTANCE>::const_reverse_iterator i = axes.rbegin(),
		e = axes.rend();
	for( ; i!=e; ++i )
	{
		DIPROPRANGE range;
		ZeroMemory( &range, sizeof(range ) );
		range.diph.dwSize = sizeof(range);
		range.diph.dwHeaderSize = sizeof(range.diph);
		range.diph.dwHow = DIPH_BYID;
		range.diph.dwObj = i->dwType;
		// try to set some nice power of 2 values (8192)
		range.lMin = -(1<<13);
		range.lMax = (1<<13);
		// but i guess not all devices support setting range
		m_device->SetProperty( DIPROP_RANGE, &range.diph );
		// so i getproperty right afterward incase it didn't set :P
		if (SUCCEEDED(m_device->GetProperty( DIPROP_RANGE, &range.diph)))
		{
			int offset = -1;

			if (GUID_Slider ==i->guidType)
			{
				// max of 2 sliders / limit of used data format
				if (cur_slider < 2)
					offset = 6 + cur_slider++;
			}
			else
			{
				// don't add duplicate axes, some buggy drivers report the same axis twice
				const std::map<GUID,int>::iterator f = types.find(i->guidType);
				if (types.end() != f)
				{
					offset = f->second;
					// remove from the map so it isn't added again
					types.erase(f);
				}
			}


			if ( offset >= 0 )
			{
				const LONG base = (range.lMin + range.lMax) / 2;
				// each axis gets a negative and a positive input instance associated with it
				inputs.push_back( new Axis( offset, base, range.lMin-base ) );
				inputs.push_back( new Axis( offset, base, range.lMax-base ) );
			}
		}
	}

	// get supported ff effects
	std::vector<DIDEVICEOBJECTINSTANCE> objects;
	m_device->EnumObjects( DIEnumDeviceObjectsCallback, (LPVOID)&objects, DIDFT_AXIS );
	// got some ff axes or something
	if ( objects.size() )
	{
		// temporary
		DWORD rgdwAxes[] = { DIJOFS_X, DIJOFS_Y };
		LONG rglDirection[] = { 0, 0 };
		DICONSTANTFORCE cf = { 0 };
		DIEFFECT eff;
		ZeroMemory(&eff, sizeof(DIEFFECT));
		eff.dwSize = sizeof(DIEFFECT);
		eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
		eff.dwDuration = INFINITE;
		eff.dwGain = DI_FFNOMINALMAX;
		eff.dwTriggerButton = DIEB_NOTRIGGER;
		eff.cAxes = std::min( (DWORD)2, (DWORD)objects.size() );
		eff.rgdwAxes = rgdwAxes;
		eff.rglDirection = rglDirection;
		eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
		eff.lpvTypeSpecificParams = &cf;

		LPDIRECTINPUTEFFECT pEffect;
		if (SUCCEEDED(m_device->CreateEffect(GUID_ConstantForce, &eff, &pEffect, NULL)))
		{
			// temp
			outputs.push_back( new Force( 0 ) );
			m_state_out.push_back( EffectState( pEffect ) );
		}
	}

	// disable autocentering
	if ( outputs.size() )
	{
		DIPROPDWORD dipdw;
		dipdw.diph.dwSize = sizeof( DIPROPDWORD );
		dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
		dipdw.diph.dwObj = 0;
		dipdw.diph.dwHow = DIPH_DEVICE;
		dipdw.dwData = DIPROPAUTOCENTER_OFF;
		m_device->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );
	}

	ClearInputState();
}

Joystick::~Joystick()
{
	// release the ff effect iface's
	std::vector<EffectState>::iterator i = m_state_out.begin(),
		e = m_state_out.end();
	for ( ; i!=e; ++i )
	{
		i->iface->Stop();
		i->iface->Unload();
		i->iface->Release();
	}

	m_device->Unacquire();
	m_device->Release();
}

void Joystick::ClearInputState()
{
	ZeroMemory(&m_state_in, sizeof(m_state_in));
	// set hats to center
	memset( m_state_in.rgdwPOV, 0xFF, sizeof(m_state_in.rgdwPOV) );
}

std::string Joystick::GetName() const
{
	DIPROPSTRING str;
	ZeroMemory( &str, sizeof(str) );
	str.diph.dwSize = sizeof(str);
	str.diph.dwHeaderSize = sizeof(str.diph);
	str.diph.dwHow = DIPH_DEVICE;
	m_device->GetProperty( DIPROP_PRODUCTNAME, &str.diph );
	return StripSpaces(TStringToString(str.wsz));
	//return m_name;
}

int Joystick::GetId() const
{
	return m_index;
}

std::string Joystick::GetSource() const
{
	return "DirectInput";
}

// update IO

bool Joystick::UpdateInput()
{
	HRESULT hr = 0;

	if (m_must_poll)
	{
		m_device->Poll();
		hr = m_device->GetDeviceState(sizeof(m_state_in), &m_state_in);
	}
	else
	{
		DIDEVICEOBJECTDATA evtbuf[DATA_BUFFER_SIZE];
		DWORD numevents;

GETDEVDATA :
		numevents = DATA_BUFFER_SIZE;
		hr = m_device->GetDeviceData(sizeof(*evtbuf), evtbuf, &numevents, 0);

		if (SUCCEEDED(hr))
		{
			for (LPDIDEVICEOBJECTDATA evt = evtbuf; evt != (evtbuf + numevents); ++evt)
			{
				// all the buttons are at the end of the data format
				// they are bytes rather than longs
				if (evt->dwOfs < DIJOFS_BUTTON(0))
					*(DWORD*)(((BYTE*)&m_state_in) + evt->dwOfs) = evt->dwData;
				else
					((BYTE*)&m_state_in)[evt->dwOfs] = (BYTE)evt->dwData;
			}

			// if there is more data to be received
			if (DI_BUFFEROVERFLOW == hr)
				goto GETDEVDATA;
		}
	}

	// try reacquire if input lost
	if (DIERR_INPUTLOST == hr || DIERR_NOTACQUIRED == hr)
		hr = m_device->Acquire();

	return SUCCEEDED(hr);
}

bool Joystick::UpdateOutput()
{
	// temporary
	size_t ok_count = 0;
	std::vector<EffectState>::iterator i = m_state_out.begin(),
		e = m_state_out.end();
	for ( ; i!=e; ++i )
	{
		if ( i->changed )
		{
			i->changed = false;
			DICONSTANTFORCE cf;
			cf.lMagnitude = LONG(10000 * i->magnitude);

			if ( cf.lMagnitude )
			{
				DIEFFECT eff;
				ZeroMemory( &eff, sizeof( eff ) );
				eff.dwSize = sizeof( DIEFFECT );
				eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
				eff.cbTypeSpecificParams = sizeof( cf );
				eff.lpvTypeSpecificParams = &cf;
				// set params and start effect
				ok_count += SUCCEEDED(i->iface->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START));
			}
			else
				ok_count += SUCCEEDED(i->iface->Stop());
		}
		else
			++ok_count;
	}

	return ( m_state_out.size() == ok_count );
}

// get name

std::string Joystick::Button::GetName() const
{
	std::ostringstream ss;
	ss << "Button " << m_index;
	return ss.str();
}

std::string Joystick::Axis::GetName() const
{
	std::ostringstream ss;
	// axis
	if ( m_index < 6 )
	{
		ss << "Axis " << "XYZ"[m_index%3];
		if ( m_index > 2 )
			ss << 'r';
	}
	// slider
	else
		ss << "Slider " << m_index-6;

	ss << ( m_range>0 ? '+' : '-' );
	return ss.str();
}

std::string Joystick::Hat::GetName() const
{
	std::ostringstream ss;
	ss << "Hat " << m_index << ' ' << "NESW"[m_direction];
	return ss.str();
}

std::string Joystick::Force::GetName() const
{
	// temporary
	return "Constant";
}

// get / set state

ControlState Joystick::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState( &m_state_in );
}

void Joystick::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
{
	((Output*)output)->SetState( state, &m_state_out[0] );
}

// get / set state

ControlState Joystick::Axis::GetState( const DIJOYSTATE* const joystate )
{
	return std::max( 0.0f, ControlState((&joystate->lX)[m_index]-m_base) / m_range );
}

ControlState Joystick::Button::GetState( const DIJOYSTATE* const joystate )
{
	return ControlState( joystate->rgbButtons[m_index] > 0 );
}

ControlState Joystick::Hat::GetState( const DIJOYSTATE* const joystate )
{
	// can this func be simplified ?
	const DWORD val = joystate->rgdwPOV[m_index];
	// hat centered code from msdn
	if ( 0xFFFF == LOWORD(val) )
		return 0;
	return ( abs( (int)(val/4500-m_direction*2+8)%8 - 4) > 2 );
}

void Joystick::Force::SetState( const ControlState state, Joystick::EffectState* const joystate )
{
	joystate[m_index].magnitude = state;
	joystate[m_index].changed = true;
}

}
}

#endif
