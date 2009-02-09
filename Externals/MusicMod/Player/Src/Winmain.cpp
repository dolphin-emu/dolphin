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


#include "Global.h"
#include "Font.h"
#include "InputPlugin.h"
#include "OutputPlugin.h"
#include "VisPlugin.h"
#include "DspPlugin.h"
#include "GenPlugin.h"
#include "Main.h"
#include "Rebar.h"
#include "Playlist.h"
#include "Status.h"
#include "PluginManager.h"
#include "Prefs.h"
#include "Config.h"

#include "Emabox/Emabox.h"



#define PLUS_ALT            ( FVIRTKEY | FALT )
#define PLUS_CONTROL        ( FVIRTKEY | FCONTROL )
#define PLUS_CONTROL_ALT    ( FVIRTKEY | FCONTROL | FALT )
#define PLUS_CONTROL_SHIFT  ( FVIRTKEY | FCONTROL | FSHIFT )
#define PLUS_SHIFT          ( FVIRTKEY | FSHIFT )



HINSTANCE g_hInstance = NULL; // extern

TCHAR * szHomeDir = NULL; // extern
int     iHomeDirLen = 0; // extern

TCHAR * szPluginDir = NULL; // extern
int     iPluginDirLen = 0; // extern



TCHAR szCurDir[ MAX_PATH + 1 ] = TEXT( "" );
ConfCurDir ccdCurDir( szCurDir, TEXT( "CurDir" ) );


