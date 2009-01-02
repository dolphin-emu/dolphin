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
#include "InputPlugin.h"
#include "Console.h"
#include "Config.h"
#include <vector>

using namespace std;



vector <char *> eq_vec;



// Note:  CurPresetFixed will be renamed to CurPreset when
//        custom presets are implemented. This hopefully avoids
//        migration trouble.

int iCurPreset; // -1 means EQ off
ConfInt ciCurPreset( &iCurPreset, TEXT( "CurPresetFixed" ), CONF_MODE_INTERNAL, -1 );

bool bPreventDistortion; // Automatic preamp adjustment
ConfBool cbPreventDistortion( &bPreventDistortion, TEXT( "PreventDistortion" ), CONF_MODE_PUBLIC, true );



////////////////////////////////////////////////////////////////////////////////
// PRE valid index
////////////////////////////////////////////////////////////////////////////////
void Playback_Eq_Set( int iPresetIndex )
{
	if( active_input_plugin && active_input_plugin->plugin )
	{
		if( iPresetIndex == -1 ) // == EQ disabled
		{
			char data[ 10 ] = { 31, 31, 31, 31, 31, 31, 31, 31, 31, 31 };
			active_input_plugin->plugin->EQSet( 0, data, 31 );
		}
		else
		{
			char data[ 10 ];
			memcpy( data, eq_vec[ iPresetIndex ], sizeof( char ) * 10 );
			
			if( bPreventDistortion )
			{
				// Search minimum (most amplifying band)
				int iMin = 63;
				int i;
				for( i = 0; i < 10; i++ )
				{
					if( data[ i ] < iMin ) iMin = data[ i ];
				}
				
				if( iMin < 31 ) // Possible distortion
				{
					// Adjust preamp to prevent distortion
					active_input_plugin->plugin->EQSet( 1, data, 31 + ( 31 - iMin ) );
				}
				else
				{
					if( iMin > 31 ) // Lower than necessary
					{
						// Push to zero level so we get
						// more volume without distortion
						const int iSub = iMin - 31;
						for( i = 0; i < 10; i++ )
						{
							data[ i ] -= iSub;
						}
					}
					active_input_plugin->plugin->EQSet( 1, data, 31 );
				}
			}
			else
			{
				active_input_plugin->plugin->EQSet( 1, data, 31 );
			}
		}
	}
}



