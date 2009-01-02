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


#include "DspPlugin.h"
#include "Main.h"
#include "Unicode.h"
#include "Console.h"



vector <DspPlugin *> dsp_plugins; // extern
Lock DspLock = Lock( TEXT( "PLAINAMP_DSP_LOCK" ) ); // extern



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
DspPlugin::DspPlugin( TCHAR * szDllpath, bool bKeepLoaded ) : Plugin( szDllpath )
{
	header = NULL;
	
	if( !Load() )
	{
		return;
	}

	if( !bKeepLoaded )
	{
		Unload();
	}
	
	dsp_plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspPlugin::Load()
{
	if( IsLoaded() ) return true;

	// (1) Load DLL
	hDLL = LoadLibrary( GetFullpath() );
	if( !hDLL ) return false;

	// (2) Find export
	WINAMP_DSP_GETTER winampGetDSPHeader2 =
		( WINAMP_DSP_GETTER )GetProcAddress( hDLL, "winampDSPGetHeader2" );
	if( winampGetDSPHeader2 == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (3) Get header
	header = winampGetDSPHeader2();
	if( header == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}
	
////////////////////////////////////////////////////////////////////////////////
	// Forget old modules or we get them twice
	if( !modules.empty() )
	{
		modules.clear();
	}
////////////////////////////////////////////////////////////////////////////////
	
	if( !szName )
	{
		// Note:  The prefix is not removed to hide their
		//        origin at Nullsoft! It just reads easier.
		if( !strnicmp( header->description, "nullsoft ", 9 ) )
		{
			header->description += 9;
		}
		iNameLen = ( int )strlen( header->description );
		szName = new TCHAR[ iNameLen + 1 ];
		ToTchar( szName, header->description, iNameLen );
		szName[ iNameLen ] = TEXT( '\0' );
	}

	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Loading <%s>, %s" ), GetFilename(), szName );
	Console::Append( szBuffer );

	// (4) Get modules
	winampDSPModule * mod;
	int iFound = 0;
	while( true )
	{
		mod = header->getModule( iFound );
		if( !mod ) break;

		// (4a) Modify module
		mod->hDllInstance  = hDLL;
		mod->hwndParent    = WindowMain;
		
		// (4b) Add module to list
		DspModule * dspmod = new DspModule(
			mod->description,  // char * szName
			iFound,            // UINT uIndex
			mod,               // winampDspModule * mod
			this               // DspPlugin * plugin
		);
		modules.push_back( dspmod );
		iFound++;

		_stprintf( szBuffer, TEXT( "   %s" ), dspmod->GetName() );
		Console::Append( szBuffer );
	}

	Console::Append( TEXT( " " ) );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspPlugin::Unload()
{
	if( !IsLoaded() ) return true;
	if( IsActive() ) return false;

	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Unloading <%s>" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );

	header = NULL;

	/*
	TODO
	DspModule * walk;
	vector <DspModule *>::iterator iter = modules.begin();
	while( iter != modules.end() )
	{
		walk = *iter;
		delete [] walk->szName;
		delete walk;
		
		iter++;	
	}
	*/

	FreeLibrary( hDLL );
	hDLL = NULL;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool DspPlugin::IsActive()
{
	vector <DspModule *>::iterator iter = modules.begin();
	while( iter != modules.end() )
	{
		if( ( *iter )->IsActive() ) return true;
		iter++;	
	}
	return false;
}
