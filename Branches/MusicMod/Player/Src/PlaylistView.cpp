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
#include "Main.h"
#include "Status.h"
#include "Rebar.h"
#include "Playback.h"
#include "Config.h"
#include "Util.h"

WNDPROC WndprocPlaylistBackup = NULL;
LRESULT CALLBACK WndprocPlaylist( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );


PlaylistControler * playlist = NULL; // extern

bool bPlaylistEntryNumberZeroPadding;
ConfBool cbPlaylistEntryNumberZeroPadding( &bPlaylistEntryNumberZeroPadding, TEXT( "PlaylistEntryNumberZeroPadding" ), CONF_MODE_PUBLIC, true );

int iCurPlaylistPosition;
ConfInt ciCurPlaylistPosition( &iCurPlaylistPosition, TEXT( "CurPlaylistPosition" ), CONF_MODE_INTERNAL, -1 );

bool bInfinitePlaylist;
ConfBool cbInfinitePlaylist( &bInfinitePlaylist, TEXT( "InfinitePlaylist" ), CONF_MODE_PUBLIC, false );


void PlaylistView::Create()
{
	RECT ClientMain;
	GetClientRect( WindowMain, &ClientMain );

	const int iClientHeight    = ClientMain.bottom - ClientMain.top;
	const int iClientWidth     = ClientMain.right - ClientMain.left;
	const int iPlaylistHeight  = iClientHeight - iRebarHeight - iStatusHeight;



	LoadCommonControls();


	DWORD       dwStyle;
	// HWND        WindowPlaylist;
	BOOL        bSuccess = TRUE;

	dwStyle =   WS_TABSTOP | 
				WS_CHILD | 
				WS_BORDER | 
				WS_VISIBLE |
				LVS_AUTOARRANGE | // TODO
				LVS_REPORT | 
				LVS_OWNERDATA |
				LVS_NOCOLUMNHEADER ;

	WindowPlaylist = CreateWindowEx(   WS_EX_CLIENTEDGE,          // ex style
									 WC_LISTVIEW,               // class name - defined in commctrl.h
									 TEXT( "" ),                        // dummy text
									 dwStyle,                   // style
		0,
		iRebarHeight, //  + -2,
		iClientWidth,
		iPlaylistHeight,
									 WindowMain,                // parent
									 NULL,        // ID
									 g_hInstance,                   // instance
									 NULL);                     // no extra data

	if(!WindowPlaylist) return; // TODO

	   
playlist = new PlaylistControler( WindowPlaylist, bPlaylistEntryNumberZeroPadding, &iCurPlaylistPosition );


	// Exchange window procedure
	WndprocPlaylistBackup = ( WNDPROC )GetWindowLong( WindowPlaylist, GWL_WNDPROC );
	if( WndprocPlaylistBackup != NULL )
	{
		SetWindowLong( WindowPlaylist, GWL_WNDPROC, ( LONG )WndprocPlaylist );
	}


	ListView_SetExtendedListViewStyle( WindowPlaylist, LVS_EX_FULLROWSELECT ); // | LVS_EX_GRIDLINES );
	playlist->Resize( WindowMain );

/*
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/listview/structures/lvcolumn.asp
 *
 * Remarks:
 * If a column is added to a list-view control with index 0 (the leftmost column)
 * and with LVCFMT_RIGHT or LVCFMT_CENTER specified, the text is not right-aligned
 * or centered. The text in the index 0 column is left-aligned. Therefore if you
 * keep inserting columns with index 0, the text in all columns are left-aligned.
 * If you want the first column to be right-aligned or centered you can make a dummy
 * column, then insert one or more columns with index 1 or higher and specify the
 * alignment you require. Finally delete the dummy column.
 */

LV_COLUMN lvColumn;
memset( &lvColumn, 0, sizeof( LV_COLUMN ) );
lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;

// Number column (with dummy hack)
lvColumn.fmt      = LVCFMT_LEFT;
lvColumn.cx       = 0;
lvColumn.pszText  = TEXT( "" );
ListView_InsertColumn( WindowPlaylist, 0, &lvColumn );
lvColumn.fmt      = LVCFMT_RIGHT;
lvColumn.cx       = 120;
lvColumn.pszText  = TEXT( "" );
ListView_InsertColumn( WindowPlaylist, 1, &lvColumn );
ListView_DeleteColumn( WindowPlaylist, 0 );

// Entry
lvColumn.fmt      = LVCFMT_LEFT;
lvColumn.cx       = 120;
lvColumn.pszText  = TEXT( "Filename" );
ListView_InsertColumn(WindowPlaylist, 1, &lvColumn);



/*
stupid test code

SCROLLINFO scrollinfo;
ZeroMemory( &scrollinfo, sizeof( SCROLLINFO ) );
scrollinfo.cbSize = sizeof( SCROLLINFO );
scrollinfo.fMask = 0; // SIF_DISABLENOSCROLL;

if( !GetScrollInfo( WindowPlaylist, SB_VERT, &scrollinfo ) )
{
	MessageBox( 0, "ERROR", "", 0 );
}
else
{
	MessageBox( 0, "OKAY", "", 0 );
	scrollinfo.fMask = SIF_DISABLENOSCROLL;
	SetScrollInfo( WindowPlaylist, SB_VERT, &scrollinfo, TRUE );   	
}

if( !ShowScrollBar( WindowPlaylist, SB_VERT, TRUE ) )
{
	MessageBox( 0, "ERROR ShowScrollBar", "", 0 );
}


SCROLLBARINFO scrollbarinfo;
scrollbarinfo.cbSize = sizeof( SCROLLBARINFO );
if( !GetScrollBarInfo( WindowPlaylist, OBJID_VSCROLL, &scrollbarinfo ) )
{
	MessageBox( 0, "ERROR GetScrollBarInfo", "", 0 );
}
*/

}









