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


#ifndef PA_VIS_CACHE_H
#define PA_VIS_CACHE_H


#include "Global.h"



namespace VisCache
{
	void Create();
	void Destroy();
	
	void EnsureLatency( int iLatency ); // TODO
	void EnsureDataFps( int iFps ); // TODO
	
	void Clean();
	
	void SetReadTime( int ms );
	void SetWriteTime( int ms );

	int LatencyToOffset( int iLatency );
	void NextFrame();

	void PutSpecLeft( unsigned char * data );
	void PutSpecRight( unsigned char * data );
	void PutWaveLeft( unsigned char * data );
	void PutWaveRight( unsigned char * data );
	
	void GetSpecLeft( unsigned char * dest, int iOffset );
	void GetSpecRight( unsigned char * dest, int iOffset );
	void GetWaveLeft( unsigned char * dest, int iOffset );
	void GetWaveRight( unsigned char * dest, int iOffset );
};



#endif // PA_VIS_CACHE_H
