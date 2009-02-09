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


#include "Playlist.h"
#include "AddDirectory.h"
#include "Rebar.h"
#include "Main.h"
#include "Status.h"
#include "Console.h"
#include "Font.h"
#include "Playback.h"
#include "InputPlugin.h"
#include "Prefs.h"
#include "Config.h"
#include "Unicode.h"
#include "Path.h"
#include "commdlg.h"








HWND WindowPlaylist = NULL; // extern
// WNDPROC WndprocPlaylistBackup = NULL;

// int iCurIndex = -1;
// int iMaxIndex = -1;



// LRESULT CALLBACK WndprocPlaylist( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );
void Playlist_SelectSingle( int iIndex );


struct PlaylistEntry
{
	TCHAR * szFilename;
	// More to come
};

/*
///////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playlist::GetCurIndex()
{
	return iCurIndex;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playlist::GetMaxIndex()
{
	return iMaxIndex;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::SetCurIndex( int iIndex )
{
	if( iIndex < 0 || iIndex > iMaxIndex ) return false;

	iCurIndex = iIndex;
	if( bPlaylistFollow )
	{
		Playlist_SelectSingle( iCurIndex );
	}

	return true;
}
*/


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::Create()
{
	if( WindowPlaylist ) return false;
	
	// If this is not called a crash occurs
	PlaylistView::Create();


	#ifndef NOGUI

	RECT ClientMain;
	GetClientRect( WindowMain, &ClientMain );

	const int iClientHeight    = ClientMain.bottom - ClientMain.top;
	const int iClientWidth     = ClientMain.right - ClientMain.left;
	const int iPlaylistHeight  = iClientHeight - iRebarHeight - iStatusHeight;
	
	WindowPlaylist = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		TEXT( "LISTBOX" ),
		NULL,
		WS_VSCROLL |
			LBS_DISABLENOSCROLL |
			LBS_EXTENDEDSEL |
			LBS_HASSTRINGS |
			LBS_NOTIFY |
			LBS_NOINTEGRALHEIGHT |
			WS_CHILD |
			WS_VISIBLE, // 
		0,
		iRebarHeight, //  + -2,
		iClientWidth,
		iPlaylistHeight,
		WindowMain,
		NULL,
		g_hInstance,
		NULL
	);

	// Exchange window procedure
	WndprocPlaylistBackup = ( WNDPROC )GetWindowLong( WindowPlaylist, GWL_WNDPROC );
	if( WndprocPlaylistBackup != NULL )
	{
		SetWindowLong( WindowPlaylist, GWL_WNDPROC, ( LONG )WndprocPlaylist );
	}
		
	Font::Apply( WindowPlaylist );

	#endif NOGUI
	


	TCHAR * szPlaylistMind = new TCHAR[ iHomeDirLen + 12 + 1 ];
	memcpy( szPlaylistMind, szHomeDir, iHomeDirLen * sizeof( TCHAR ) );
	memcpy( szPlaylistMind + iHomeDirLen, TEXT( "Plainamp.m3u" ), 12 * sizeof( TCHAR ) );

	

	szPlaylistMind[ iHomeDirLen + 12 ] = TEXT( '\0' );


	Playlist::AppendPlaylistFile( szPlaylistMind );

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline void Playlist_SelectSingle( int iIndex )
{
	SendMessage(
		WindowPlaylist,
		LB_SETSEL,
		FALSE,
		-1
	);

	SendMessage(
		WindowPlaylist,
		LB_SETSEL,
		TRUE,
		iIndex
	);
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline bool Playlist_IsSelected( int iIndex )
{
	return ( 0 != SendMessage(
		WindowPlaylist,
		LB_GETSEL,
		( WPARAM )iIndex,
		0
	) );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline bool Playlist_SelectModify( int iIndex, bool bSelected )
{
	return ( LB_ERR != SendMessage(
		WindowPlaylist,
		LB_SETSEL,
		( WPARAM )( bSelected ? TRUE : FALSE ),
		iIndex
	) );
}


/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist_Remove( int iIndex  )
{
	if( iIndex < 0 || iIndex > iMaxIndex ) return false;

	// Get entry data
	PlaylistEntry * entry = ( PlaylistEntry * )SendMessage(
		WindowPlaylist,
		LB_GETITEMDATA,
		iIndex,
		0
	);

	// Free data
	if( entry )
	{
		if( entry->szFilename ) delete [] entry->szFilename;
		delete entry;
	}

	SendMessage(
		WindowPlaylist,
		LB_DELETESTRING,
		iIndex,
		0
	);
	
	if( iIndex < iCurIndex )
		iCurIndex--;
	if( ( iIndex == iCurIndex ) && ( iCurIndex == iMaxIndex ) )
		iCurIndex = iMaxIndex - 1;
	iMaxIndex--;

	return true;
}
*/


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline int Playlist_GetCount()
{
	return ( int )SendMessage(
		WindowPlaylist,
		LB_GETCOUNT,
		0,
		0
	);
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline int Playlist_GetCaretIndex()
{
	return ( int )SendMessage(
		WindowPlaylist,
		LB_GETCARETINDEX,
		0,
		0
	);
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline bool Playlist_SetCaretIndex( int iIndex )
{
	return ( LB_OKAY == SendMessage(
		WindowPlaylist,
		LB_SETCARETINDEX,
		( WPARAM )iIndex,
		FALSE
	) );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline int Playlist_GetSelCount()
{
	return ( int )SendMessage(
		WindowPlaylist,
		LB_GETSELCOUNT,
		0,
		0
	);
}


/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playlist_MouseToIndex()
{
	POINT p;
	GetCursorPos( &p );
	ScreenToClient( WindowPlaylist, &p );
				
	int iIndex = ( int )SendMessage(
		WindowPlaylist,
		LB_ITEMFROMPOINT,
		0,
		p.x | ( p.y << 16 )
	);
	
	if( ( iIndex < 0 ) || ( iIndex > iMaxIndex ) )
		return -1;
	else
		return iIndex;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::Clear()
{
	if( iMaxIndex < 0 ) return false;

	int iCount = iMaxIndex + 1;
	while( iCount-- )
	{
		Playlist_Remove( iCount );
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::RemoveSelected()
{
	int iSelCount = Playlist_GetSelCount();
	if( iSelCount < 0 ) return false;

	// Which items are selected?
	int * sel = new int[ iSelCount ];
	LRESULT lResult = SendMessage(
		WindowPlaylist,
		LB_GETSELITEMS,
		( WPARAM )iSelCount,
		( LPARAM )sel
	);

	// Remove
	if( lResult > 0 )
	{
		while( lResult-- )
		{
			int iIndex = sel[ lResult ];
			Playlist_Remove( iIndex );
		}
	}

	delete [] sel;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::Crop()
{
	int iAllCount = Playlist_GetCount();
	if( iAllCount < 1 ) return false;

	int iSelCount = Playlist_GetSelCount();
	if( iSelCount < 0 )
	{
		return false;
	}
	else if( iSelCount == 0 )
	{
		// None selected
		return Clear();
	}

	// Which items are selected?
	int * sel = new int[ iSelCount ];
	LRESULT lResult = SendMessage(
		WindowPlaylist,
		LB_GETSELITEMS,
		( WPARAM )iSelCount,
		( LPARAM )sel
	);
	
	int iLowerEqualIndex = iSelCount - 1;
	for( int i = iAllCount - 1; i >= 0; i-- )
	{
		while( ( sel[ iLowerEqualIndex ] > i ) && ( iLowerEqualIndex > 0 ) )
		{
			iLowerEqualIndex--;
		}
		
		if( i != sel[ iLowerEqualIndex ] )
		{
			// Not selected -> remove
			Playlist_Remove( i );
		}
	}
	
	delete [] sel;
	return true;
}
*/

/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist_MoveSel( bool bUpOrDown )
{
	static bool bMoving = false;	
	if( bMoving ) return false;

	bMoving = true;

	const int iSelCount = Playlist_GetSelCount();
	if( iSelCount < 0 )
	{
		// No items selected
		bMoving = false;
		return false;
	}

	// Which items are selected?
	int * sel = new int[ iSelCount ];
	LRESULT lResult = SendMessage(
		WindowPlaylist,
		LB_GETSELITEMS,
		( WPARAM )iSelCount,
		( LPARAM )sel
	);

	if( lResult <= 0 )
	{
		// Nothing to move
		delete [] sel;
		bMoving = false;
		return false;
	}
	
	if( ( bUpOrDown && ( sel[ 0 ] == 0 ) ) ||
		( !bUpOrDown && ( sel[ iSelCount - 1 ] == iMaxIndex ) ) )
	{
		// Cannot move
		delete [] sel;
		bMoving = false;
		return false;
	}
	
	const int iOldTop = ( int )SendMessage(
		WindowPlaylist,
		LB_GETTOPINDEX,
		0,
		0
	);
	
	//     1 _2_[3][4][5] 6  7 [8] 9
	// --> 1 [3][4][5]_2_ 6 [8]_7_ 9

	// Redrawing OFF
	// SendMessage( WindowPlaylist, WM_SETREDRAW, ( WPARAM )FALSE, 0 );

	int i = ( bUpOrDown ? 0 : iSelCount - 1 );
	do
	{
		// Backup the jumper
		PlaylistEntry * entry_old = ( PlaylistEntry * )SendMessage(
			WindowPlaylist,
			LB_GETITEMDATA,
			sel[ i ] + ( bUpOrDown ? -1 : 1 ),
			0
		);

		do
		{
			// Copy <n> on <n +/- 1>
			PlaylistEntry * entry_new = ( PlaylistEntry * )SendMessage(
				WindowPlaylist,
				LB_GETITEMDATA,
				sel[ i ],
				0
			);
			
			int iDest = sel[ i ] + ( bUpOrDown ? -1 : 1 );
			
			// Update entry display == delete, insert, set data
			SendMessage(
				WindowPlaylist,
				LB_DELETESTRING,
				iDest,
				0
			);
			iDest = ( int )SendMessage(
				WindowPlaylist,
				LB_INSERTSTRING,
				iDest,
				( LPARAM )entry_new->szFilename
			);
			SendMessage(
				WindowPlaylist,
				LB_SETITEMDATA,
				iDest,
				( LPARAM )entry_new
			);
			
			if( sel[ i ] == iCurIndex )
			{
				iCurIndex += ( bUpOrDown ? -1 : 1 );
			}
			
			i += ( bUpOrDown ? 1 : -1 );
		} while( bUpOrDown
			?
			( i < iSelCount ) && ( sel[ i - 1 ] + 1 == sel[ i ] )
			:
			( i >= 0 )        && ( sel[ i + 1 ] - 1 == sel[ i ] )
		);
		
		// Place the jumper
		int iLast = ( bUpOrDown ? sel[ i - 1 ] : sel[ i + 1 ] );

		// Update entry display == delete, insert, set data
		SendMessage(
			WindowPlaylist,
			LB_DELETESTRING,
			iLast,
			0
		);
		iLast = ( int )SendMessage(
			WindowPlaylist,
			LB_INSERTSTRING,
			iLast,
			( LPARAM )entry_old->szFilename
		);
		SendMessage(
			WindowPlaylist,
			LB_SETITEMDATA,
			iLast,
			( LPARAM )entry_old
		);
	} while( bUpOrDown
		?
		( i < iSelCount )
		:
		( i >= 0 )
	);
	
	// Select new indices (old selection went away on insert/delete
	if( bUpOrDown )
	{
		for( i = 0; i < iSelCount; i++ )
			SendMessage( WindowPlaylist, LB_SETSEL, TRUE, sel[ i ] - 1 );
	}
	else
	{
		for( i = 0; i < iSelCount; i++ )
			SendMessage( WindowPlaylist, LB_SETSEL, TRUE, sel[ i ] + 1 );
	}
	
	// Prevent scrolling
	SendMessage(
		WindowPlaylist,
		LB_SETTOPINDEX,
		( WPARAM )iOldTop,
		0
	);
	
	// Redrawing ON
	// SendMessage( WindowPlaylist, WM_SETREDRAW, ( WPARAM )TRUE, 0 );

	
	delete [] sel;
	bMoving = false;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocPlaylist( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{

	static bool bDragging  = false;
	static bool bMoveLock  = false;
	static int  iDragStartY;
	static int  iItemHeight = 0x7fffffff;
	
	switch( message )
	{
	case WM_MOUSEMOVE:
		{
			if( !bDragging || bMoveLock ) break;
			bMoveLock = true;
			
			const int y = HIWORD( lp );
			const int diff = y - iDragStartY;
			if( abs( diff ) > iItemHeight / 2 )
			{
				iDragStartY += ( ( diff > 0 ) ? iItemHeight : -iItemHeight );
				Playlist_MoveSel( diff < 0 );
			}
			
			bMoveLock = false;
			break;
		}

	case WM_LBUTTONDOWN:
		{
			if( GetKeyState( VK_MENU ) >= 0 ) break;

			// Dragging ON
			iDragStartY = HIWORD( lp );
			iItemHeight = ( int )SendMessage(
				WindowPlaylist,
				LB_GETITEMHEIGHT,
				0,
				0
			);
			bDragging = true;

			return 0;
		}

	case WM_LBUTTONUP:
		// Dragging OFF
		bDragging = false;
		break;

	case WM_SYSKEYDOWN:
		switch( wp ) // [Alt]+[...]
		{
		case VK_UP:
			Playlist_MoveSel( true );
			break;

		case VK_DOWN:
			Playlist_MoveSel( false );
			break;
		}
		break;

	case WM_CHAR:
	case WM_KEYUP:
		// SMALL LETTERS!!!!!!
		switch( wp )
		{
		case 'z':
		case 'y':
		case 'x':
		case 'c':
		case 'v':
		case 'b':
		case 'l':
			return 0;
		}
		break;

	case WM_KEYDOWN:
		{
			const bool bShift    = ( ( GetKeyState( VK_SHIFT   ) & 0x8000 ) != 0 );
			const bool bControl  = ( ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0 );
			// const bool bAlt      = ( ( GetKeyState( VK_MENU    ) & 0x8000 ) != 0 );
			

			switch( wp )
			{
			case VK_LEFT:
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_REW5S, 0 );
				return 0;
				
			case VK_RIGHT:
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_FFWD5S, 0 );
				return 0;
				
			case VK_UP:
				if( bControl && !bShift )
				{
					// Move caret not selection
					const int iCaretBefore = Playlist_GetCaretIndex();
					if( iCaretBefore > 0 )
					{
						Playlist_SetCaretIndex( iCaretBefore - 1 );
					}
					else if( ( iCaretBefore == 0 ) && bInfinitePlaylist )
					{
						Playlist_SetCaretIndex( iMaxIndex );
					}
					return 0;
				}
				else
				{
					if( bInfinitePlaylist )
					{
						if( Playlist_GetCaretIndex() != 0 ) break;
						Playlist_SelectSingle( iMaxIndex );
						return 0; // Or it will increase one more
					}
				}
				break;
	
			case VK_DOWN:
				if( bControl && !bShift )
				{
					// Move caret not selection
					const int iCaretBefore = Playlist_GetCaretIndex();
					if( ( iCaretBefore < iMaxIndex ) && ( iCaretBefore >= 0 ) )
					{
						Playlist_SetCaretIndex( iCaretBefore + 1 );
					}
					else if( ( iCaretBefore == iMaxIndex ) && bInfinitePlaylist )
					{
						Playlist_SetCaretIndex( 0 );
					}
					return 0;
				}
				else
				{
					if( bInfinitePlaylist )
					{
						if( Playlist_GetCaretIndex() != iMaxIndex ) break;
						Playlist_SelectSingle( 0 );
						return 0; // Or it will increase one more
					}
				}
				break;
				
			case VK_SPACE:
				if( bControl && !bShift )
				{
					const int iCaret = Playlist_GetCaretIndex();
					if( iCaret == -1 ) return 0;
					bool bSelected = Playlist_IsSelected( iCaret );
					Playlist_SelectModify( iCaret, !bSelected );
				}
				return 0;
	
			case VK_DELETE:
				{
					if( bShift ) break;
				
					if( bControl )
						playlist->RemoveSelected( false );	
					else
						playlist->RemoveSelected( true );
	
					break;
				}
	
			case VK_RETURN:
				iCurIndex = Playlist_GetCaretIndex();
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON2, 0 );
				return 0;
				
			case 'Y':
			case 'Z':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON1, 0 );
				return 0;
	
			case 'X':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON2, 0 );
				return 0;
	
			case 'C':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON3, 0 );
				return 0;
				
			case 'V':
				// Todo modifiers pressed? -> fadeout/...
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON4, 0 );
				return 0;
	
			case 'B':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_BUTTON5, 0 );
				return 0;
	/			
			case 'J':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_JUMPFILE, 0 );
				return 0;
	/		
			case 'L':
				if( bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, bShift ? WINAMP_FILE_DIR : WINAMP_FILE_PLAY, 0 );
				return 0;
			}
			break;
		}

	case WM_LBUTTONDBLCLK:
		iCurIndex = Playlist_MouseToIndex();
		if( iCurIndex < 0 ) break;
		Playback::Play();
		Playback::UpdateSeek();
		break;

	}
	return CallWindowProc( WndprocPlaylistBackup, hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::Add( int iIndex, TCHAR * szDisplay, TCHAR * szFilename )
{
	iMaxIndex++;
	if( iIndex < 0 || iIndex > iMaxIndex ) iIndex = iMaxIndex;
	if( iIndex <= iCurIndex )
	{
		iCurIndex++;
	}
	
	// Create entry data
	PlaylistEntry * new_entry = new PlaylistEntry;
	new_entry->szFilename = szFilename;
	
	iIndex = ( int )SendMessage(
		WindowPlaylist,
		LB_INSERTSTRING, // LB_ADDSTRING,
		iIndex,
		( LPARAM )szDisplay
	);
	
	// Associate data
	SendMessage(
		WindowPlaylist,
		LB_SETITEMDATA,
		iIndex,
		( LPARAM )new_entry
	);
	
	return true;
}
*/


////////////////////////////////////////////////////////////////////////////////
/// Opens a dialog box and loads the playlist if <bOpenOrSave> is [true],
/// or saves the playlist if it is [false].
////////////////////////////////////////////////////////////////////////////////
bool Playlist_DialogBoth( bool bOpenOrSave )
{
	TCHAR szFilters[] = TEXT(
		"All files (*.*)\0*.*\0"
		"Playlist files (*.M3U)\0*.m3u\0"
		"\0"
	);
	TCHAR szFilename[ MAX_PATH ] = TEXT( "\0" );

	OPENFILENAME ofn;
	memset( &ofn, 0, sizeof( OPENFILENAME ) ); 
	ofn.lStructSize        = sizeof( OPENFILENAME );
	ofn.hwndOwner          = WindowMain;
	ofn.hInstance          = g_hInstance;
	ofn.lpstrFilter        = szFilters;
	ofn.lpstrCustomFilter  = NULL;
	ofn.nMaxCustFilter     = 0;
	ofn.nFilterIndex       = 2;
	ofn.lpstrFile          = szFilename;
	ofn.nMaxFile           = MAX_PATH;
	ofn.Flags              = OFN_EXPLORER |
								OFN_ENABLESIZING |
								( bOpenOrSave ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT ) |
								OFN_PATHMUSTEXIST |
								OFN_HIDEREADONLY;
	ofn.nMaxFileTitle      = 0,
	ofn.lpstrInitialDir    = NULL;
	ofn.lpstrTitle         = ( bOpenOrSave ? TEXT( "Open playlist" ) : TEXT( "Save playlist" ) );

	if( bOpenOrSave )
	{
		if( !GetOpenFileName( &ofn ) ) return false;
	}
	else
	{
		if( !GetSaveFileName( &ofn ) ) return false;
	}
	
	if( bOpenOrSave )
	{
		// Open
		const int iFilenameLen = ( int )_tcslen( szFilename );
		if( !_tcsncmp( szFilename + iFilenameLen - 3, TEXT( "m3u" ), 3 ) )
		{
			// Playlist file
			playlist->RemoveAll();
			Playlist::AppendPlaylistFile( szFilename );
			Playback::Play();

			
			Console::Append( TEXT( "Playlist.cpp:Playlist_DialogBoth() called Playback::Play()" ) );
			Console::Append( TEXT( " " ) );
		}
	}
	else
	{
		// TODO: Check extension, ask for appending if missing
		
		// Save
		Playlist::ExportPlaylistFile( szFilename );
	}

	return true;
	
}


////////////////////////////////////////////////////////////////////////////////
/// Opens a dialog box and loads the selected playlist
////////////////////////////////////////////////////////////////////////////////
bool Playlist::DialogOpen()
{
	return Playlist_DialogBoth( true );
}



////////////////////////////////////////////////////////////////////////////////
/// Opens a dialog box and saves the playlist to the filename selected
////////////////////////////////////////////////////////////////////////////////
bool Playlist::DialogSaveAs()
{
	return Playlist_DialogBoth( false );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::AppendPlaylistFile( TCHAR * szFilename )
{



	// Open playlist file
	HANDLE hFile = CreateFile(
		szFilename,             // LPCTSTR lpFileName
		FILE_READ_DATA,         // DWORD dwDesiredAccess
		FILE_SHARE_READ,        // DWORD dwShareMode
		NULL,                   // LPSECURITY_ATTRIBUTES lpSecurityAttributes
		OPEN_EXISTING,          // DWORD dwCreationDisposition
		FILE_ATTRIBUTE_NORMAL,  // DWORD dwFlagsAndAttributes
		NULL                    // HANDLE hTemplateFile
	);

	// This will happen if there is no Playlist.m3u
	/*
	if( hFile == INVALID_HANDLE_VALUE )
	{
		// MessageBox( 0, TEXT( "Could not read playlist file" ), TEXT( "Error" ), MB_ICONERROR );
		//Console::Append( "We got INVALID_HANDLE_VALUE" );
		return false;
	}
	*/
	// Disable this
	//const bool bEmptyBefore = ( playlist->GetSize() == 0 );


// =======================================================================================
	// Remove filename from <szFilename> so we can
	// use it as relative directory root
	TCHAR * szWalk = szFilename + _tcslen( szFilename ) - 1;
	while( ( szWalk > szFilename ) && ( *szWalk != TEXT( '\\' ) ) ) szWalk--;
	szWalk++;
	*szWalk = TEXT( '\0' );

	TCHAR * szBaseDir = szFilename;
	const int iBaseDirLen = ( int )_tcslen( szBaseDir );


	DWORD iSizeBytes = GetFileSize( hFile, NULL );
	if( iSizeBytes <= 0 )
	{
		CloseHandle( hFile );
		return false;
	}

	// Allocate
	char * rawdata = new char[ iSizeBytes + 1 ]; // One more so we can write '\0' on EOF
	DWORD iBytesRead;

	// =======================================================================================
	// Read whole file
	ReadFile(
		hFile,        // HANDLE hFile
		rawdata,      // LPVOID lpBuffer
		iSizeBytes,   // DWORD nNumberOfBytesToRead
		&iBytesRead,  // LPDWORD lpNumberOfBytesRead
		NULL          // LPOVERLAPPED lpOverlapped
	);

	if( iBytesRead < iSizeBytes )
	{
		delete [] rawdata;
		CloseHandle( hFile );
		
		MessageBox( 0, TEXT( "Could not read whole file" ), TEXT( "Error" ), MB_ICONERROR );
		return false;
	}

	// Parse file content
	
	// File must be
	// * M3U
	// * ANSI
	
	char * walk = rawdata;
	const char * eof = rawdata + iSizeBytes;
	
	char * beg = rawdata;
	char * end;
	
	while( true )
	{
		// Find newline or eof
		while( ( walk < eof ) && ( *walk != '\015' ) && ( *walk != '\012' ) ) walk++;
		end = walk;
		
		if( ( end - beg > 2 ) && ( *beg != '#' ) )
		{

			TCHAR * szKeep;
			if( beg[ 1 ] == ':' )
			{
				// TODO: Better detection, network path?
				
				// Absolute path, skip this
				/*
				const int iLen = end - beg;

				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "iLen <%i>" ), iLen );
				Console::Append( szBuffer );

				szKeep = new TCHAR[ iLen + 1 ];
				ToTchar( szKeep, beg, iLen );
				szKeep[ iLen ] = TEXT( '\0' );
				*/
				
			}
			else
			{
				// Skip initial so we don't get a double backslash in between



				while( ( beg[ 0 ] == '\\' ) && ( beg < end ) ) beg++;
				
				// Relative path
				const int iSecondLen = end - beg;
				szKeep = new TCHAR[ iBaseDirLen + iSecondLen + 1 ];
				memcpy( szKeep, szBaseDir, iBaseDirLen * sizeof( TCHAR ) );
				ToTchar( szKeep + iBaseDirLen, beg, iSecondLen );
				
				szKeep[ iBaseDirLen + iSecondLen ] = TEXT( '\0' );
				
				UnbloatFilename( szKeep, false );
			}

			// if( !Add( iMaxIndex + 1, szKeep, szKeep ) ) break;
			playlist->PushBack( szKeep );
		}
		
		// Skip newlines
		while( ( walk < eof ) && ( ( *walk == '\015' ) || ( *walk == '\012' ) ) ) walk++;
		if( walk == eof )
		{
			break;
		}

		beg = walk;
	}

	delete [] rawdata;
	CloseHandle( hFile );
/*	
	if( bEmptyBefore )
	{
		iCurIndex = 0;
	}
*/	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::ExportPlaylistFile( TCHAR * szFilename )
{
	// Open playlist file
	HANDLE hFile = CreateFile(
		szFilename,             // LPCTSTR lpFileName
		FILE_WRITE_DATA,        // DWORD dwDesiredAccess
		0,                      // DWORD dwShareMode
		NULL,                   // LPSECURITY_ATTRIBUTES lpSecurityAttributes
		CREATE_ALWAYS,          // DWORD dwCreationDisposition
		FILE_ATTRIBUTE_NORMAL,  // DWORD dwFlagsAndAttributes
		NULL                    // HANDLE hTemplateFile
	);

	if( hFile == INVALID_HANDLE_VALUE )
	{
		MessageBox( 0, TEXT( "Could not write playlist file" ), TEXT( "Error" ), MB_ICONERROR );
		return false;
	}
	
	
	// Remove filename from <szFilename> so we can
	// use it as relative directory root
	TCHAR * szWalk = szFilename + _tcslen( szFilename ) - 1;
	while( ( szWalk > szFilename ) && ( *szWalk != TEXT( '\\' ) ) ) szWalk--;
	szWalk++;
	*szWalk = TEXT( '\0' );

	TCHAR * szBaseDir = szFilename;
	const int iBaseDirLen = ( int )_tcslen( szBaseDir );
	
	
	char * rawdata = new char[ ( playlist->GetMaxIndex() + 1 ) * ( MAX_PATH + 2 ) ];
	char * walk = rawdata;

	// Write playlist to buffer
	const int iMaxMax = playlist->GetMaxIndex();
	for( int i = 0; i <= iMaxMax; i++ )
	{
		// Get
		TCHAR * szEntry = GetFilename( i );
		if( !szEntry ) break;
		int iEntryLen = ( int )_tcslen( szEntry );

		// Copy
		TCHAR * szTemp = new TCHAR[ iEntryLen + 1 ];
		memcpy( szTemp, szEntry, iEntryLen * sizeof( TCHAR ) );
		szTemp[ iEntryLen ] = TEXT( '\0' );

		// Convert
		if( ApplyRootToFilename( szBaseDir, szTemp ) )
		{
			// Update length or we are writing too much
			iEntryLen = ( int )_tcslen( szTemp );
		}
		
		// Copy
#ifdef PA_UNICODE
		ToAnsi( walk, szTemp, iEntryLen );
#else
		memcpy( walk, szTemp, iEntryLen );
#endif

		delete [] szTemp;
		
		walk += iEntryLen;
		memcpy( walk, "\015\012", 2 );
		walk += 2;
	}
	
	const DWORD iSizeBytes = walk - rawdata;
	DWORD iBytesRead;
	WriteFile(
		hFile,        // HANDLE hFile,
		rawdata,      // LPCVOID lpBuffer,
		iSizeBytes,   // DWORD nNumberOfBytesToWrite,
		&iBytesRead,  // LPDWORD lpNumberOfBytesWritten,
		NULL          // LPOVERLAPPED lpOverlapped
	);

	if( iBytesRead < iSizeBytes )
	{
		delete [] rawdata;
		CloseHandle( hFile );
		
		MessageBox( 0, TEXT( "Could not write whole file" ), TEXT( "Error" ), MB_ICONERROR );
		return false;
	}

	delete [] rawdata;
	CloseHandle( hFile );

	return true;
}


/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::Append( TCHAR * szDisplay, TCHAR * szFilename )
{
	return Add( iMaxIndex + 1, szDisplay, szFilename );
}
*/


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
TCHAR * Playlist::GetFilename( int iIndex )
{
	// if( iIndex < 0 || iIndex > iMaxIndex ) return NULL;

/*
	PlaylistEntry * entry = ( PlaylistEntry * )SendMessage(
		WindowPlaylist,
		LB_GETITEMDATA,
		iIndex,
		0
	);
	
	return ( entry ? entry->szFilename : NULL );
	*/
	//TCHAR * szFilename = "C:\Files\Spel och spelfusk\Console\Gamecube\Code\vgmstream (isolate ast)\Music\demo36_02.ast";
	//return szFilename;
	return ( TCHAR * )playlist->Get( iIndex );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playlist::GetFilename( int iIndex, char * szAnsiFilename, int iChars )
{
	// if( iIndex < 0 || iIndex > iMaxIndex ) return 0;
	/*
	PlaylistEntry * entry = ( PlaylistEntry * )SendMessage(
		WindowPlaylist,
		LB_GETITEMDATA,
		iIndex,
		0
	);
	if( !entry || !entry->szFilename ) return 0;

	TCHAR * & szFilename = entry->szFilename;
	*/
	TCHAR * szFilename = ( TCHAR * )playlist->Get( iIndex );
	
	const int iFilenameLen = ( int )_tcslen( szFilename );
	const int iCopyLen = ( iFilenameLen < iChars ) ? iFilenameLen : iChars;

#ifdef PA_UNICODE
	ToAnsi( szAnsiFilename, szFilename, iCopyLen );
#else
	memcpy( szAnsiFilename, szFilename, iCopyLen );
#endif

	szAnsiFilename[ iCopyLen ] = '\0';
	return iCopyLen;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playlist::GetTitle( int iIndex, char * szAnsiTitle, int iChars )
{
	// if( iIndex < 0 || iIndex > iMaxIndex ) return 0;
	/*
	TCHAR * szFilename = ( TCHAR * )SendMessage(
		WindowPlaylist,
		LB_GETITEMDATA,
		iIndex,
		0
	);
	if( !szFilename ) return 0;
	*/
	TCHAR * szFilename = ( TCHAR * )playlist->Get( iIndex );

	// Get extension
	const int iFilenameLen = ( int )_tcslen( szFilename );
	TCHAR * szExt = szFilename + iFilenameLen - 1;
	while( ( szExt > szFilename ) && ( *szExt != TEXT( '.' ) ) ) szExt--;
	szExt++;
	
	// Get plugin for extension
	map <TCHAR *, InputPlugin *, TextCompare>::iterator iter = ext_map.find( szExt );
	if( iter == ext_map.end() ) return 0;
	InputPlugin * input_plugin = iter->second;

#ifdef PA_UNICODE
	// Filename
	char * szTemp = new char[ iFilenameLen + 1 ];
	ToAnsi( szTemp, szFilename, iFilenameLen );
	szTemp[ iFilenameLen ] = '\0';
	
	// Ansi Title
	char szTitle[ 2000 ] = "\0";
	int length_in_ms;
	input_plugin->plugin->GetFileInfo( szTemp, szTitle, &length_in_ms );
	const int iTitleLen = strlen( szTitle );
	memcpy( szAnsiTitle, szTitle, iChars * sizeof( char ) );
	szTitle[ iChars ] = '\0';
#else
	char szTitle[ 2000 ] = "\0";
	int length_in_ms;
	input_plugin->plugin->GetFileInfo( szFilename, szTitle, &length_in_ms );
	const int iTitleLen = ( int )strlen( szAnsiTitle );
	memcpy( szAnsiTitle, szTitle, iChars * sizeof( char ) );
	szTitle[ iChars ] = '\0';
#endif

	return ( iTitleLen < iChars ) ? iTitleLen : iChars;
}


/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::SelectZero()
{
	SendMessage(
		WindowPlaylist,
		LB_SETSEL,
		FALSE,
		-1
	);
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playlist::SelectAll()
{
	SendMessage(
		WindowPlaylist,
		LB_SETSEL,
		TRUE,
		-1
	);
	return true;
}
*/
