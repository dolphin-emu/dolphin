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


#ifndef PA_PLAYLIST_H
#define PA_PLAYLIST_H



#include "Global.h"
#include "PlaylistControler.h"
#include "PlaylistView.h"


#define PLAINAMP_PL_REM_SEL      50004
#define PLAINAMP_PL_REM_CROP     50005



extern HWND WindowPlaylist;

extern PlaylistControler * playlist;

namespace Playlist
{
	bool Create();
/*	
	int GetCurIndex();
	int GetMaxIndex();
	bool SetCurIndex( int iIndex );
*/
	TCHAR * GetFilename( int iIndex );
	
	int GetFilename( int iIndex, char * szAnsiFilename, int iChars );
	int GetTitle( int iIndex, char * szAnsiTitle, int iChars );

	bool DialogOpen();
	bool DialogSaveAs();

	bool AppendPlaylistFile( TCHAR * szFilename );
	bool ExportPlaylistFile( TCHAR * szFilename );
/*
	bool Append( TCHAR * szDisplay, TCHAR * szFilename );
	bool Add( int iIndex, TCHAR * szDisplay, TCHAR * szFilename );

	bool Clear(); // aka RemoveAll()
	bool RemoveSelected();
	bool Crop(); // aka RemoveUnselected
	
	bool SelectZero();
	bool SelectAll();
*/
};



#endif // PA_PLAYLIST_H
