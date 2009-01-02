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


#include "PlaylistControler.h"
#include "Config.h"
#include "Font.h"

bool bPlaylistFollow;
ConfBool cbPlaylistFollow( &bPlaylistFollow, TEXT( "PlaylistFollow" ), CONF_MODE_PUBLIC, true );


void PlaylistControler::MoveSelected( int iDistance )
{
	if( iDistance == -1 )
	{
		if( ListView_GetItemState( _hView, 0, LVIS_SELECTED ) )
		{
			// Cannot move upwards
			return;
		}
	}
	else if( iDistance == 1 )
	{
		if( ListView_GetItemState( _hView, _database.GetMaxIndex(), LVIS_SELECTED ) )
		{
			// Cannot move downwards
			return;
		}
	}
	else
	{
		// More distance maybe later
		return;
	}
	
	const int iFocus = ListView_GetNextItem( _hView, ( UINT )-1, LVIS_FOCUSED );
	
	// Negative is to the top
	LRESULT iBefore = 0;
	LRESULT iAfter = -2; // Extra value to check after big for-loop
	
	for( ; ; )
	{
		// Count 
		LRESULT iAfter = ListView_GetNextItem( _hView, iBefore - 1, LVNI_SELECTED );
		if( iAfter == -1 ) break; // No more selections selected
		
		LRESULT iFirst = iAfter;

		// Search end of selection block
		iBefore = iAfter + 1;
		for( ; ; )
		{
			iAfter = ListView_GetNextItem( _hView, iBefore - 1, LVNI_SELECTED );
			if( iAfter == iBefore )
			{
				// Keep searching
				iBefore++;
			}
			else
			{
				// End found (iBefore is the first not selected)
				const int iSelSize = iBefore - iFirst;
				if( iDistance == -1 )
				{
					// Updwards
					const int iOldIndex = iFirst - 1;
					const int iNewIndex = iOldIndex + iSelSize;
				
					TCHAR * szData = ( TCHAR * )_database.Get( iOldIndex );
					_database.Erase( iOldIndex );
					_database.Insert( iNewIndex, szData );
					
					ListView_SetItemState( _hView, iOldIndex, LVIS_SELECTED, LVIS_SELECTED );
					ListView_SetItemState( _hView, iNewIndex, 0, LVIS_SELECTED );
					ListView_RedrawItems( _hView, iOldIndex, iNewIndex );
				}
				else
				{
					// Downwards
					const int iOldIndex = iFirst + iSelSize;
					const int iNewIndex = iFirst;
					
					TCHAR * szData = ( TCHAR * )_database.Get( iOldIndex );
					_database.Erase( iOldIndex );
					_database.Insert( iNewIndex, szData );

					ListView_SetItemState( _hView, iOldIndex, LVIS_SELECTED, LVIS_SELECTED );
					ListView_SetItemState( _hView, iNewIndex, 0, LVIS_SELECTED );
					ListView_RedrawItems( _hView, iNewIndex, iOldIndex );
				}
				
				iBefore++;
				break;
			}
		}
	}
	
	if( iAfter != -2 ) return; // Nothing was selected so nothing was moved
	ListView_SetItemState( _hView, iFocus + iDistance, LVIS_FOCUSED, LVIS_FOCUSED );
	
	Refresh();
}

int PlaylistControler::GetCurIndex()
{
	return _database.GetCurIndex();
}

int PlaylistControler::GetMaxIndex()
{
	return _database.GetMaxIndex();
}

int PlaylistControler::GetSize()
{
	return _database.GetSize();
}

void PlaylistControler::SetCurIndex( int iIndex )
{
	const int iCurIndexBefore = _database.GetCurIndex();
	_database.SetCurIndex( iIndex );

	if( bPlaylistFollow )
	{
		ListView_SetItemState( _hView, ( UINT )-1, 0, LVIS_SELECTED | LVIS_FOCUSED );
		ListView_SetItemState( _hView, iIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED );
	}
	
	if( iCurIndexBefore != -1 )
		ListView_RedrawItems( _hView, iCurIndexBefore, iCurIndexBefore );
	ListView_RedrawItems( _hView, iIndex, iIndex );
}

