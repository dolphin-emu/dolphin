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


#ifndef PLAYLIST_MODEL_H
#define PLAYLIST_MODEL_H 1

#include <windows.h>
#include <tchar.h>
#include <vector>
using namespace std;



class PlaylistModel
{
	vector<TCHAR *> _database;
	int _iCurIndex;
	int * _piCurIndex;

public:
	PlaylistModel()
	{
		_piCurIndex = &_iCurIndex;
	}

	PlaylistModel( int * piIndexSlave )
	{
		_piCurIndex = piIndexSlave;
	}
	
	void SetCurIndexSlave( int * piIndexSlave )
	{
		// *piIndexSlave = *_piCurIndex;
		_piCurIndex = piIndexSlave;
	}

	void PushBack( TCHAR * szText )
	{
		_database.push_back( szText );
	}
	
	void Insert( int i, TCHAR * szText )
	{
		if( i <= *_piCurIndex ) ( *_piCurIndex )++;
		_database.insert( _database.begin() + i, szText );
	}

	void Erase( int i )
	{
		if( i < *_piCurIndex ) ( *_piCurIndex )--;
		_database.erase( _database.begin() + i );
	}
	
	const TCHAR * Get( int i )
	{
		if( 0 > i || i >= ( int )_database.size() )
		{
			static const TCHAR * szError = TEXT( "INDEX OUT OF RANGE" );
			return szError;
		}

		return _database[ i ];
	}
	
	void Clear()
	{
		_database.clear();
		*_piCurIndex = -1;
	}
	
	int GetMaxIndex()
	{
		return _database.size() - 1;
	}
	
	int GetCurIndex()
	{
		return *_piCurIndex;
	}

	void SetCurIndex( int iIndex )
	{
		if( 0 > iIndex || iIndex >= ( int )_database.size() ) return;
		*_piCurIndex = iIndex;
	}

	int GetSize()
	{
		return _database.size();
	}
};


#endif // PLAYLIST_MODEL_H
