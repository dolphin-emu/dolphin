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


#include "AddDirectory.h"
#include "Playlist.h"
#include "Main.h"
#include "InputPlugin.h"
#include <stdio.h>
#include <shlobj.h>
#include <vector>
#include <algorithm>

using namespace std;



HWND WindowBrowse = NULL;



void SearchFolder( TCHAR * szPath );



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
LPITEMIDLIST GetCurrentFolder()
{
/*
	How To Convert a File Path to an ITEMIDLIST
	http://support.microsoft.com/default.aspx?scid=kb;en-us;132750
*/
	LPITEMIDLIST   pidl;
	LPSHELLFOLDER  pDesktopFolder;
	TCHAR          szPath[ MAX_PATH ];
#ifndef PA_UNICODE
	OLECHAR        olePath[ MAX_PATH ];
#endif
	ULONG          chEaten;
	ULONG          dwAttributes;
	HRESULT        hr;

	//
	// Get the path we need to convert.
	//
	GetCurrentDirectory( MAX_PATH, szPath );

	//
	// Get a pointer to the Desktop's IShellFolder interface.
	//
	if( SUCCEEDED( SHGetDesktopFolder( &pDesktopFolder ) ) )
	{

		//
		// IShellFolder::ParseDisplayName requires the file name be in
		// Unicode.
		//
#ifndef	PA_UNICODE	
		MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, szPath, -1, olePath, MAX_PATH );
#endif
		//
		// Convert the path to an ITEMIDLIST.
		//
		// hr = pDesktopFolder->lpVtbl->ParseDisplayName(
		hr = pDesktopFolder->ParseDisplayName(
			( HWND__ * )pDesktopFolder,
			NULL,
#ifndef PA_UNICODE			
			olePath,
#else
			szPath,
#endif			
			&chEaten,
			&pidl,
			&dwAttributes
		);
	
		if( FAILED( hr ) )
		{
			// Handle error.
			return NULL;
		}
	
		//
		// pidl now contains a pointer to an ITEMIDLIST for .\readme.txt.
		// This ITEMIDLIST needs to be freed using the IMalloc allocator
		// returned from SHGetMalloc().
		//
	
		// release the desktop folder object
		// pDesktopFolder->lpVtbl->Release();
		pDesktopFolder->Release();

		return pidl;
	}
	else
	{
		return NULL;
	}
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
BOOL CALLBACK EnumChildProc( HWND hwnd, LPARAM lp )
{
	TCHAR szClassName[ 8 ] = TEXT( "\0" );
	HWND * hFirstFoundStatic = ( ( HWND * )lp );
	
	if( GetClassName( hwnd, szClassName, 7 ) )
	{
		if( !_tcscmp( szClassName, TEXT( "Static" ) ) )
		{
			if( *hFirstFoundStatic )
			{
				// Both found
				RECT r1;
				GetWindowRect( *hFirstFoundStatic, &r1 );
				
				RECT r2;
				GetWindowRect( hwnd, &r2 );
				
				// First must be taller one
				if( r1.bottom - r1.top < r2.bottom - r2.top )
				{
					// Swap
					RECT r = r1;
					HWND h = *hFirstFoundStatic;
					
					r1 = r2;
					*hFirstFoundStatic = hwnd;
					
					r2 = r;
					hwnd = h;
				}
				
				POINT xy2 = { r2.left, r2.top };
				ScreenToClient( WindowBrowse, &xy2 );
				
				SetWindowPos(
					*hFirstFoundStatic,
					NULL,
					0,
					0,
					r2.right - r2.left,
					r2.bottom - r2.top,
					SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER
				);
				
				SetWindowPos(
					hwnd,
					NULL,
					xy2.x,
					xy2.y + ( r2.bottom - r2.top ) - ( r1.bottom - r1.top ),
					r1.right - r1.left,
					r1.bottom - r1.top,
					SWP_NOOWNERZORDER | SWP_NOZORDER
				);

				
				return FALSE; // Stop
			}
			else
			{
				// First found
				*hFirstFoundStatic = hwnd;
			}
		}
	}
	
	return TRUE; // Continue
}



////////////////////////////////////////////////////////////////////////////////
// 
////////////////////////////////////////////////////////////////////////////////
int CALLBACK BrowseCallbackProc( HWND hwnd, UINT message, LPARAM lp, LPARAM wp )
{
	switch( message )
	{
	case BFFM_INITIALIZED:
		{
			WindowBrowse = hwnd;

			// Init with curdir
			SendMessage( hwnd, BFFM_SETSELECTION, FALSE, ( LPARAM )GetCurrentFolder() );
			
			// Swap static dimensions
			HWND hFirstFoundStatic = NULL;
			EnumChildWindows( hwnd, EnumChildProc, ( LPARAM )&hFirstFoundStatic );
			
			break;	
		}

	case BFFM_SELCHANGED:
        {
            TCHAR szPath[ MAX_PATH ] = TEXT( "\0" );
            SHGetPathFromIDList( ( LPITEMIDLIST )lp, szPath );
            SendMessage( hwnd, BFFM_SETSTATUSTEXT, 0, ( LPARAM )szPath );

	        break;
	    }		

	case BFFM_VALIDATEFAILED:
		return TRUE;

	}

	return 0;
}



