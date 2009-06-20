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


#include "VisPlugin.h"
#include "Unicode.h"
#include "Console.h"
#include "Main.h"



vector <VisPlugin *> vis_plugins; // extern

Lock VisLock = Lock( TEXT( "PLAINAMP_VIS_LOCK" ) ); // extern



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
VisPlugin::VisPlugin( TCHAR * szDllpath, bool bKeepLoaded ) : Plugin( szDllpath )
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
	
	vis_plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool VisPlugin::Load()
{
	// (1) Load DLL
	hDLL = LoadLibrary( GetFullpath() );
	if( !hDLL ) return false;

	// (2) Find export
	WINAMP_VIS_GETTER winampGetVisHeader =
		( WINAMP_VIS_GETTER )GetProcAddress( hDLL, "winampVisGetHeader" );
	if( winampGetVisHeader == NULL )
	{
		FreeLibrary( hDLL );
		hDLL = NULL;
		return false;
	}

	// (3) Get header
	header = winampGetVisHeader();
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
		if( !_strnicmp( header->description, "nullsoft ", 9 ) )
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
	winampVisModule * mod;
	int iFound = 0;
	while( true )
	{
		mod = header->getModule( iFound );
		if( !mod ) break;

		// (4a) Modify module
		mod->hDllInstance	= hDLL;
		mod->hwndParent		= WindowMain;
		mod->sRate          = 44100; // TODO
		mod->nCh            = 2; // TODO

		// (4b) Add module to list
		VisModule * vismod = new VisModule(
			mod->description,  // char * szName
			iFound,            // UINT uIndex
			mod,               // winampVisModule * mod
			this               // VisPlugin * plugin
		);
		modules.push_back( vismod );
		
		iFound++;

		_stprintf( szBuffer, TEXT( "   %s" ), vismod->GetName() );
		Console::Append( szBuffer );
	}

	Console::Append( TEXT( " " ) );

	if( !iFound ) return false;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool VisPlugin::Unload()
{
	if( !IsLoaded() ) return true;

	TCHAR szBuffer[ 5000 ];
	_stprintf( szBuffer, TEXT( "Unloading <%s>" ), GetFilename() );
	Console::Append( szBuffer );
	Console::Append( TEXT( " " ) );
	printf(  ">>>Unloading <%s>\n" , GetFilename() );

	header = NULL;

	/*
	TODO
	VisModule * walk;
	vector <VisModule *>::iterator iter = modules.begin();
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
bool VisPlugin::IsActive()
{
	vector <VisModule *>::iterator iter = modules.begin();
	while( iter != modules.end() )
	{
		if( ( *iter )->IsActive() ) return true;
		iter++;	
	}
	return false;
}
