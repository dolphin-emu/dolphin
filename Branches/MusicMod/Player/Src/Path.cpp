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


#include "Path.h"
#include <ctype.h>
#include <vector>

using namespace std;



////////////////////////////////////////////////////////////////////////////////
/// Applies a virtual root to a filename
///
/// Example:
///   Rootpath:         "C:\111\222\333\"	
///   Filename before:  "C:\111\444\test.mp3"
///   Filename after:   "..\..\444\test.mp3"
////////////////////////////////////////////////////////////////////////////////
bool ApplyRootToFilename( TCHAR * szRootpath, TCHAR * szFilename )
{
	// returns modified flag
	
	int iFilenameLen  = ( int )_tcslen( szFilename );
	int iRootLen      = ( int )_tcslen( szRootpath );
	
	// Too short?
	if( ( iRootLen < 2 ) || ( iFilenameLen < 4 ) )
	{
		return false;
	}

	// Ensure trailing backslash
	bool bDelOnRet = false;
	TCHAR * szFinalRoot;
	TCHAR * szFinalRootBackup;
	if( szRootpath[ iRootLen - 1 ] != TEXT( '\\' ) )
	{
		szFinalRoot = new TCHAR[ iRootLen + 2 ];
		memcpy( szFinalRoot, szRootpath, iRootLen * sizeof( TCHAR ) );
		memcpy( szFinalRoot + iRootLen, TEXT( "\\\0" ), 2 * sizeof( TCHAR ) );
		iRootLen++;
		szFinalRootBackup = szFinalRoot;
		bDelOnRet = true;
	}
	else
	{
		szFinalRoot = szRootpath;
		szFinalRootBackup = NULL;
		bDelOnRet = false;
	}


	// Different drives?
	if( _totlower( *szFilename ) != _totlower( *szFinalRoot ) )
	{
		if( bDelOnRet ) delete [] szFinalRootBackup;
		return false;
	}


	// Skip drive
	if( _tcsnicmp( szFilename, szFinalRoot, 3 ) )
	{
		szFinalRoot += 3;
		iRootLen -= 3;
		
		memmove( szFilename, szFilename + 3, ( iFilenameLen - 2 ) * sizeof( TCHAR ) ); // Plus \0
		iFilenameLen -=3;
	}


	int iBackslashLast = -1;
	
	int iCompLen; // Maximum chars to compare
	if( iRootLen > iFilenameLen )
		iCompLen = iFilenameLen;
	else
		iCompLen = iRootLen;
	

	// Walk while equal
	int i = 0;
	while( i < iCompLen )
	{
		if( ( szFilename[ i ] == TEXT( '\\' ) ) && ( szFinalRoot[ i ] == TEXT( '\\' ) ) )
		{
			iBackslashLast = i;
			i++;
		}
		else if( _totlower( szFilename[ i ] ) == _totlower( szFinalRoot[ i ] ) )
		{
			i++;
		}
		else
		{
			break;
		}
	}


	// Does the filename contain the full root?
	int iLevelDiff = 0;
	if( i != iCompLen )
	{
		// Calculate level difference
		for( i = iBackslashLast + 1; i < iRootLen; i++ )
		{
			if( szFinalRoot[ i ] == TEXT( '\\' ) )
			{
				iLevelDiff++;
			}
		}
	}

	
	if( iBackslashLast == -1 )
	{
		if( bDelOnRet ) delete [] szFinalRootBackup;
		return false;
	}


	TCHAR * szSource = szFilename + iBackslashLast + 1;
	if( iLevelDiff > 0 )
	{
		const int iExtraCharsForPrefix = ( 3 * iLevelDiff ) - iBackslashLast - 1;
		const int iCharsToMove = iFilenameLen - iBackslashLast; // One more for '\0'
		memmove( szSource + iExtraCharsForPrefix, szSource, sizeof( TCHAR ) * iCharsToMove );
		
		TCHAR * szWalk = szFilename;
		while( iLevelDiff-- )
		{
			memcpy( szWalk, TEXT( "..\\" ), 3 * sizeof( TCHAR ) );
			szWalk += 3;
		}
	}
	else
	{
		const int iCharsToMove = iFilenameLen - iBackslashLast; // One more for '\0'
		memmove( szFilename, szSource, sizeof( TCHAR ) * iCharsToMove );
	}


	if( bDelOnRet ) delete [] szFinalRootBackup;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
/// Compresses filenames (inplace)
///
/// Example:
///   Before  "C:\111\222\..\333\.\444\..\..\test.mp3"
///   After   "C:\111\test.mp3"
////////////////////////////////////////////////////////////////////////////////
bool UnbloatFilename( TCHAR * szFullpath, bool bFixTooDeep )
{
	int iLen = ( int )_tcslen( szFullpath );
	bool bModified = false;
	
	// Exclude drive letter from conversion "C:\"
	if( ( iLen > 3 ) && !_tcsnicmp( szFullpath + 1, TEXT( ":\\" ), 2 ) )
	{
		szFullpath += 3;
		iLen -= 3;
	}
	
	vector< TCHAR * > after_backslash;
	TCHAR * end = szFullpath + iLen;
	TCHAR * szWalk = szFullpath;
	
	while( true )
	{
		if( !_tcsnicmp( szWalk, TEXT( "..\\" ), 3 ) )
		{
			TCHAR * szAfterBackslashLast;
			
			if( after_backslash.empty() )
			{
				// Getting here means we go deeper than root level e.g. "../test"
				if( bFixTooDeep )
				{
					szAfterBackslashLast = szWalk;
				}
				else
				{
					break;
				}
			}
			else
			{
				szAfterBackslashLast = after_backslash.back();
				after_backslash.pop_back();
			}
			
			const int iBytesToCopy = end - szWalk - ( 3 * sizeof( TCHAR ) );
			const int iBytesLess = szWalk + ( 3 * sizeof( TCHAR ) ) - szAfterBackslashLast;
			
			memmove( szAfterBackslashLast, szWalk + 3, iBytesToCopy );
			
			char * byte_end = ( char * )end;
			byte_end -= iBytesLess;
			end = byte_end;
			*end = TEXT( '\0' );

			szWalk = szAfterBackslashLast;

			bModified = true;
		}
		else if( !_tcsnicmp( szWalk, TEXT( ".\\" ), 2 ) )
		{
			const int iBytesToCopy = end - szWalk - ( 2 * sizeof( TCHAR ) );
			memmove( szWalk, szWalk + 2, iBytesToCopy );
			end -= 2;
			*end = TEXT( '\0' );

			bModified = true;
		}
		else
		{
			
			if( szWalk >= end ) break;
			after_backslash.push_back( szWalk );
			
			// Jump after next backslash
			while( ( szWalk < end ) && ( *szWalk != TEXT( '\\' ) ) ) szWalk++;
			szWalk++;
		}
	}
	
	return bModified;
}