bool bWarnPluginsMissing;
ConfBool cbWarnPluginsMissing( &bWarnPluginsMissing, TEXT( "WarnPluginsMissing" ), CONF_MODE_PUBLIC, true );



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow )
{
	g_hInstance = hInstance;
	

	// Load full config
	Conf::Init( hInstance );


	// Get home dir
	szHomeDir = new TCHAR[ MAX_PATH + 1 ];
	iHomeDirLen = GetModuleFileName( NULL, szHomeDir, MAX_PATH );
	if( !iHomeDirLen ) return 1;	

	TCHAR * walk = szHomeDir + iHomeDirLen - 1;
	while( ( walk > szHomeDir ) && ( *walk != TEXT( '\\' ) ) ) walk--;
	walk++;
	*walk = TEXT( '\0' );
	iHomeDirLen = walk - szHomeDir;

	
	// Get plugins dir
	szPluginDir = new TCHAR[ MAX_PATH + 1 ];
	memcpy( szPluginDir, szHomeDir, iHomeDirLen * sizeof( TCHAR ) );
	memcpy( szPluginDir + iHomeDirLen, TEXT( "Plugins" ), 7 * sizeof( TCHAR ) );
	szPluginDir[ iHomeDirLen + 7 ] = TEXT( '\0' );

	
	Font::Create();
	BuildMainWindow();
	Prefs::Create();


	// Show window
	ShowWindow( WindowMain, SW_SHOW );
	SetForegroundWindow( WindowMain );
	SetFocus( WindowMain );

	Plugin::FindAll<InputPlugin> ( szPluginDir, TEXT( "in_*.dll"  ), true  );
	Plugin::FindAll<OutputPlugin>( szPluginDir, TEXT( "out_*.dll" ), false );	
	Plugin::FindAll<VisPlugin>   ( szPluginDir, TEXT( "vis_*.dll" ), false );	
	Plugin::FindAll<DspPlugin>   ( szPluginDir, TEXT( "dsp_*.dll" ), false );	
	Plugin::FindAll<GenPlugin>   ( szPluginDir, TEXT( "gen_*.dll" ), true  );	

	PluginManager::Fill();


	// Check plugin presence
	// One msgbox maximum
	if( bWarnPluginsMissing )
	{
		
		
		if( input_plugins.empty() )
		{
			// No input plugins
			TCHAR szBuffer[ 5000 ];
			_stprintf(
				szBuffer,
				TEXT(
					"No input plugins were found.\n"
					"\n"
					"Please install at least one Winamp input plugin to  \n"
					"%s  "
				),
				szPluginDir
			);
			
			int iNeverAgain = bWarnPluginsMissing ? 0 : 1;
			EmaBox( 0, szBuffer, TEXT( "Input plugins missing" ), MB_ICONEXCLAMATION | MB_CHECKNEVERAGAIN, &iNeverAgain );
			bWarnPluginsMissing = ( iNeverAgain == 0 );
		}
		else if( output_plugins.empty() )
		{
			// No output plugins
			TCHAR szBuffer[ 5000 ];
			_stprintf(
				szBuffer,
				TEXT(
					"No output plugins were found.\n"
					"\n"
					"Please install at least one Winamp output plugin to  \n"
					"%s  "
				),
				szPluginDir
			);
			
			int iNeverAgain = bWarnPluginsMissing ? 0 : 1;
			EmaBox( 0, szBuffer, TEXT( "Output plugins missing" ), MB_ICONEXCLAMATION | MB_CHECKNEVERAGAIN, &iNeverAgain );
			bWarnPluginsMissing = ( iNeverAgain == 0 );
		}
	}


	// Todo: all the rest...
	ACCEL accels[] = {
		{ PLUS_CONTROL,        'A',        ID_PE_SELECTALL      },  // [Ctrl] + [A]
		{ PLUS_CONTROL,        'I',        ID_PE_INVERT         },  // [Ctrl] + [I]
		{ PLUS_CONTROL,        'N',        ID_PE_NEW            },  // [Ctrl] + [N]
		{ PLUS_CONTROL,        'O',        ID_PE_OPEN           },  // [Ctrl] + [O]
		{ PLUS_CONTROL,        'P',        WINAMP_OPTIONS_PREFS },  // [Ctrl] + [P]
		{ PLUS_CONTROL,        'S',        ID_PE_SAVEAS         },  // [Ctrl] + [S]
		{ PLUS_CONTROL,        VK_F1,      WINAMP_HELP_ABOUT    },  // [Ctrl] + [F1]
		{ PLUS_CONTROL_SHIFT,  'A',        ID_PE_NONE           },  // [Ctrl] + [Shift] + [A]
		{ PLUS_CONTROL_SHIFT,  VK_DELETE,  ID_PE_CLEAR          },  // [Ctrl] + [Shift] + [Del]
		{ PLUS_ALT,            'F',        WINAMP_MAINMENU      },  // [Alt] + [F]
		{ PLUS_ALT,            VK_F4,      WINAMP_FILE_QUIT     }   // [Alt] + [F4]
	};
	
	
	HACCEL hAccel = CreateAcceleratorTable( accels, sizeof( accels ) / sizeof( ACCEL ) );
	if( !hAccel )
	{
		MessageBox( 0, TEXT( "Accelerator table error" ), TEXT( "" ), 0 );
	}

	// Message loop
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
	{
		// Note: Keys without [Alt] or [Ctrl] not everywhere!
		if( ( ( msg.hwnd == WindowMain ) || IsChild( WindowMain, msg.hwnd ) ) &&
			TranslateAccelerator( WindowMain, hAccel, &msg ) )
        {
			// MessageBox( 0, TEXT( "Trans" ), TEXT( "" ), 0 );
		}

		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
	
	DestroyAcceleratorTable( hAccel );


	// Input
	vector <InputPlugin *>::iterator iter_input = input_plugins.begin();
	while( iter_input != input_plugins.end() )
	{
		( *iter_input )->Unload();
		iter_input++;
	}
	
	// Output
	vector <OutputPlugin *>::iterator iter_output = output_plugins.begin();
	while( iter_output != output_plugins.end() )
	{
		( *iter_output )->Unload();
		iter_output++;
	}

	// General
	vector <GenPlugin *>::iterator iter_gen = gen_plugins.begin();
	while( iter_gen != gen_plugins.end() )
	{
		( *iter_gen )->Unload();
		iter_gen++;
	}
	
	
	// TODO: create main::destroy
	// UnregisterClass( PA_CLASSNAME, g_hInstance );

	Prefs::Destroy();

	Font::Destroy();

/*	
	delete [] szPluginDir;
	delete [] szHomeDir;
*/	

	Conf::Write();

	return 0;
}
