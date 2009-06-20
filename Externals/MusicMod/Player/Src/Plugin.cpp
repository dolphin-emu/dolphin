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


#include "Plugin.h"
#include "InputPlugin.h"
#include "OutputPlugin.h"
#include "VisPlugin.h"
#include "DspPlugin.h"
#include "GenPlugin.h"



vector<Plugin *> plugins; // extern

int Plugin::iWndprocHookCounter = 0;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
Plugin::Plugin( TCHAR * szDllpath )
{
	hDLL           = NULL;
	szName         = NULL;
	iNameLen       = 0;

	
	iFullpathLen   = ( int )_tcslen( szDllpath );
	szFullpath     = new TCHAR[ iFullpathLen + 1 ];
	memcpy( szFullpath, szDllpath, iFullpathLen * sizeof( TCHAR ) );
	szFullpath[ iFullpathLen ] = TEXT( '\0' );
	
	TCHAR * walk = szFullpath + iFullpathLen - 1;
	while( ( *walk != TEXT( '\\') ) && ( walk >= szFullpath ) ) walk--;
	if( *walk == TEXT( '\\') ) walk++;
	
	szFilename     = walk;
	iFilenameLen   = iFullpathLen - ( walk - szFullpath );
	_tcslwr( szFilename );
	
	plugins.push_back( this );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
Plugin::~Plugin()
{
	if( szFullpath ) delete [] szFullpath;
	if( szName     ) delete [] szName;
}