////////////////////////////////////////////////////////////////////////////////
/// Shows a Browse-For-Folder dialog and recursively adds supported files
/// to the playlist. Files are sorted by full filaname before being added.
////////////////////////////////////////////////////////////////////////////////
void AddDirectory()
{
	TCHAR szPath[ MAX_PATH ];
	BROWSEINFO bi = { 0 };
	bi.hwndOwner  = WindowMain;
	bi.pidlRoot   = NULL; // Desktop folder
	bi.lpszTitle  = TEXT( "Please select a directory:" );
	bi.ulFlags    = BIF_VALIDATE | BIF_STATUSTEXT;
	bi.lpfn       = BrowseCallbackProc;

	LPITEMIDLIST pidl = SHBrowseForFolder( &bi );
	if( !pidl ) return;
	
	// Get path
	SHGetPathFromIDList( pidl, szPath );


	// Search
	SearchFolder( szPath );


	// Stay here
	SetCurrentDirectory( szPath );


	// Free memory used
	IMalloc * imalloc = 0;
	if( SUCCEEDED( SHGetMalloc( &imalloc ) ) )
	{
		imalloc->Free( pidl );
		imalloc->Release();
	}
}



////////////////////////////////////////////////////////////////////////////////
/* Warning: There is SetCurrentDirectory() here, be aware of it. We don't really
   want to use that. */
////////////////////////////////////////////////////////////////////////////////
void SearchFolder( TCHAR * szPath )
{
	// Remove trailing backslash
	int iPathLen = ( int )_tcslen( szPath );
	if( iPathLen < 1 ) return;
	if( szPath[ iPathLen - 1 ] == TEXT( '\\' ) )
	{
		iPathLen--;
	}

	// Init working buffer
	TCHAR szFullpath[ MAX_PATH ];
	memcpy( szFullpath, szPath, iPathLen * sizeof( TCHAR ) );
	szFullpath[ iPathLen     ] = TEXT( '\\' );
	szFullpath[ iPathLen + 1 ] = TEXT( '\0' );

	// Make pattern
	_tcscpy( szFullpath + iPathLen + 1, TEXT( "*" ) );

	// Find
	vector <TCHAR *> Files;
	vector <TCHAR *> Dirs;
	WIN32_FIND_DATA  FindFileData;
	HANDLE			 hFind;
	hFind = FindFirstFile( szFullpath, &FindFileData );
	if( hFind == INVALID_HANDLE_VALUE ) return;

	do
	{
		// Skip "." and ".."
		if(	!_tcscmp( FindFileData.cFileName, TEXT( "."  ) ) ||
			!_tcscmp( FindFileData.cFileName, TEXT( ".." ) ) ) continue;

		// Make full path
		_tcscpy( szFullpath + iPathLen + 1, FindFileData.cFileName );
		
		// Is directory?
		TCHAR * szPartname = new TCHAR[ MAX_PATH ];
		_tcscpy( szPartname, FindFileData.cFileName );
		if( SetCurrentDirectory( szFullpath ) )
		{
			// New dir
			Dirs.push_back( szPartname );
			continue;
		}

		// Search "."
		const int iFilenameLen = ( int )_tcslen( FindFileData.cFileName );
		TCHAR * szExt = FindFileData.cFileName + iFilenameLen - 1;
		while( ( szExt > FindFileData.cFileName ) && ( *szExt != TEXT( '.' ) ) ) szExt--;
		if( *szExt != TEXT( '.' ) ) continue;
		szExt++;

		// Check extension
		map <TCHAR *, InputPlugin *, TextCompare>::iterator iter = ext_map.find( szExt );
		if( iter == ext_map.end() ) continue;

		// New file
		Files.push_back( szPartname );
	}
	while( FindNextFile( hFind, &FindFileData ) );

	FindClose( hFind );
	vector <TCHAR *>::iterator iter;
	
	// Sort and recurse directories
	sort( Dirs.begin(), Dirs.end(), TextCompare() );
	iter = Dirs.begin();
	while( iter != Dirs.end() )
	{
		TCHAR * szWalk = *iter;

		_tcscpy( szFullpath + iPathLen + 1, szWalk );
		SearchFolder( szFullpath );
		
		iter++;
	}

	// Sort and add files
	sort( Files.begin(), Files.end(), TextCompare() );
	iter = Files.begin();
	while( iter != Files.end() )
	{
		TCHAR * szWalk = *iter;

		TCHAR * szKeep = new TCHAR[ MAX_PATH ];
		memcpy( szKeep, szFullpath, ( iPathLen + 1 ) * sizeof( TCHAR ) );
		_tcscpy( szKeep + iPathLen + 1, szWalk );
		playlist->PushBack( szKeep );
		
		iter++;	
	}
}
