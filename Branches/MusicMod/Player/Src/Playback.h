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


#ifndef PA_PLAYBACK_H
#define PA_PLAYBACK_H



#include "Global.h"



#define ORDER_SINGLE           0
#define ORDER_SINGLE_REPEAT    1
#define ORDER_NORMAL           2
#define ORDER_NORMAL_REPEAT    3
#define ORDER_INVERSE          4
#define ORDER_INVERSE_REPEAT   5
#define ORDER_RANDOM           6

#define ORDER_FIRST            ORDER_SINGLE
#define ORDER_LAST             ORDER_RANDOM  

#define ORDER_DEFAULT          ORDER_NORMAL_REPEAT


#define TIMER_SEEK_UPDATE      1



typedef bool ( * PresetCallback )( TCHAR * );


namespace Playback
{
	bool Prev();
	bool Play();
	bool Pause();
	bool Stop();
	bool Next();

	bool IsPlaying();
	bool IsPaused();

	bool UpdateSeek();
	int  PercentToMs( float fPercent );
	bool SeekPercent( float fPercent );
	bool Forward();
	bool Rewind();

	void NotifyTrackEnd();

	namespace Volume
	{
		int  Get();
		bool Set( int iVol );
		bool Up();
		bool Down();
	};
	
	namespace Pan
	{
		int  Get();
		bool Set( int iPan );
	};
	
	namespace Order
	{
		int  GetCurMode();
		bool SetMode( int iMode );

		TCHAR * GetModeName( int iMode );
//		int GetModeNameLen( int iMode );

		bool Next( int & iCur, int iMax );
		bool Prev( int & iCur, int iMax );
	};
	
	namespace Eq
	{
		// 63 -> -12db
		// 31 ->   0
		// 0  -> +12db
		// bool Get( char * eq_data );
		// bool Set( bool bOn, char * pData, int iPreamp );
		int  GetCurIndex();
		bool SetIndex( int iPresetIndex );
		
		bool Reapply();
		
		bool ReadPresets( PresetCallback AddPreset );
	};
};



#endif // PA_PLAYBACK_H
