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


#include "VisCache.h"
#include "Console.h"
#include "malloc.h"



unsigned char * SpecCacheLeft;
unsigned char * SpecCacheRight;
unsigned char * WaveCacheLeft;
unsigned char * WaveCacheRight;

int iWritePos;
int iWriteOffset;	// == iWritePos * 576

int iVisLatency;
int iDataFps;
int iCacheLen;

bool bReady = false;

int iReadTimeMs = 0;
int iWriteTimeMs = 0;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache_Resize( int iLatency, int iFps )
{
	// if( !bReady ) return;

	const int iNewCacheLen = ( iFps * iLatency ) / 1000 + 1;
/*	
TCHAR szBuffer[ 5000 ];
_stprintf( szBuffer, TEXT( "RESIZE ( %i * %i ) / 1000 + 1 === %i" ), iFps, iLatency, iNewCacheLen );
Console::Append( szBuffer );
*/
	const int iByteNewCacheLen = iNewCacheLen * 576;
	if( !iCacheLen )
	{
		// First time
		SpecCacheLeft   = ( unsigned char * )malloc( iByteNewCacheLen );
//		                    memset( SpecCacheLeft, 0, iByteNewCacheLen );
		SpecCacheRight  = ( unsigned char * )malloc( iByteNewCacheLen );
//		                    memset( SpecCacheRight, 0, iByteNewCacheLen );
		WaveCacheLeft    = ( unsigned char * )malloc( iByteNewCacheLen );
//		                    memset( WaveCacheLeft, 0, iByteNewCacheLen );
		WaveCacheRight   = ( unsigned char * )malloc( iByteNewCacheLen );
//		                    memset( WaveCacheRight, 0, iByteNewCacheLen );
	}
	else if( iNewCacheLen > iCacheLen )
	{
		// Grow
		const int iByteCacheLen = iCacheLen * 576;
		const int iByteClearLen = ( iNewCacheLen - iCacheLen ) * 576;
	
		SpecCacheLeft   = ( unsigned char * )realloc( SpecCacheLeft, iByteNewCacheLen );
//		                    memset( SpecCacheLeft + iByteCacheLen, 0, iByteClearLen );
		SpecCacheRight  = ( unsigned char * )realloc( SpecCacheRight, iByteNewCacheLen );
//	                    memset( SpecCacheRight + iByteCacheLen, 0, iByteClearLen );
		WaveCacheLeft    = ( unsigned char * )realloc( WaveCacheLeft, iByteNewCacheLen );
//		                    memset( WaveCacheLeft + iByteCacheLen, 0, iByteClearLen );
		WaveCacheRight   = ( unsigned char * )realloc( WaveCacheRight, iByteNewCacheLen );
//		                    memset( WaveCacheRight + iByteCacheLen, 0, iByteClearLen );
	}

	iCacheLen  = iNewCacheLen;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::Create()
{
	if( bReady ) return;

	iWritePos     = 0;
	iWriteOffset  = 0;
	
	iVisLatency   = 50;
	iDataFps      = 40;
	iCacheLen     = 0;

	bReady = true;


	VisCache_Resize( iVisLatency, iDataFps );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::Destroy()
{
	if( !bReady ) return;

	if( SpecCacheLeft ) free( SpecCacheLeft );
	if( SpecCacheRight ) free( SpecCacheRight );
	if( WaveCacheLeft ) free( WaveCacheLeft );
	if( WaveCacheRight ) free( WaveCacheRight );

	bReady = false;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::EnsureLatency( int iLatency )
{
	if( !bReady ) return;
	
	if( iLatency <= iVisLatency ) return;

	VisCache_Resize(
		iLatency,
		iDataFps
	);
	iVisLatency = iLatency;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::EnsureDataFps( int iFps )
{
	if( !bReady ) return;
	if( iFps <= iDataFps ) return;

	VisCache_Resize(
		iVisLatency, 
		iFps
	);
	iDataFps = iFps;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::Clean()
{
	if( !bReady ) return;

	const int iByteCacheLen = iCacheLen * 576;
	memset( SpecCacheLeft,   0, iByteCacheLen );
	memset( SpecCacheRight,  0, iByteCacheLen );
	memset( WaveCacheLeft,   0, iByteCacheLen );
	memset( WaveCacheRight,  0, iByteCacheLen );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::SetReadTime( int ms )
{
	iReadTimeMs = ms;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::SetWriteTime( int ms )
{
	iWriteTimeMs = ms;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int VisCache::LatencyToOffset( int iLatency )
{
	int iFrame = iWritePos - 1 - ( ( iWriteTimeMs - iReadTimeMs - iLatency ) * iDataFps ) / 1000;
	if( iFrame < 0 ) iFrame += iCacheLen;
	return iFrame * 576;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::NextFrame()
{
	iWritePos++;
	if( iWritePos >= iCacheLen ) iWritePos = 0;
	iWriteOffset = iWritePos * 576;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::PutSpecLeft( unsigned char * data )
{
	if( !bReady ) return;
	memcpy( SpecCacheLeft + iWriteOffset, data, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::PutSpecRight( unsigned char * data )
{
	if( !bReady ) return;
	memcpy( SpecCacheRight + iWriteOffset, data, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::PutWaveLeft( unsigned char * data )
{
	if( !bReady ) return;
	memcpy( WaveCacheLeft + iWriteOffset, data, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::PutWaveRight( unsigned char * data )
{
	if( !bReady ) return;
	memcpy( WaveCacheRight + iWriteOffset, data, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::GetSpecLeft( unsigned char * dest, int iOffset )
{
	if( !bReady ) return;
	memcpy( dest, SpecCacheLeft + iOffset, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::GetSpecRight( unsigned char * dest, int iOffset )
{
	if( !bReady ) return;
	memcpy( dest, SpecCacheRight + iOffset, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::GetWaveLeft( unsigned char * dest, int iOffset )
{
	if( !bReady ) return;
	memcpy( dest, WaveCacheLeft + iOffset, 576 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void VisCache::GetWaveRight( unsigned char * dest, int iOffset )
{
	if( !bReady ) return;
	memcpy( dest, WaveCacheRight + iOffset, 576 );
}
