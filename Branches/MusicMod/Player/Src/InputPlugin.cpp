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

// =======================================================================================
#include "Global.h"
// =======================================================================================

#include "InputPlugin.h"
#include "Console.h"
#include "Main.h"
#include "Input.h"
#include "Unicode.h"



map <TCHAR *, InputPlugin *, TextCompare> ext_map; // extern 
vector <InputPlugin *> input_plugins; // extern 
InputPlugin * active_input_plugin = NULL; // extern



// =======================================================================================
// The InputPlugin class is inherited from the Plugin class
InputPlugin::InputPlugin( TCHAR * szDllpath, bool bKeepLoaded ) : Plugin( szDllpath )
{
	iHookerIndex   = -1;

	szFilters      = NULL;
	iFiltersLen    = 0;
	plugin         = NULL;
	
	wprintf("\InputPlugin::InputPlugin > Begin\n");

	if( !Load() )
	{
		return;
	}

	if( !bKeepLoaded )
	{
		Unload();
	}
	
	input_plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
InputPlugin::~InputPlugin()
{
	if( szFilters ) delete [] szFilters;
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::Load()
{
	printf("InputPlugin::Load() was called. IsLoaded: %i\n", IsLoaded());

	if( IsLoaded() ) return true;


	// (1) Load DLL
	hDLL = LoadLibrary( GetFullpath() );
	if( !hDLL ) return false;

	// (2) Find export
	WINAMP_INPUT_GETTER winampGetInModule2 =
		( WINAMP_INPUT_GETTER )GetProcAddress( hDLL, "winampGetInModule2" );
	if( winampGetInModule2 == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (3) Get module
	plugin = winampGetInModule2();
	if( plugin == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (4) Process module
	plugin->hDllInstance	= hDLL;
	plugin->hMainWindow		= WindowMain;

	plugin->SAVSAInit		= SAVSAInit;
	plugin->SAVSADeInit		= SAVSADeInit;
	plugin->SAAddPCMData	= SAAddPCMData;
	plugin->SAGetMode		= SAGetMode;
	plugin->SAAdd			= SAAdd;
	plugin->VSAAddPCMData	= VSAAddPCMData;
	plugin->VSAGetMode		= VSAGetMode;
	plugin->VSAAdd			= VSAAdd;
	plugin->VSASetInfo		= VSASetInfo;

	plugin->dsp_dosamples	= dsp_dosamples;
	plugin->dsp_isactive	= dsp_isactive;

	plugin->SetInfo			= SetInfo;


	if( !szName )
	{
		// Note:  The prefix is not removed to hide their
		//        origin at Nullsoft! It just reads easier.
		if( !strnicmp( plugin->description, "nullsoft ", 9 ) )
		{
			plugin->description += 9;
			if( !strnicmp( plugin->description, "mpeg(layer1-3/ct aac+/dolby aac) ", 33 ) )
			{
				plugin->description += ( 33 - 5 );
				memcpy( plugin->description, "MPEG", 4 * sizeof( char ) );
			}
		}
		iNameLen = ( int )strlen( plugin->description );
		szName = new TCHAR[ iNameLen + 1 ];
		ToTchar( szName, plugin->description, iNameLen );
		szName[ iNameLen ] = TEXT( '\0' );
	}


	
	// (5) Init
	const WNDPROC WndprocBefore  = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
		plugin->Init();
	const WNDPROC WndprocAfter   = ( WNDPROC )GetWindowLong( WindowMain, GWL_WNDPROC );
	
	if( WndprocBefore != WndprocAfter )
	{
		WndprocBackup  = WndprocBefore;
		iHookerIndex   = iWndprocHookCounter++;
	}
	

#ifdef NOGUI
	printf( "Loading <%s>, %s\n" , GetFilename(), szName );
#else
	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Loading <%s>, %s" ), GetFilename(), szName );
	Console::Append( szBuffer );
#endif
	
	Integrate(); // This function is just below
	

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
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::Integrate()
{
	if( !IsLoaded() ) return false;

	// (6) Build filter

	// (6a) First pass: get needed buffer length
	int needed = 0;
	int len = 0;
	bool even = false;
	char * walk = plugin->FileExtensions;
	while( ( len = ( int )strlen( walk ) ) > 0 )
	{
		len++; // For '\0'
		if( even )
		{
			// Extensions e.g. "mp3;mp2;mp1"
			// Worst case "a;b;c" (5) --> "*.a;*.b;*.c" (11)
			needed += ( 3 * len );
		}
		else
		{
			// Filter name e.g. "MPEG audio files"
			needed += len;
		}
		even = !even;
		walk += len;
	}
	szFilters = new TCHAR[ needed + 1 ];
	TCHAR * walk_out = szFilters;

	// (6b) Once again with copy
	walk = plugin->FileExtensions;
	even = false;
	while( true )
	{
		// Check extensions
		char * start_filter    = walk;
		const int len_filter  = ( int )strlen( walk );
		if( len_filter == 0 )
		{
			// End reached
			break;
		}
		walk += len_filter + 1;

		// Check filter name
		char * start_display = walk;
		int len_display = ( int )strlen( walk );
		if( len_display == 0 )
		{
			break;
		}
		walk += ++len_display;
			
		// Append filter name
		ToTchar( walk_out, start_display, len_display );

// =======================================================================================
// Print used filetypes
		TCHAR szBuffer[ 5000 ];
		*(walk_out + len_display) = TEXT( '\0' );
		_stprintf( szBuffer, TEXT( "   %s" ), walk_out );
		Console::Append( szBuffer );
		//printf( szBuffer, TEXT( "   %s\n" ), walk_out );
// =======================================================================================
		walk_out += len_display;

		// Convert and append extensions
		char * walk_filter = start_filter;
		char * last_filter = start_filter;
		int len;
		while( true )
		{
			if( ( *walk_filter == ';' ) || ( *walk_filter == '\0' ) )
			{
				len = ( walk_filter - last_filter );
				if( len < 1 )
				{
					break;	
				}
				
				// Add extension to map
				TCHAR * szExt = new TCHAR[ len + 1 ];
				ToTchar( szExt, last_filter, len );
				szExt[ len ] = TEXT( '\0' );
				_tcslwr( szExt );

				ext_map.insert( pair<TCHAR *, InputPlugin *>( szExt, this ) );

				// Append extension as "*.ext[;\0]"
				len++; // Also copy ';' and '\0'
				memcpy( walk_out, TEXT( "*." ), 2 * sizeof( TCHAR ) );
				walk_out += 2;
				ToTchar( walk_out, last_filter, len );
				walk_out += len;
				
				// Any more extensions?
				if( *walk_filter == '\0' )
				{
					break;
				}
				last_filter = walk_filter + 1;
			}
			walk_filter++;
		}
		
		if( *walk == '\0' )
		{
			*walk_out = TEXT( '\0' );
			iFiltersLen = walk_out - szFilters;

			break;
		}
	}

	Console::Append( TEXT( " " ) );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::DisIntegrate()
{
	return true;	
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::Unload()
{
	if( !IsLoaded() ) return true;


	DisIntegrate();


	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Unloading <%s>" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	printf( ">>>Unloading <%s>\n" , GetFilename() );

	// Quit
	if( plugin )
	{
		if( plugin->Quit ) plugin->Quit();
		plugin->outMod = NULL;
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
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::About( HWND hParent )
{
	if( !plugin ) return false;
	if( !plugin->About ) return false;
	
	plugin->About( hParent );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
bool InputPlugin::Config( HWND hParent )
{
	if( !plugin ) return false;
	if( !plugin->Config ) return false;
	
	plugin->Config( hParent );
	
	// TODO:  File extension could have changed (e.g. in_mp3)
	//        So we have to process ext_map here...
	
	return true;
}
