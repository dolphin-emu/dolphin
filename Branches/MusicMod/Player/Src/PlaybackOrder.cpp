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



#include "Playback.h"
#include "Config.h"
#include <time.h>
#include <stdlib.h>



int iCurOrder = ORDER_DEFAULT;
ConfIntMinMax ciCurOrder( &iCurOrder, TEXT( "Order" ), CONF_MODE_INTERNAL, ORDER_DEFAULT, ORDER_FIRST, ORDER_LAST );

bool bRandomReady  = false;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playback::Order::GetCurMode()
{
	return iCurOrder;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Order::SetMode( int iMode )
{
	iCurOrder = iMode;
	ciCurOrder.MakeValidDefault();

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
TCHAR * Playback::Order::GetModeName( int iMode )
{
	switch( iMode )
	{
	case ORDER_SINGLE:          return TEXT( " Single" );
	case ORDER_SINGLE_REPEAT:   return TEXT( " Single + Repeat" );
	case ORDER_NORMAL:          return TEXT( " Normal" );
	case ORDER_NORMAL_REPEAT:   return TEXT( " Normal + Repeat" );
	case ORDER_INVERSE:         return TEXT( " Inverse" );
	case ORDER_INVERSE_REPEAT:  return TEXT( " Inverse + Repeat" );
	case ORDER_RANDOM:          return TEXT( " Random" );
	default: return NULL;
	}
}

/*
int Playback::Order::GetModeNameLen( int iMode )
{
	switch( uMode )
	{
	case ORDER_SINGLE:          return 7;
	case ORDER_SINGLE_REPEAT:   return 16;
	case ORDER_NORMAL:          return 7;
	case ORDER_NORMAL_REPEAT:   return 16;
	case ORDER_INVERSE:         return 8;
	case ORDER_INVERSE_REPEAT:  return 17;
	case ORDER_RANDOM:          return 7;
	default: return 0;
	}
}
*/


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool NextNormal( int & iCur, int iMax )
{
	if( iCur >= iMax ) return false;
	iCur++;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool NextNormalRepeat( int & iCur, int iMax )
{
	if( iCur >= iMax )
		iCur = 0;
	else
		iCur++;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool NextInverse( int & iCur, int iMax )
{
	if( iCur <= 0 ) return false;
	iCur--;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool NextInverseRepeat( int & iCur, int iMax )
{
	if( iCur <= 0 )
		iCur = iMax;
	else
		iCur--;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool NextRandom( int & iCur, int iMax )
{
	if( iMax < 2 ) return false;

	if( !bRandomReady )
	{
		srand( ( unsigned )time( NULL ) );
		bRandomReady = true;
	}

	const int iNew = ( int )( rand() / ( float )RAND_MAX * iMax );
	if( iNew != iCur )
		iCur = iNew;
	else
	{
		if( iCur >= iMax )
			iCur = 0;
		else
			iCur++;
	}
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Order::Next( int & iCur, int iMax )
{
	switch( iCurOrder )
	{
	case ORDER_SINGLE:          return false;
	case ORDER_SINGLE_REPEAT:   return true;
	case ORDER_NORMAL:          return NextNormal( iCur, iMax );
	case ORDER_NORMAL_REPEAT:   return NextNormalRepeat( iCur, iMax );
	case ORDER_INVERSE:         return NextInverse( iCur, iMax );
	case ORDER_INVERSE_REPEAT:  return NextInverseRepeat( iCur, iMax );
	case ORDER_RANDOM:          return NextRandom( iCur, iMax );
	default:                    return false;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Order::Prev( int & iCur, int iMax )
{
	switch( iCurOrder )
	{
	case ORDER_SINGLE:          return false;
	case ORDER_SINGLE_REPEAT:   return true;
	case ORDER_NORMAL:          return NextInverse( iCur, iMax );
	case ORDER_NORMAL_REPEAT:   return NextInverseRepeat( iCur, iMax );
	case ORDER_INVERSE:         return NextNormal( iCur, iMax );
	case ORDER_INVERSE_REPEAT:  return NextNormalRepeat( iCur, iMax );
	case ORDER_RANDOM:          return NextRandom( iCur, iMax );
	default:                    return false;
	}
}

