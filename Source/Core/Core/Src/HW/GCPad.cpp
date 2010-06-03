#include <ControllerInterface/ControllerInterface.h>
#include "GCPadEmu.h"
#include <InputConfig.h>
#include "../ConfigManager.h"

/*staticTODOSHUFFLE*/ Plugin g_GCPad( "GCPad", "Pad", "GCPad" );

void PAD_Init()
{
	// i realize i am checking IsInit() twice, just too lazy to change it
	if ( false == g_GCPad.controller_interface.IsInit() )
	{
		// add 4 gcpads
		for ( unsigned int i = 0; i<4; ++i )
			g_GCPad.controllers.push_back( new GCPad( i ) );

		// load the saved controller config
		g_GCPad.LoadConfig();

		// needed for Xlib and exclusive dinput
		g_GCPad.controller_interface.SetHwnd( SConfig::GetInstance().m_LocalCoreStartupParameter.hMainWindow );
		g_GCPad.controller_interface.Init();

		// update control refs
		std::vector<ControllerEmu*>::const_iterator i = g_GCPad.controllers.begin(),
			e = g_GCPad.controllers.end();
		for ( ; i!=e; ++i )
			(*i)->UpdateReferences( g_GCPad.controller_interface );

	}
}

void PAD_Shutdown()
{
	if ( g_GCPad.controller_interface.IsInit() )
	{
		std::vector<ControllerEmu*>::const_iterator
			i = g_GCPad.controllers.begin(),
			e = g_GCPad.controllers.end();
		for ( ; i!=e; ++i )
			delete *i;
		g_GCPad.controllers.clear();

		g_GCPad.controller_interface.DeInit();
	}
}

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	memset( _pPADStatus, 0, sizeof(*_pPADStatus) );
	_pPADStatus->err = PAD_ERR_NONE;
	// wtf is this?	
	_pPADStatus->button |= PAD_USE_ORIGIN;

	// try lock
	if ( false == g_GCPad.controls_crit.TryEnter() )
	{
		// if gui has lock (messing with controls), skip this input cycle
		// center axes and return
		memset( &_pPADStatus->stickX, 0x80, 4 );
		return;
	}

	// if we are on the next input cycle, update output and input
	// if we can get a lock
	static int _last_numPAD = 4;
	if ( _numPAD <= _last_numPAD && g_GCPad.interface_crit.TryEnter() )
	{
		g_GCPad.controller_interface.UpdateOutput();
		g_GCPad.controller_interface.UpdateInput();
		g_GCPad.interface_crit.Leave();
	}
	_last_numPAD = _numPAD;

	// get input
	((GCPad*)g_GCPad.controllers[ _numPAD ])->GetInput( _pPADStatus );

	// leave
	g_GCPad.controls_crit.Leave();

}

void PAD_Input(u16 _Key, u8 _UpDown)
{
	// nofin
}

void PAD_Rumble(u8 _numPAD, u8 _uType, u8 _uStrength)
{
	// enter
	if ( g_GCPad.controls_crit.TryEnter() )
	{
		// TODO: this has potential to not stop rumble if user is messing with GUI at the perfect time
		// set rumble
		((GCPad*)g_GCPad.controllers[ _numPAD ])->SetOutput( 1 == _uType && _uStrength > 2 );

		// leave
		g_GCPad.controls_crit.Leave();
	}
}
