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


#include "OutputPlugin.h"
#include "Main.h"
#include "Unicode.h"
#include "Console.h"
#include "Config.h"
#include "Playback.h"



vector <OutputPlugin *> output_plugins; // extern
OutputPlugin ** active_output_plugins = NULL; // extern
int active_output_count = 0; // extern



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
OutputPlugin::OutputPlugin( TCHAR * szDllpath, bool bKeepLoaded ) : Plugin( szDllpath )
{
	iHookerIndex  = -1;

	bActive       = false;
	iArrayIndex   = -1;
	plugin        = NULL;
	
	if( !Load() ) 
	{
		return;
	}

	////////////////////////////////////////////////////////////////////////////////

	// Quick hack!!!
	TCHAR * szBuffer = new TCHAR[ 500 ]; // NOT LOCAL!!!
	_stprintf( szBuffer, TEXT( "OutputPluginActive___%s" ), GetFilename() );
	ConfBool * cbActive = new ConfBool( &bActive, szBuffer, CONF_MODE_INTERNAL, false );
	cbActive->Read();
	printf("OutputPlugin > GetFilename() returned <%s>\n", szBuffer );

	printf("OutputPlugin > We now have <bActive:%i> and <bKeepLoaded:%i>\n", bActive, bKeepLoaded );

	if( bActive )
	{
		bActive = false;
		Start();
	}
	else
	{
		if( !bKeepLoaded )
		{
			// Note:  out_ds seems to do weird things
			//        when unloaded here!?
			//        So out_ds keeps loaded for now.
			if( _tcscmp( GetFilename(), TEXT( "out_ds.dll" ) ) )
			{
				printf("OutputPlugin > Unload called from OutputPlugin::OutputPlugin\n");
				Unload();
			}
		}
	}
	// Quick hack!!!
	////////////////////////////////////////////////////////////////////////////////
	
	
	output_plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::Load()
{
	if( IsLoaded() ) return true;

	// (1) Load DLL
	hDLL = LoadLibrary( GetFullpath() );
	if( !hDLL ) return false;

	// (2) Find export
	WINAMP_OUTPUT_GETTER winampGetOutModule =
		( WINAMP_OUTPUT_GETTER )GetProcAddress(	hDLL, "winampGetOutModule" );
	if( winampGetOutModule == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (3) Get module
	plugin = winampGetOutModule();
	if( !plugin )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (4) Process module
	plugin->hDllInstance  = hDLL;
	plugin->hMainWindow   = WindowMain;

	if( !szName )
	{
		// Note:  The prefix is not removed to hide their
		//        origin at Nullsoft! It just reads easier.
		if( !_strnicmp( plugin->description, "nullsoft ", 9 ) )
		{
			plugin->description += 9;
		}
		iNameLen = ( int )strlen( plugin->description );
		szName = new TCHAR[ iNameLen + 1 ];
		ToTchar( szName, plugin->description, iNameLen );
		szName[ iNameLen ] = TEXT( '\0' );
	}


	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Loading <%s>, %s" ), GetFilename(), szName );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	printf( ">>>Loading <%s>, %s\n" , GetFilename(), szName );
	
	if( plugin->Init )
	{
		// We remove the WNDPROC things
#ifdef NOGUI
	plugin->Init();
#else

		// Init
		const WNDPROC WndprocBefore = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
			plugin->Init();
		const WNDPROC WndprocAfter = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
		
		if( WndprocBefore != WndprocAfter )
		{
			WndprocBackup  = WndprocBefore;
			iHookerIndex   = iWndprocHookCounter++;
		}


		// Note:  Plugins that use a wndproc hook need
		//        to be unloaded in the inverse loading order.
		//        This is due to the nature of wndproc hooking.
		if( iHookerIndex != -1 )
		{
			Console::Append( TEXT( "Wndproc hook added (by plugin)" ) );
		}
#endif
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::Unload()
{
	if( !IsLoaded() ) return true;
	if( bActive && Playback::IsPlaying() ) return false;

	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Unloading <%s>" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	printf( ">>>Unloading <%s>\n" , GetFilename() );

	// Quit
	if( plugin )
	{
		if( plugin->Quit ) plugin->Quit();
		plugin = NULL;
	}
	
	// Remove wndproc hook
	if( ( iHookerIndex != -1 ) && ( iHookerIndex == iWndprocHookCounter - 1 ) )
	{
		// If we don't restore it the plugins wndproc will
		// still be called which is not there anymore -> crash
		SetWindowLong( WindowMain, GWL_WNDPROC, ( LONG )WndprocBackup );
		Console::Append( TEXT( "Wndproc hook removed (by host)" ) );
		Console::Append( TEXT( " " ) );

		iHookerIndex  = -1;
		iWndprocHookCounter--;
	}

	FreeLibrary( hDLL );
	hDLL = NULL;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::About( HWND hParent )
{
	if( !IsLoaded() ) return false;
	if( !plugin ) return false;
	if( !plugin->About ) return false;
	
	plugin->About( hParent );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::Config( HWND hParent )
{
	if( !IsLoaded() ) return false;
	if( !plugin ) return false;
	if( !plugin->Config ) return false;
	
	plugin->Config( hParent );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::Start()
{
	//NOTICE_LOG(AUDIO, "OutputPlugin::Start() > Begin <IsLoaded():%i> <bActive:%i> <active_output_count:%i>\n",
	//	IsLoaded(), bActive, active_output_count );

	if( !IsLoaded() ) return false;
	if( bActive ) return true;

	if( active_output_count )
	{
		active_output_plugins = ( OutputPlugin ** )realloc( active_output_plugins, ( active_output_count + 1 ) * sizeof( OutputPlugin * ) );
		active_output_plugins[ active_output_count ] = this;
		iArrayIndex = active_output_count;
		active_output_count++;
	}
	else
	{
		active_output_plugins = ( OutputPlugin ** )malloc( sizeof( OutputPlugin * ) );
		active_output_plugins[ 0 ] = this;
		iArrayIndex = 0;
		active_output_count = 1;
	}
	
	#ifndef NOGUI
		TCHAR szBuffer[ 5000 ];
		_stprintf( szBuffer, TEXT( "Output plugin <%s> activated" ), GetFilename() );
		Console::Append( szBuffer );
		Console::Append( TEXT( " " ) );
	#else
		NOTICE_LOG(AUDIO, "\n >>> Output plugin '%s' activated\n\n" , GetFilename() );
	#endif
	
	bActive = true;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool OutputPlugin::Stop()
{
	if( !IsLoaded() ) return true;
	if( !bActive ) return true;

	const int iMaxIndex = active_output_count - 1;
	if( iArrayIndex < iMaxIndex )
	{
		active_output_plugins[ iArrayIndex ] = active_output_plugins[ iMaxIndex ];
		active_output_plugins[ iArrayIndex ]->iArrayIndex = iArrayIndex;
	}
	iArrayIndex = -1;
	active_output_count--;
	
	// TODO Flush?
	// TODO Close?
	
	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Output plugin <%s> deactivated" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	
	bActive = false;

	return true;
}