// Returns <true> on digit count change
bool PlaylistControler::FixDigitsMore()
{
	const int iCountAfter = _database.GetSize();
	const int iDigitsBefore = _iDigits;
	while( iCountAfter > _iDigitMax )
	{
		_iDigitMin *= 10;                  // 10 -> 100
		_iDigitMax = _iDigitMax * 10 + 9;  // 99 -> 999
		_iDigits++;                        //  2 ->   3
	}
	
	
	return ( ( _iDigits != iDigitsBefore )
		|| ( iCountAfter == 1 ) ); // Force update when first item is inserted
}

// Returns <true> on digit count change
bool PlaylistControler::FixDigitsLess()
{
	const int iCountAfter = _database.GetSize();
	const int iDigitsBefore = _iDigits;
	while( ( iCountAfter < _iDigitMin ) && ( _iDigits > 1 ) )
	{
		_iDigitMin /= 10;  // 999 -> 99
		_iDigitMax /= 10;  // 100 -> 10
		_iDigits--;        //   3 ->  2
	}

	return ( _iDigits != iDigitsBefore );
}


void PlaylistControler::Refresh()
{
	AutosizeColumns();
	
	const int iFirst  = ListView_GetTopIndex( _hView );
	const int iLast   = iFirst + ListView_GetCountPerPage( _hView );
	
	ListView_RedrawItems( _hView, iFirst, iLast );
}

PlaylistControler::PlaylistControler( HWND hView, bool bEnableZeroPadding, int * piIndexSlave )
{
	_hView = hView;
	
	_bZeroPadding = bEnableZeroPadding;
	_iDigits    = 1;
	_iDigitMin  = 1;
	_iDigitMax  = 9;
	
	_database.SetCurIndexSlave( piIndexSlave );
	
	Refresh();
	
	// TODO clear list view here???
}


void PlaylistControler::PushBack( TCHAR * szText )
{
	const int iSize = _database.GetMaxIndex();

	_database.PushBack( szText );
	ListView_SetItemCount( _hView, _database.GetSize() );
	
	if( FixDigitsMore() ) Refresh();
}

void PlaylistControler::Insert( int i, TCHAR * szText )
{
	const int iSize = _database.GetMaxIndex();

	_database.Insert( i, szText );
	ListView_SetItemCount( _hView, _database.GetSize() );
	
	if( FixDigitsMore() ) Refresh();
}

void PlaylistControler::RemoveAll()
{
	_database.Clear();
	ListView_DeleteAllItems( _hView );
	
	if( FixDigitsLess() ) Refresh();
}

