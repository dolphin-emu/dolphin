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


#include "GenPlugin.h"
#include "Main.h"
#include "Unicode.h"
#include "Console.h"
#include <string.h>



vector <GenPlugin *> gen_plugins; // extern



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
GenPlugin::GenPlugin( TCHAR * szDllpath, bool bKeepLoaded ) : Plugin( szDllpath )
{
	iHookerIndex  = -1;
	plugin        = NULL;
	
	if( !Load() )
	{
		return;
	}

	gen_plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool GenPlugin::Load()
{
	if( IsLoaded() ) return true;

	// (1) Load DLL
	hDLL = LoadLibrary( GetFullpath() );
	if( !hDLL ) return false;

	// (2) Find export
	WINAMP_GEN_GETTER winampGetGeneralPurposePlugin =
		( WINAMP_GEN_GETTER )GetProcAddress(	hDLL, "winampGetGeneralPurposePlugin" );
	if( winampGetGeneralPurposePlugin == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (3) Get module
	plugin = winampGetGeneralPurposePlugin();
	if( !plugin )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (4) Process module
	plugin->hDllInstance  = hDLL;
	plugin->hwndParent    = WindowMain;

	// Note:  Some plugins (mainly old ones) set description in init.
	//        Therefore we init first and copy the name after.


	// (5) Init
	if( plugin->init )
	{
		const WNDPROC WndprocBefore = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
			plugin->init();
		const WNDPROC WndprocAfter = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
		
		if( WndprocBefore != WndprocAfter )
		{
			WndprocBackup  = WndprocBefore;
			iHookerIndex   = iWndprocHookCounter++;
		}
	}


	if( !szName )
	{
		// Note:  The prefix is not removed to hide their
		//        origin at Nullsoft! It just reads easier.
		if( !strnicmp( plugin->description, "nullsoft ", 9 ) )
		{
			plugin->description += 9;
		}
		
		// Get rid of " (xxx.dll)" postfix
		char * walk = plugin->description + strlen( plugin->description ) - 5;
		while( true )
		{
			if( ( walk <= plugin->description ) || strnicmp( walk, ".dll)", 5 ) ) break;
			while( ( walk > plugin->description ) && ( *walk != '(' ) ) walk--;
			if( walk <= plugin->description ) break;
			walk--;
			if( ( walk <= plugin->description ) || ( *walk != ' ' ) ) break;
			*walk = '\0';
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


	// Note:  Plugins that use a wndproc hook need
	//        to be unloaded in the inverse loading order.
	//        This is due to the nature of wndproc hooking.
	if( iHookerIndex != -1 )
	{
		Console::Append( TEXT( "Wndproc hook added (by plugin)" ) );
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool GenPlugin::Unload()
{
	if( !IsLoaded() ) return true;

	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Unloading <%s>" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	printf( ">>>Unloading <%s>\n" , GetFilename() );


	// Quit
	if( plugin )
	{
		if( plugin->quit ) plugin->quit();
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
bool GenPlugin::Config()
{
	if( !IsLoaded() ) return false;
	if( !plugin ) return false;
	if( !plugin->config ) return false;
	
	plugin->config();
	
	return true;
}
