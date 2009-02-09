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


#include "Global.h"



int  dsp_isactive();
int  dsp_dosamples( short int * samples, int numsamples, int bps, int nch, int srate );
void SAVSAInit( int maxlatency_in_ms, int srate );
void SAVSADeInit();
void SAAddPCMData( void * PCMData, int nch, int bps, int timestamp );
int  SAGetMode();
void SAAdd(void * data, int timestamp, int csa );
void VSAAddPCMData( void * PCMData, int nch, int bps, int timestamp );
int  VSAGetMode( int * specNch, int * waveNch );
void VSAAdd( void * data, int timestamp );
void VSASetInfo( int nch, int srate );
void SetInfo( int bitrate, int srate, int stereo, int synched );