void PlaylistControler::RemoveSelected( bool bPositive )
{
	SendMessage( _hView, WM_SETREDRAW, FALSE, 0 );

	if( bPositive )
	{
		LRESULT iWalk = 0;
		for( ; ; )
		{
			// MSDN: The specified item itself is excluded from the search.
			iWalk = ListView_GetNextItem( _hView, iWalk - 1, LVNI_SELECTED );
			if( iWalk != -1 )
			{
				_database.Erase( iWalk );
				ListView_DeleteItem( _hView, iWalk );
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		LRESULT iBefore = 0;
		LRESULT iAfter = 0;
		for( ; ; )
		{
			// MSDN: The specified item itself is excluded from the search.
			iAfter = ListView_GetNextItem( _hView, iBefore - 1, LVNI_SELECTED );
			if( iAfter != -1 )
			{
				// <iAfter> is the first selected
				// so we delete all before and restart
				// at the beginning
				const int iDelIndex = iBefore;
				for( int i = iBefore; i < iAfter; i++ )
				{
					_database.Erase( iDelIndex );
					ListView_DeleteItem( _hView, iDelIndex );
				}
				
				iBefore++; // Exclude the one we found selected right before
			}
			else
			{
				if( iBefore == 0 )
				{
					// All selected
					_database.Clear();
					ListView_DeleteAllItems( _hView );
				}
				else
				{
					const int iSize = _database.GetSize();
					const int iDelIndex = iBefore;
					for( int i = iBefore; i < iSize; i++ )
					{
						_database.Erase( iDelIndex );
						ListView_DeleteItem( _hView, iDelIndex );
					}
				}

				break;
			}
		}
	}

	bool bRefresh = FixDigitsLess();
	
	SendMessage( _hView, WM_SETREDRAW, TRUE, 0 );
	
	if( bRefresh ) Refresh();
}

void PlaylistControler::SelectAll( bool bPositive )
{
	ListView_SetItemState( _hView, ( UINT )-1, bPositive ? LVIS_SELECTED : 0, LVIS_SELECTED );
}

void PlaylistControler::SelectInvert()
{
	SendMessage( _hView, WM_SETREDRAW, FALSE, 0 );

	const int iOneTooMuch = _database.GetSize();
	for( int i = 0; i < iOneTooMuch; i++ )
	{
		ListView_SetItemState(
			_hView,
			i,
			( ListView_GetItemState( _hView, i, LVIS_SELECTED ) == LVIS_SELECTED )
				? 0
				: LVIS_SELECTED,
			LVIS_SELECTED
		);

	}
	
	SendMessage( _hView, WM_SETREDRAW, TRUE, 0 );
}

const TCHAR * PlaylistControler::Get( int i )
{
	return _database.Get( i );
}

void PlaylistControler::Fill( LVITEM & request )
{
	if( ( request.mask & LVIF_TEXT ) == 0 ) return;
	if( request.iSubItem )
	{
		// Text
		_sntprintf( request.pszText, request.cchTextMax, TEXT( "%s" ), _database.Get( request.iItem ) );
	}
	else
	{
		// Number
		if( _bZeroPadding )
		{
			TCHAR szFormat[ 6 ];
			_stprintf( szFormat, TEXT( "%%0%dd" ), _iDigits );
			_sntprintf( request.pszText, request.cchTextMax, szFormat, request.iItem + 1 );
		}
		else
		{
			_sntprintf( request.pszText, request.cchTextMax, TEXT( "%d" ), request.iItem + 1 );
		}
	}
}

void PlaylistControler::EnableZeroPadding( bool bActive )
{
	if( bActive == _bZeroPadding ) return;

	LVCOLUMN col;
	memset( &col, 0, sizeof( LVCOLUMN ) );
	col.mask = LVCF_FMT;
	
	if( bActive )
	{
		// TODO recalculation yes/no?
		/*
		int iSize = _database.GetSize();
		if( iSize != 0 )
		{
			int iDigits = 0;
			while( iSize > 0 )
			{
				iSize /= 10;
				iDigits++;
			}
			
			_iDigits = iSize;
			_iDigitMin = 1;
			_iDigitMax = 9;
			while( iSize-- > 0 )
			{
				_iDigitMin *= 10;
				_iDigitMax = _iDigitMax * 10 + 9;
			}
		}
		else
		{
			_iDigits = 1;
			_iDigitMin = 1;
			_iDigitMax = 9;
		}
		*/
		
		col.fmt = LVCFMT_LEFT;
		ListView_SetColumn( _hView, 0, &col );
		
		Refresh();
	}
	else
	{
		col.fmt = LVCFMT_RIGHT;
		ListView_SetColumn( _hView, 0, &col );

		Refresh();
	}
	_bZeroPadding = bActive;
}

void PlaylistControler::AutosizeColumns()
{
	RECT r;
	if( !GetClientRect( _hView, &r ) ) return;
	
	if( _bZeroPadding )
	{
		ListView_SetColumnWidth( _hView, 0, LVSCW_AUTOSIZE );
		const int iWidth = ListView_GetColumnWidth( _hView, 0 );
		ListView_SetColumnWidth( _hView, 1, r.right - r.left - iWidth );
	}
	else
	{
		HDC hdc = GetDC( _hView );
		const HFONT hOldFont = ( HFONT )SelectObject( hdc, Font::Get() );
		SIZE size;
		BOOL res = GetTextExtentPoint32( hdc, TEXT( "0" ), 1, &size );
		SelectObject( hdc, hOldFont );
		ReleaseDC( _hView, hdc );
		const int iWidth = res ? ( int )( size.cx * ( _iDigits + 0.25f ) ) : 120;
		ListView_SetColumnWidth( _hView, 0, iWidth );
		ListView_SetColumnWidth( _hView, 1, r.right - r.left - iWidth );
	}
}

void PlaylistControler::Resize( HWND hParent )
{
	/*
	RECT rc;
	GetClientRect( hParent, &rc );
	MoveWindow( _hView, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE );
	*/

	AutosizeColumns();
}