// Dragging
static int iItemHeight = 15;
static int iDragStartY = 0;
static bool bDragging = false;

// Liquid selection
static bool bLiquidSelecting = false;
static int iLastTouched = -1;

// Liquid or range selection
static int iSelAnchor = -1;

LRESULT CALLBACK WndprocPlaylist( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	/*
	 * Click, click and click
	 *
	 *  [Alt]   [Ctrl]    [Shift]    Action
	 * -------+---------+---------+-----------------------
	 *    X   |    X    |    X    |  
	 *    X   |    X    |         |  
	 *    X   |         |    X    |  
	 *    X   |         |         |  
	 *        |    X    |    X    |  Range selection
	 *        |    X    |         |  Toggle selection
	 *        |         |    X    |  Range selection
	 *        |         |         |  Single selection
	 *
	 *
	 * Click, hold and move
	 *
	 *  [Alt]   [Ctrl]    [Shift]    Action
	 * -------+---------+---------+-----------------------
	 *    X   |    X    |    X    |  Selection move
	 *    X   |    X    |         |  Selection move
	 *    X   |         |    X    |  Selection move
	 *    X   |         |         |  Selection move
	 *        |    X    |    X    |  
	 *        |    X    |         |  
	 *        |         |    X    |  
	 *        |         |         |  Liquid selection
	 */
	
	static bool bCapturing = true;
	
	switch( message )
	{
/*		
	case WM_CAPTURECHANGED:
		if( bCapturing && ( GetCapture() != WindowPlaylist ) )
		{
			MessageBox( 0, TEXT( "Capture stolen" ), TEXT( "" ), 0 );
		}
		break;
*/		
	case WM_MOUSEMOVE:
		if( bLiquidSelecting )
		{
			LVHITTESTINFO hittest;
			memset( &hittest, 0, sizeof( LVHITTESTINFO ) );
			hittest.pt.x = LOWORD( lp );
			hittest.pt.y = HIWORD( lp );
			const int iIndex = ( int )ListView_HitTest( WindowPlaylist, &hittest );
			if( iIndex == -1 ) return 0;
			if( iIndex == iLastTouched ) return 0;			
			
			// Note:  Update this as early as possible!
			//        We cannot be sure this code is
			//        not called two or three times at the
			//        same time without losing much speed
			//        but this at least lowers the chance
			const int iLastTouchedBackup = iLastTouched;
			iLastTouched = iIndex;

			const bool bControl  = ( ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0 );
			
			
			
			ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVIS_FOCUSED );
			ListView_SetItemState( WindowPlaylist, iIndex, LVIS_FOCUSED, LVIS_FOCUSED );

			// Below anchor?
			if( iIndex > iSelAnchor )
			{
				if( iIndex > iLastTouchedBackup )
				{
					// iSelAnchor
					// ..
					// iLastTouchedBackup
					// ..
					// >> iIndex <<
					if( iLastTouchedBackup > iSelAnchor )
					{
						// Select downwards
						for( int i = iLastTouchedBackup + 1; i <= iIndex; i++ )
							ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
					}
					
					// iLastTouchedBackup
					// ..
					// iSelAnchor
					// ..
					// >> iIndex <<
					else
					{
						// Unselect downwards
						for( int i = iLastTouchedBackup; i < iSelAnchor; i++ )
							ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );

						// Select downwards
						for( int i = iSelAnchor + 1; i <= iIndex; i++ )
							ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
					}
				}
				else // iIndex < iLastTouchedBackup
				{
					// iSelAnchor
					// ..
					// >> iIndex <<
					// ..
					// iLastTouchedBackup

					// Unselect upwards
					for( int i = iLastTouchedBackup; i > iIndex; i-- )
						ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
				}
			}

			// Above anchor?
			else if( iIndex < iSelAnchor )
			{
				if( iIndex < iLastTouchedBackup )
				{
					// >> iIndex <<
					// ..
					// iSelAnchor
					// ..
					// iLastTouchedBackup
					if( iIndex < iLastTouchedBackup )
					{
						// Unselect upwards
						for( int i = iLastTouchedBackup; i > iSelAnchor; i-- )
							ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );

						// Select upwards
						for( int i = iSelAnchor - 1; i >= iIndex; i-- )
							ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
					}
					
					// >> iIndex <<
					// ..
					// iLastTouchedBackup
					// ..
					// iSelAnchor
					else // iIndex < iLastTouchedBackup
					{
						// Select upwards
						for( int i = iLastTouchedBackup - 1; i >= iIndex; i-- )
							ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
					}
				}
				else // iIndex > iLastTouchedBackup
				{
					// iLastTouchedBackup
					// ..
					// >> iIndex <<
					// ..
					// iSelAnchor

					// Unselect downwards
					for( int i = iLastTouchedBackup; i < iIndex; i++ )
						ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
				}
			}
			
			// At anchor
			else // iIndex == iSelAnchor
			{
				if( iIndex < iLastTouchedBackup )
				{
					// iSelAnchor / >> iIndex <<
					// ..
					// iLastTouchedBackup
					
					// Unselect upwards
					for( int i = iLastTouchedBackup; i > iSelAnchor; i-- )
					{
						ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
					}
				}
				else // iIndex > iLastTouchedBackup
				{
					// iLastTouchedBackup
					// ..
					// iSelAnchor / >> iIndex <<

					// Unselect downwards
					for( int i = iLastTouchedBackup; i < iSelAnchor; i++ )
						ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
				}
			}
		}
		else if( bDragging )
		{
			static bool bMoveLock = false;
			
			if( bMoveLock ) return 0;
			bMoveLock = true;
			
			const int y = HIWORD( lp );
			const int diff = y - iDragStartY;
			if( abs( diff ) > iItemHeight / 2 )
			{
				iDragStartY += ( ( diff > 0 ) ? iItemHeight : -iItemHeight );
				playlist->MoveSelected( ( diff > 0 ) ? +1 : -1 );
			}
			
			bMoveLock = false;
		}
		return 0;
		
	case WM_LBUTTONDOWN:
		{
			static int iLastClicked = -1;
			static bool bLastClickNoneOrShift = true;
			
			
			SetFocus( hwnd ); // TODO monitor focus loss
			
			LVHITTESTINFO hittest;
			memset( &hittest, 0, sizeof( LVHITTESTINFO ) );
			GetCursorPos( &hittest.pt );
			ScreenToClient( hwnd, &hittest.pt );
			const int iIndex = ( int )ListView_HitTest( WindowPlaylist, &hittest );
			if( iIndex == -1 ) return 0;
			
			const bool bShift    = ( ( GetKeyState( VK_SHIFT   ) & 0x8000 ) != 0 );
			const bool bControl  = ( ( GetKeyState( VK_CONTROL ) & 0x8000 ) != 0 );
			const bool bAlt      = ( ( GetKeyState( VK_MENU    ) & 0x8000 ) != 0 );
			
			// [Shift] or [Shift]+[Ctrl]?
			if( bShift )
			{
				if( bAlt ) return 0;

				// Last click usable as selection anchor?
				if( !bLastClickNoneOrShift )
				{
					// Treat as normal click
					iSelAnchor = iIndex;
					iLastClicked = iIndex;

					ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVIS_SELECTED | LVIS_FOCUSED );
					ListView_SetItemState( WindowPlaylist, iIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
					
					bLiquidSelecting = true;
					bLastClickNoneOrShift = true;

					return 0;
				}

				if( iIndex != iLastClicked )
				{
					// Below anchor?
					if( iIndex > iSelAnchor )
					{
						// Below last click?
						if( iIndex > iLastClicked )
						{
							// Other side of anchor?
							if( iLastClicked < iSelAnchor )
							{
								// Unselect downwards
								for( int i = iLastClicked; i < iSelAnchor; i++ )
									ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );

								// Select downwards
								for( int i = iSelAnchor; i <= iIndex; i++ )
									ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
							}
							
							// Same side of anchor?
							else
							{
								// Select downwards
								for( int i = iLastClicked + 1; i <= iIndex; i++ )
									ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
							}
						}
						
						// Above last click?
						else // iIndex < iLastClicked
						{
							// Unselect upwards
							for( int i = iLastClicked; i > iIndex; i-- )
								ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
						}
					}

					// Above anchor?
					else if( iIndex < iSelAnchor )
					{
						// Above last clicked?
						if( iIndex < iLastClicked )
						{
							// Other side of anchor?
							if( iLastClicked > iSelAnchor )
							{
								// Unselect upwards
								for( int i = iLastClicked; i > iSelAnchor; i-- )
									ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );

								// Select upwards
								for( int i = iSelAnchor; i >= iIndex; i-- )
									ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
							}
							
							// Same side of anchor?
							else
							{
								// Select upwards
								for( int i = iLastClicked - 1; i >= iIndex; i-- )
									ListView_SetItemState( WindowPlaylist, i, LVIS_SELECTED, LVIS_SELECTED );
							}
						}
						
						// Below last clicked?
						else // iIndex > iLastClicked
						{
							// Unselect downwards
							for( int i = iLastClicked; i < iIndex; i++ )
								ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
						}
					}

					// At anchor
					else // iIndex == iSelAnchor
					{
						if( iLastClicked < iSelAnchor )
						{
							// Unselect downwards
							for( int i = iLastClicked; i < iIndex; i++ )
								ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
						}
						else if( iLastClicked > iSelAnchor )
						{
							// Unselect upwards
							for( int i = iLastClicked; i > iIndex; i-- )
								ListView_SetItemState( WindowPlaylist, i, 0, LVIS_SELECTED );
						}
					}
					
					iLastClicked = iIndex;
					
					ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVIS_FOCUSED );
					ListView_SetItemState( WindowPlaylist, iIndex, LVIS_FOCUSED, LVIS_FOCUSED );
				}
				
				bLastClickNoneOrShift = true;
			}

			// [Ctrl]?
			else if( bControl )
			{
				if( bAlt ) return 0;
				
				iLastTouched = iIndex;
				
				// Toggle selection
				const bool bSelected = ( ListView_GetItemState( WindowPlaylist, iIndex, LVIS_SELECTED ) == LVIS_SELECTED );
				ListView_SetItemState( WindowPlaylist, iIndex, bSelected ? 0 : LVIS_SELECTED, LVIS_SELECTED );
				
				ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVIS_FOCUSED );
				ListView_SetItemState( WindowPlaylist, iIndex, LVIS_FOCUSED, LVIS_FOCUSED );
				
				bLastClickNoneOrShift = false;
			}
			
			// [Alt]?
			else if( bAlt )
			{
				// Update item height
				RECT rc;
				GetClientRect( WindowPlaylist, &rc );
				const int iItemsPerPage = ListView_GetCountPerPage( WindowPlaylist );
				iItemHeight = ( rc.bottom - rc.top ) / iItemsPerPage;
				
				iDragStartY = hittest.pt.y;
				bDragging = true;
				
				bCapturing = true;
				SetCapture( WindowPlaylist );
			}
			
			// No modifiers?
			else
			{
				iSelAnchor = iIndex;
				iLastClicked = iIndex;

				ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVIS_SELECTED | LVIS_FOCUSED );
				ListView_SetItemState( WindowPlaylist, iIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
				
				bLiquidSelecting = true;
				bLastClickNoneOrShift = true;
				
				bCapturing = true;
				SetCapture( WindowPlaylist );
			}
		}
		return 0;

	case WM_LBUTTONUP:
		bLiquidSelecting  = false;
		bDragging         = false;
		
		bCapturing = false;
		ReleaseCapture();
		
		return 0;

	case WM_SYSKEYDOWN:
		switch( wp ) // [Alt]+[...]
		{
		case VK_UP:
			playlist->MoveSelected( -1 );
			break;

		case VK_DOWN:
			playlist->MoveSelected( +1 );
			break;

		}
		break;
		
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
			
			// Note: [Alt] goes to WM_SYSKEYDOWN instead
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
				if( bInfinitePlaylist )
				{
					// First item has focus?
					if( ListView_GetNextItem( WindowPlaylist, ( UINT )-1, LVNI_FOCUSED ) != 0 ) break;
					
					if( bControl && !bShift )
					{
						// Move caret only
						ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVNI_FOCUSED );
						ListView_SetItemState( WindowPlaylist, playlist->GetMaxIndex(), LVNI_FOCUSED, LVNI_FOCUSED );
						return 0;
					}
					else
					{
						// Move Caret and selection
						ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVNI_FOCUSED );
						ListView_SetItemState( WindowPlaylist, playlist->GetMaxIndex() - 1, LVNI_FOCUSED, LVNI_FOCUSED );
						wp = VK_DOWN;
					}
				}
				break;
	
			case VK_DOWN:
				if( bInfinitePlaylist )
				{
					// Last item has focus?
					if( ListView_GetNextItem( WindowPlaylist, playlist->GetMaxIndex() - 1, LVNI_FOCUSED ) == -1 ) break;
					
					if( bControl && !bShift )
					{
						// Move caret only
						ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVNI_FOCUSED );
						ListView_SetItemState( WindowPlaylist, 0, LVNI_FOCUSED, LVNI_FOCUSED );
						return 0;
					}
					else
					{
						// Workaround
						ListView_SetItemState( WindowPlaylist, ( UINT )-1, 0, LVNI_FOCUSED );
						ListView_SetItemState( WindowPlaylist, 1, LVNI_FOCUSED, LVNI_FOCUSED );
						wp = VK_UP;
					}
				}
				break;
	
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
				playlist->SetCurIndex( ListView_GetNextItem( WindowPlaylist, ( UINT )-1, LVIS_FOCUSED ) );
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
	/*			
			case 'J':
				if( bShift || bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, WINAMP_JUMPFILE, 0 );
				return 0;
	*/		
			case 'L':
				if( bControl ) return 0;
				SendMessage( WindowMain, WM_COMMAND, bShift ? WINAMP_FILE_DIR : WINAMP_FILE_PLAY, 0 );
				return 0;
			}
			break;
		}

	case WM_LBUTTONDBLCLK:
		// iCurIndex = Playlist_MouseToIndex();

		LVHITTESTINFO hittest;
		memset( &hittest, 0, sizeof( LVHITTESTINFO ) );
		GetCursorPos( &hittest.pt );
		ScreenToClient( hwnd, &hittest.pt );
		const int iIndex = ( int )ListView_HitTest( WindowPlaylist, &hittest );
		if( iIndex == -1 ) break;
		
		playlist->SetCurIndex( iIndex );
		
		Playback::Play();
		Playback::UpdateSeek();

		break;

	}

	return CallWindowProc( WndprocPlaylistBackup, hwnd, message, wp, lp );
}