/*
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Eq::Set( bool bOn, char * pData, int iPreamp )
{
	if( active_input_plugin && active_input_plugin->plugin )
	{
		char data[ 10 ];
		memcpy( data, pData, sizeof( char ) * 10 );
		active_input_plugin->plugin->EQSet( bOn ? 1 : 0, data, iPreamp );
	}
	return true;
}
*/



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playback::Eq::GetCurIndex()
{
	return iCurPreset;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Eq::SetIndex( int iPresetIndex )
{
	if( iPresetIndex >= ( int )eq_vec.size() ) return false;
	Playback_Eq_Set( iPresetIndex );
	iCurPreset = iPresetIndex;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Eq::Reapply()
{
	Playback_Eq_Set( iCurPreset );
	return true;
}



// Note:  Most of these are the exact presets used in Winamp.
//        I do not expect any legal trouble with this but
//        in case I am wrong please let me know and I will
//        remove them.

char eq_classical       [ 10 ] = { 31, 31, 31, 31, 31, 31, 44, 44, 44, 48 };
char eq_club            [ 10 ] = { 31, 31, 26, 22, 22, 22, 26, 31, 31, 31 };
char eq_dance           [ 10 ] = { 16, 20, 28, 32, 32, 42, 44, 44, 32, 32 };
char eq_full_bass       [ 10 ] = { 16, 16, 16, 22, 29, 39, 46, 49, 50, 50 };
char eq_full_bass_treble[ 10 ] = { 20, 22, 31, 44, 40, 29, 18, 14, 12, 12 };
char eq_full_treble     [ 10 ] = { 48, 48, 48, 39, 27, 14,  6,  6,  6,  4 };
char eq_headphones      [ 10 ] = { 24, 14, 23, 38, 36, 29, 24, 16, 11,  8 };
char eq_laptop          [ 10 ] = { 24, 14, 23, 38, 36, 29, 24, 16, 11,  8 };
char eq_large_hall      [ 10 ] = { 15, 15, 22, 22, 31, 40, 40, 40, 31, 31 };
char eq_live            [ 10 ] = { 40, 31, 25, 23, 22, 22, 25, 27, 27, 28 };
char eq_more_bass       [ 10 ] = { 22, 22, 22, 22, 22, 22, 26, 31, 31, 31 };
char eq_party           [ 10 ] = { 20, 20, 31, 31, 31, 31, 31, 31, 20, 20 };
char eq_pop             [ 10 ] = { 35, 24, 20, 19, 23, 34, 36, 36, 35, 35 };
char eq_reggae          [ 10 ] = { 31, 31, 33, 42, 31, 21, 21, 31, 31, 31 };
char eq_rock            [ 10 ] = { 19, 24, 41, 45, 38, 25, 17, 14, 14, 14 };
char eq_ska             [ 10 ] = { 36, 40, 39, 33, 25, 22, 17, 16, 14, 16 };
char eq_soft            [ 10 ] = { 24, 29, 34, 36, 34, 25, 18, 16, 14, 12 };
char eq_soft_rock       [ 10 ] = { 25, 25, 28, 33, 39, 41, 38, 33, 27, 17 };
char eq_techno          [ 10 ] = { 19, 22, 31, 41, 40, 31, 19, 16, 16, 17 };



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Eq::ReadPresets( PresetCallback AddPreset )
{
	if( AddPreset == NULL ) return false;

	eq_vec.push_back( eq_classical );         AddPreset( TEXT( "Classical" ) );
	eq_vec.push_back( eq_club );              AddPreset( TEXT( "Club" ) );
	eq_vec.push_back( eq_dance );             AddPreset( TEXT( "Dance" ) );
	eq_vec.push_back( eq_full_bass );         AddPreset( TEXT( "Full Bass" ) );
	eq_vec.push_back( eq_full_bass_treble );  AddPreset( TEXT( "Full Bass & Treble" ) );
	eq_vec.push_back( eq_full_treble );       AddPreset( TEXT( "Full Treble" ) );
	eq_vec.push_back( eq_headphones );        AddPreset( TEXT( "Headphones" ) );
	eq_vec.push_back( eq_laptop );            AddPreset( TEXT( "Laptop Speakers" ) );
	eq_vec.push_back( eq_large_hall );        AddPreset( TEXT( "Large Hall" ) );
	eq_vec.push_back( eq_live );              AddPreset( TEXT( "Live" ) );
	eq_vec.push_back( eq_more_bass );         AddPreset( TEXT( "More Bass" ) );
	eq_vec.push_back( eq_party );             AddPreset( TEXT( "Party" ) );
	eq_vec.push_back( eq_pop );               AddPreset( TEXT( "Pop" ) );
	eq_vec.push_back( eq_reggae );            AddPreset( TEXT( "Reggae" ) );
	eq_vec.push_back( eq_rock );              AddPreset( TEXT( "Rock" ) );
	eq_vec.push_back( eq_ska );               AddPreset( TEXT( "Ska" ) );
	eq_vec.push_back( eq_soft );              AddPreset( TEXT( "Soft" ) );
	eq_vec.push_back( eq_soft_rock );         AddPreset( TEXT( "Soft Rock" ) );
	eq_vec.push_back( eq_techno );            AddPreset( TEXT( "Techno" ) );
	
	// Fix invalid indices
	if( iCurPreset < -1 )
	{
		iCurPreset = -1;
	}
	else
	{
		const int iLen = ( int )eq_vec.size();
		if( iCurPreset >= iLen )
		{
			iCurPreset = iLen - 1;
		}
	}
	
	// TODO load/save eqf files
	//   GPL eqf loading from koders.com
	//   equalizer.c / xmms2 A Gtk2 port of xmms.(xmms2)
	//   equalizer.c / Digital Disco System(dds) 

	return true;
}
