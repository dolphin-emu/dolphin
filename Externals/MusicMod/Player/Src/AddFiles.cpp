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


#include "AddFiles.h"
#include "InputPlugin.h"
#include "Main.h"
#include "Playlist.h"
#include <commdlg.h>



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
void AddFiles()
{
	int total = 0;
	
	int iFilterLen = 0;
	
	InputPlugin * input;
	vector <InputPlugin *>::iterator iter;

	// Get len
//	if( input_plugins.empty() ) return;
	iter = input_plugins.begin();
	while( iter != input_plugins.end() )
	{
		input = *iter;
		if( !input ) iter++;
		iFilterLen += input->iFiltersLen;
		iter++;
	}
	
//	if( !iFilterLen ) return;

	iFilterLen += 40 + 29 + ( int )ext_map.size() * ( 2 + 4 + 1 ) + 7;

	TCHAR * szFilters = new TCHAR[ iFilterLen ];
	TCHAR * walk = szFilters;

	// ..................1.........1....\....\.1.........1........\.1...
	memcpy( walk, TEXT( "All files (*.*)\0*.*\0All supported types\0" ), 40 * sizeof( TCHAR ) );
	walk += 40;
	
	// Add all extensions as ";*.ext"
//	if( ext_map.empty() ) return;
	map <TCHAR *, InputPlugin *, TextCompare>::iterator iter_ext = ext_map.begin();
	bool bFirst = true;
	while( iter_ext != ext_map.end() )
	{
		if( !bFirst )
		{
			memcpy( walk, TEXT( ";*." ), 3 * sizeof( TCHAR ) );
			walk += 3;
		}
		else
		{
			memcpy( walk, TEXT( "*." ), 2 * sizeof( TCHAR ) );
			walk += 2;
			bFirst = false;	
		}
		
		TCHAR * szExt = iter_ext->first;
		int uLen = ( int )_tcslen( szExt );
		memcpy( walk, szExt, uLen * sizeof( TCHAR ) );
		walk += uLen;
		
		iter_ext++;
	}
	
	// *walk = TEXT( '\0' );
	// walk++;
	
	// ..................1..........1...
	memcpy( walk, TEXT( ";*.m3u\0" ), 7 * sizeof( TCHAR ) );
	walk += 7;
	
	// ..................1.........1.........1...........1...
	memcpy( walk, TEXT( "Playlist files (*.M3U)\0*.m3u\0" ), 29 * sizeof( TCHAR ) );
	walk += 29;

	// Copy filters
	iter = input_plugins.begin();
	while( iter != input_plugins.end() )
	{
		input = *iter;
		if( !input ) iter++;
		memcpy( walk, input->szFilters, input->iFiltersLen * sizeof( TCHAR ) );
		walk += input->iFiltersLen;
		iter++;
	}

	*walk = TEXT( '\0' );
	walk++;


////////////////////////////////////////////////////////////////////////////////		

	static TCHAR szFilenames[ 20001 ];
	*szFilenames = TEXT( '\0' ); // Each time!

	OPENFILENAME ofn;
	memset( &ofn, 0, sizeof( OPENFILENAME ) ); 
	ofn.lStructSize        = sizeof( OPENFILENAME );
	ofn.hwndOwner          = WindowMain;
	ofn.hInstance          = g_hInstance;
	ofn.lpstrFilter        = szFilters; // "MPEG Layer 3\0*.mp3\0";
	ofn.lpstrCustomFilter  = NULL;
	ofn.nMaxCustFilter     = 0;
	ofn.nFilterIndex       = 2;
	ofn.lpstrFile          = szFilenames;
	ofn.nMaxFile           = 20000;
	ofn.Flags              = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nMaxFileTitle      = 0, // NULL;
	ofn.lpstrInitialDir    = NULL;
	ofn.lpstrTitle         = TEXT( "Add files" );

	if( !GetOpenFileName( &ofn ) ) return;
	
	int uDirLen = ( int )_tcslen( szFilenames );
	TCHAR * szDir = szFilenames;
	
	TCHAR * szFileWalk = szDir + uDirLen + 1;
	if( *szFileWalk == TEXT( '\0' ) ) // "\0\0" or just "\0"?
	{
		// \0\0 -> Single file
		if( !_tcsncmp( szDir + uDirLen - 3, TEXT( "m3u" ), 3 ) )
		{
			// Playlist file
			Playlist::AppendPlaylistFile( szDir );
		}
		else
		{
			// Music file
			TCHAR * szKeep = new TCHAR[ uDirLen + 1 ];
			memcpy( szKeep, szDir, uDirLen * sizeof( TCHAR ) );
			szKeep[ uDirLen ] = TEXT( '\0' );
			playlist->PushBack( szKeep );
		}
	}
	else
	{
		// \0 -> Several files
		int iFileLen;
		while( *szFileWalk != TEXT( '\0' ) )
		{
			iFileLen = ( int )_tcslen( szFileWalk );
			if( !iFileLen ) return;
			
			TCHAR * szKeep = new TCHAR[ uDirLen + 1 + iFileLen + 1 ];
			memcpy( szKeep, szDir, uDirLen * sizeof( TCHAR ) );
			szKeep[ uDirLen ] = TEXT( '\\' );
			memcpy( szKeep + uDirLen + 1, szFileWalk, iFileLen * sizeof( TCHAR ) );
			szKeep[ uDirLen + 1 + iFileLen ] = TEXT( '\0' );

			if( !_tcsncmp( szKeep + uDirLen + 1 + iFileLen - 3, TEXT( "m3u" ), 3 ) )
			{
				// Playlist file
				Playlist::AppendPlaylistFile( szKeep );
				delete [] szKeep;
			}
			else
			{
				// Music file
				playlist->PushBack( szKeep );
			}
			
			szFileWalk += iFileLen + 1;
		}
	}
}
