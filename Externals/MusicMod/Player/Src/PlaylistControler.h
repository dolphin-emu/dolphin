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


#ifndef PLAYLIST_CONTROLER_H
#define PLAYLIST_CONTROLER_H 1

#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include "PlaylistModel.h"



// TODO: column width update on scrollbar show/hide

class PlaylistControler
{
	PlaylistModel _database;
	HWND _hView;
	
	bool _bZeroPadding;
	int _iDigits; // 3
	int _iDigitMin; // 100
	int _iDigitMax; // 999

private:
	bool FixDigitsMore();
	bool FixDigitsLess();
	void Refresh();
	void AutosizeColumns();
	
public:
	PlaylistControler( HWND hView, bool bEnableZeroPadding, int * piIndexSlave );

	void MoveSelected( int iDistance );

	int GetCurIndex();
	int GetMaxIndex();
	int GetSize();
	void SetCurIndex( int iIndex );

	void PushBack( TCHAR * szText );
	void Insert( int i, TCHAR * szText );
	void RemoveAll();
	void RemoveSelected( bool bPositive );
	void SelectAll( bool bPositive );
	void SelectInvert();

	const TCHAR * Get( int i );
	
	void Fill( LVITEM & request );
	void EnableZeroPadding( bool bActive );
	void Resize( HWND hParent );
};

#endif // PLAYLIST_CONTROLER_H
