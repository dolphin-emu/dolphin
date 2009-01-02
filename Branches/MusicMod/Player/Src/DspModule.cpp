////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#include "DspModule.h"
#include "Unicode.h"



DspModule ** active_dsp_mods = NULL; // extern
int active_dsp_count = 0; // extern



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
DspModule::DspModule( char * szName, int iIndex, winampDSPModule * mod, DspPlugin * plugin )
{
	iArrayIndex = -1;

	iNameLen = ( int )strlen( szName );
	this->szName = new TCHAR[ iNameLen + 1 ];
	ToTchar( this->szName, szName, iNameLen );
	this->szName[ iNameLen ] = TEXT( '\0' );

	this->iIndex   = iIndex;
	this->mod      = mod;
	this->plugin   = plugin;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspModule::Start( int iIndex )
{
	if( !mod ) return false;
	if( iArrayIndex != -1 ) return false;
	if( !mod->Init ) return false;
	if( mod->Init( mod ) != 0 ) return false;

////////////////////////////////////////////////////////////////////////////////
	DspLock.Enter();
////////////////////////////////////////////////////////////////////////////////

	if( !active_dsp_count )
	{
		active_dsp_mods = new DspModule * [ 1 ];
		active_dsp_mods[ 0 ] = this;
		iArrayIndex = 0;
	}
	else
	{
		if( iIndex < 0 )
			iIndex = 0;
		else if( iIndex > active_dsp_count )
			iIndex = active_dsp_count;
		
		DspModule ** new_active_dsp_mods = new DspModule * [ active_dsp_count + 1 ];
		memcpy( new_active_dsp_mods, active_dsp_mods, iIndex * sizeof( DspModule * ) );
		memcpy( new_active_dsp_mods + iIndex + 1, active_dsp_mods + iIndex, ( active_dsp_count - iIndex ) * sizeof( DspModule * ) );
		for( int i = iIndex + 1; i < active_dsp_count + 1; i++ )
		{
			new_active_dsp_mods[ i ]->iArrayIndex = i;
		}
		new_active_dsp_mods[ iIndex ] = this;
		iArrayIndex = iIndex;
		delete [] active_dsp_mods;
		active_dsp_mods = new_active_dsp_mods;
	}
	active_dsp_count++;

////////////////////////////////////////////////////////////////////////////////
	DspLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspModule::Stop()
{
	if( !mod ) return false;
	if( iArrayIndex == -1 ) return false;
	if( !mod->Quit ) return true;
	
////////////////////////////////////////////////////////////////////////////////
	DspLock.Enter();
////////////////////////////////////////////////////////////////////////////////

	for( int i = iArrayIndex; i < active_dsp_count - 1; i++ )
	{
		active_dsp_mods[ i ] = active_dsp_mods[ i + 1 ];
		active_dsp_mods[ i ]->iArrayIndex = i;
	}
	active_dsp_count--;

////////////////////////////////////////////////////////////////////////////////
	DspLock.Leave();
////////////////////////////////////////////////////////////////////////////////
	
	mod->Quit( mod );

	iArrayIndex = -1;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspModule::Config()
{
	if( !mod ) return false;
	if( iArrayIndex == -1 ) return false;
	if( !mod->Config ) return false;
	
	mod->Config( mod );
	
	return true;	
}
