
#include "Common.h"

#include "Wiimote.h"
#include "WiimoteReal/WiimoteReal.h"
#include "WiimoteEmu/WiimoteEmu.h"
#include "Movie.h"

#include "ControllerInterface/ControllerInterface.h"

#include "../../InputCommon/Src/InputConfig.h"

namespace Wiimote
{

static InputPlugin g_plugin(WIIMOTE_INI_NAME, _trans("Wiimote"), "Wiimote");
InputPlugin *GetPlugin()
{
	return &g_plugin;
}

void Shutdown()
{
	std::vector<ControllerEmu*>::const_iterator
		i = g_plugin.controllers.begin(),
		e = g_plugin.controllers.end();
	for ( ; i!=e; ++i )
		delete *i;
	g_plugin.controllers.clear();

	// WiimoteReal is shutdown on app exit
	//WiimoteReal::Shutdown();

	g_controller_interface.Shutdown();
}

// if plugin isn't initialized, init and load config
void Initialize(void* const hwnd)
{
	// add 4 wiimotes
	for (unsigned int i = 0; i<4; ++i)
		g_plugin.controllers.push_back(new WiimoteEmu::Wiimote(i));

	g_controller_interface.SetHwnd(hwnd);
	g_controller_interface.Initialize();

	g_plugin.LoadConfig();

	WiimoteReal::Initialize();
	
	// reload Wiimotes with our settings
	if (Movie::IsPlayingInput() || Movie::IsRecordingInput())
		Movie::ChangeWiiPads();
}

// __________________________________________________________________________________________________
// Function: Wiimote_Output
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID CONTROL channel.
// input:    Da pakket.
// output:   none
//
void ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	if (WIIMOTE_SRC_EMU & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[_number])->ControlChannel(_channelID, _pData, _Size);

	if (WIIMOTE_SRC_REAL & g_wiimote_sources[_number])
		WiimoteReal::ControlChannel(_number, _channelID, _pData, _Size);
}

// __________________________________________________________________________________________________
// Function: Wiimote_InterruptChannel
// Purpose:  An L2CAP packet is passed from the Core to the Wiimote,
//           on the HID INTERRUPT channel.
// input:    Da pakket.
// output:   none
//
void InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size)
{
	if (WIIMOTE_SRC_EMU & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[_number])->InterruptChannel(_channelID, _pData, _Size);
	else
		WiimoteReal::InterruptChannel(_number, _channelID, _pData, _Size);
}

// __________________________________________________________________________________________________
// Function: Wiimote_Update
// Purpose:  This function is called periodically by the Core.
// input:    none
// output:   none
//
void Update(int _number)
{
	//PanicAlert( "Wiimote_Update" );

	// TODO: change this to a try_to_lock, and make it give empty input on failure
	std::lock_guard<std::recursive_mutex> lk(g_plugin.controls_lock);

	static int _last_number = 4;
	if (_number <= _last_number)
	{
		g_controller_interface.UpdateOutput();
		g_controller_interface.UpdateInput();
	}
	_last_number = _number;

	if (WIIMOTE_SRC_EMU & g_wiimote_sources[_number])
		((WiimoteEmu::Wiimote*)g_plugin.controllers[_number])->Update();
	else
		WiimoteReal::Update(_number);
}

// __________________________________________________________________________________________________
// Function: PAD_GetAttachedPads
// Purpose:  Get mask of attached pads (eg: controller 1 & 4 -> 0x9)
// input:	 none
// output:   number of pads
//
unsigned int GetAttached()
{
	unsigned int attached = 0;
	for (unsigned int i=0; i<4; ++i)
		if (g_wiimote_sources[i])
			attached |= (1 << i);
	return attached;
}

// ___________________________________________________________________________
// Function: DoState
// Purpose:  Saves/load state
// input/output: ptr
// input: mode
//
void DoState(unsigned char **ptr, int mode)
{
	// TODO:

	PointerWrap p(ptr, mode);
	for (unsigned int i=0; i<4; ++i)
		((WiimoteEmu::Wiimote*)g_plugin.controllers[i])->DoState(p);
}

// ___________________________________________________________________________
// Function: EmuStateChange
// Purpose: Notifies the plugin of a change in emulation state
// input:    newState
// output:   none
//
void EmuStateChange(EMUSTATE_CHANGE newState)
{
	// TODO
	WiimoteReal::StateChange(newState);
}

}
