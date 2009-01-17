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
#include "Output.h"
#include "Playlist.h"
#include "Console.h"
#include "Unicode.h"
#include "Rebar.h"
#include "Main.h"
#include "Config.h"
#include "Status.h"



int iCurVol = 255;
ConfIntMinMax ciCurVol( &iCurVol, TEXT( "Volume" ), CONF_MODE_INTERNAL, 255, 0, 255 );

int iCurPan = 0;
ConfIntMinMax ciCurPan( &iCurPan, TEXT( "Panning" ), CONF_MODE_INTERNAL, 0, -127, 127 );



#define VOLUME_STEP ( 255 / 10 )



bool bPlaying  = false;
bool bPaused   = false;

bool bTimerRunning = false;



// Only for reference comparison!!!
TCHAR * szCurrentFilename = NULL;



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
// =======================================================================================
// The timer that calls WndprocMain every second
void EnableTimer( bool bEnabled )
{
	// Return if the timer is already activated
	if( bEnabled == bTimerRunning ) return;
	
	if( bEnabled )
	{
		SetTimer( WindowMain, TIMER_SEEK_UPDATE, 1000, NULL );
		Console::Print( "EnableTimer > Activated\n" );
	}
	else
	{
		KillTimer( WindowMain, TIMER_SEEK_UPDATE );
		StatusReset();

		Console::Print( "EnableTimer > Killed\n" );
	}
	
	bTimerRunning = bEnabled;
}
// =======================================================================================


////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
bool OpenPlay( TCHAR * szFilename, int iNumber )
{
	// =======================================================================================
	#ifdef NOGUI
		//Console::Print( "Playback.cpp: OpenPlay > Begin <%i> <%s>\n" , iNumber, szFilename );
	#else
		TCHAR sszBuffer[ 5000 ];
		_stprintf( sszBuffer, TEXT( "Playback.cpp: OpenPlay was called <%i> <%s>" ), iNumber, szFilename );
		Console::Append( sszBuffer ); 
	#endif
	// =======================================================================================


	// =======================================================================================
	// Get the right input plugin
	// ---------------------------------------------------------------------------------------
	if( !szFilename ) return false;
	szCurrentFilename = szFilename;

	// ---------------------------------------------------------------------------------------
	// TODO: non-file support
	// ---------------------------------------------------------------------------------------
	// Get file extension
	const int iLen = ( int )_tcslen( szFilename );
	TCHAR * walk = szFilename + iLen - 1;
	while( ( walk >= szFilename ) && ( *walk != TEXT( '.' ) ) ) walk--;
	walk++;

	// ---------------------------------------------------------------------------------------
	const int iExtLen = ( int )_tcslen( walk );
	TCHAR * szExt = new TCHAR[ iExtLen + 1 ];
	memcpy( szExt, walk, iExtLen * sizeof( TCHAR ) );
	szExt[ iExtLen ] = TEXT( '\0' );

	// ---------------------------------------------------------------------------------------
	// Compare the extension to the supported extension by the current input plugins
	map <TCHAR *, InputPlugin *, TextCompare>::iterator iter =
		ext_map.find( szExt );
	delete [] szExt;
	if( iter == ext_map.end() )
	{
		Console::Append( TEXT( "ERROR: Extension not supported" ) );
		Console::Append( " " );
		Console::Print("OpenPlay > ERROR: Extension not supported\n");
		return false;
	}	
	// ---------------------------------------------------------------------------------------
	// Now that we know which input pugin to use we set that one as active
	InputPlugin * old_input = active_input_plugin; // Save the last one, if any
	active_input_plugin = iter->second;
	Console::Print("OpenPlay > Input plugin '%s' activated\n", active_input_plugin->GetFilename());
	// =======================================================================================

	if( old_input )
	{
		// if( active_input_plugin != old_input ) ----> TODO unload old plugin
		
		// Some output plugins require a call to Close() before each
		// call to Open(). Calling Input::Stop() will make the input plugin
		// call Output::Close() and thus solve this problem.
		old_input->plugin->Stop();
	}

	if( !active_input_plugin->plugin )
	{
		Console::Append( TEXT( "ERROR: Input plugin is NULL" ) );
		Console::Append( " " );
		Console::Print("OpenPlay > ERROR: Input plugin is NULL\n");
		return false;
	}
	
	// Connect
	//Console::Print( "OpenPlay > OutMod\n" );
	active_input_plugin->plugin->outMod = &output_server; // output->plugin;
	
	// =======================================================================================
	// Re-apply volume and panning
	//active_input_plugin->plugin->SetVolume( iCurVol );
	//active_input_plugin->plugin->SetPan( iCurPan );
	//Playback::Eq::Reapply();
	// =======================================================================================

	// Play

#ifdef PA_UNICODE
	// Filename
	const int iFilenameLen = _tcslen( szFilename );
	char * szTemp = new char[ iFilenameLen + 1 ];
	ToAnsi( szTemp, szFilename, iFilenameLen );
	szTemp[ iFilenameLen ] = '\0';
	
	// Ansi Title
	char szAnsiTitle[ 2000 ] = "\0";
	int length_in_ms;
	active_input_plugin->plugin->GetFileInfo( szTemp, szAnsiTitle, &length_in_ms );
	const int iAnsiTitleLen = strlen( szAnsiTitle );

	// Unicode title
	TCHAR szTitle[ 2000 ];
	ToTchar( szTitle, szAnsiTitle, iFilenameLen, iAnsiTitleLen );
	szTitle[ iAnsiTitleLen ] = TEXT( "\0" );

	active_input_plugin->plugin->Play( szTemp );
	delete [] szTemp;
#else
	// =======================================================================================
	// Play the file
	// ---------------------------------------------------------------------------------------
	// Title
	TCHAR szTitle[ 2000 ] = TEXT( "\0" );
	int length_in_ms;

	//Console::Print( "OpenPlay > GetFileInfo\n" );
	active_input_plugin->plugin->GetFileInfo( szFilename, szTitle, &length_in_ms );

	//Console::Print( "OpenPlay > Play\n" );
	active_input_plugin->plugin->Play( szFilename );
	// =======================================================================================
#endif

	bPlaying  = true;
	bPaused   = false;

	// =======================================================================================
	// Title

	//TCHAR szBuffer[ 5000 ];
	//_stprintf( szBuffer, TEXT( "%i. %s - Plainamp" ), iNumber, szTitle );
	//SetWindowText( WindowMain, szBuffer );
	// =======================================================================================

	/*
	TCHAR * szBasename = szFilename + uLen - 1;
	while( ( szBasename > szFilename ) && ( *szBasename != TEXT( '\\' ) ) ) szBasename--;
	szBasename++;
	*/	

	// Timer ON
	//EnableTimer( true );

	//Console::Print( "OpenPlay > End\n" );

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback_PrevOrNext( bool bPrevOrNext )
{
	// todo: prev/next in pause mode?
	
	if( !active_input_plugin ) return false;
	if( !active_input_plugin->plugin ) return false;

	int iNextIndex  = playlist->GetCurIndex();
	int iMaxIndex   = playlist->GetMaxIndex();
	if( iMaxIndex < 0 || iNextIndex < 0 ) return false;
	
	bool res;
	if( bPrevOrNext )
		res = Playback::Order::Prev( iNextIndex, iMaxIndex );
	else
		res = Playback::Order::Next( iNextIndex, iMaxIndex );
	
	if(	res )
	{
		if( bPlaying )
		{
			// NOT TWICE active_input_plugin->plugin->Stop();
			bPlaying  = false;
			bPaused   = false;
			
			// Timer OFF
			EnableTimer( false );
		}

		TCHAR * szFilename = Playlist::GetFilename( iNextIndex );
		if( !szFilename ) return false;

		playlist->SetCurIndex( iNextIndex );
	
		return OpenPlay( szFilename, iNextIndex + 1 );
	}
	else
	{
		return false;
	}
	return true;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Prev()
{
	return Playback_PrevOrNext( true );
}



////////////////////////////////////////////////////////////////////////////////
// Play the file
////////////////////////////////////////////////////////////////////////////////
bool Playback::Play()
{
	static int iLastIndex = -1;

	// ---------------------------------------------------------------------------------------
	
	#ifndef NOGUI
 		TCHAR sszBuffer[ 5000 ];
		_stprintf( sszBuffer, TEXT( "Playback::Play() with bPlaying <%i>\n" ), bPlaying );
		Console::Append( sszBuffer );
		Console::Append( TEXT( " " ) );
	#else
		//Console::Print( "Playback::Play() > Begin <%i>\n" , bPlaying );
	#endif
	// ---------------------------------------------------------------------------------------

	// ---------------------------------------------------------------------------------------
	if( bPlaying ) // If we are currently playing a file
	{
		if( !active_input_plugin ) return false;
		if( !active_input_plugin->plugin ) return false;

		const int iIndex = playlist->GetCurIndex();
		if( iIndex < 0 ) return false;

		/*
		TCHAR szBuffer[ 5000 ];
		_stprintf( szBuffer, TEXT( "OLD [%i] NEW [%i]" ), iLastIndex, iIndex );
		SetWindowText( WindowMain, szBuffer );
		*/

		// If we are not playing the same track/file as before
		TCHAR * szFilename = Playlist::GetFilename( iIndex );
		if( szFilename != szCurrentFilename )
		{
			// New track!
			
			// Stop
			// NOT TWICE active_input_plugin->plugin->Stop();
			
			// Timer OFF
			//EnableTimer( false );
			
			// Get filename
			if( !szFilename )
			{
				Console::Append( TEXT( "ERROR: Could not resolve filename" ) );
				Console::Append( " " );
				return false;
			}
	
			// Play
			iLastIndex = iIndex;
			bPlaying  = OpenPlay( szFilename, iIndex + 1 );
			bPaused   = false;
		}
		else
		{
			// Same track!
			if( bPaused )
			{
				// Unpause
				active_input_plugin->plugin->UnPause();
				bPaused = false;
				
				// Timer ON
				//EnableTimer( true );
			}
			else
			{
				// Restart at beginning
				active_input_plugin->plugin->SetOutputTime( 0 );
			}
		}
	}
	// =======================================================================================
	else // we are not currently playing
	{
		const int iIndex = playlist->GetCurIndex();
		if( iIndex < 0 ) return false;

		// Get filename
		TCHAR * szFilename = Playlist::GetFilename( iIndex );

		// =======================================================================================
		

		//bool bPlaying  = false;
		//TCHAR * szFilename = TEXT("C:\Files\Spel och spelfusk\Console\Gamecube\Code\vgmstream (isolate ast)\Music\demo36_02.ast");
		//bPlaying  = OpenPlay( szFilename, 1 );
			//bPlaying  = OpenPlay( szFilename, iIndex + 1 );
		//Console::Append( TEXT( "Playback.cpp:Playback::Play() called OpenPlay" ) );	

		// =======================================================================================

		if( !szFilename )
		{
			Console::Append( TEXT( "ERROR: Could not resolve filename" ) );
			Console::Append( " " );
			return false;
		}
			#ifndef NOGUI
		 		TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "Playback.cpp: Play() got the filename <%s>" ), szFilename);
				Console::Append( szBuffer );
				//Console::Append( TEXT( " " ) );
			#else
				//Console::Print(  "Playback::Play() > Filename <%s>\n", szFilename);
			#endif

		// Play
		iLastIndex = iIndex;
		bPlaying  = OpenPlay( szFilename, iIndex + 1 );
		bPaused   = false;
	}
	return true;
}
// =======================================================================================


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Pause()
{
	if( !bPlaying ) return false;
	if( !active_input_plugin ) return false;
	if( !active_input_plugin->plugin ) return false;

	if( bPaused )
	{
		// Unpause
		active_input_plugin->plugin->UnPause();
		bPaused = false;
		
		// Timer ON
		//EnableTimer( true );
	}
	else
	{
		// Pause	
		active_input_plugin->plugin->Pause();
		bPaused = true;

		// Timer OFF
		//EnableTimer( false );
	}
	
//	Console::Append( TEXT( "Playback::Pause" ) );	
	return true;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Stop()
{
	if( !bPlaying ) return false;

	// ---------------------------------------------------------------------------------------
	// Stop
	if( active_input_plugin && active_input_plugin->plugin )
	{
		active_input_plugin->plugin->Stop();
	}
	active_input_plugin = NULL; // QUICK FIX
	// ---------------------------------------------------------------------------------------

	bPlaying  = false;
	bPaused   = false;
	
	// Timer OFF > It was never turned on
	//EnableTimer( false );
	
	// Reset seekbar > We don't need this
	//Playback::UpdateSeek();

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Next()
{
	return Playback_PrevOrNext( false );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::IsPlaying()
{
	return bPlaying;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::IsPaused()
{
	return bPaused;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
//int getlength();
//int getoutputtime();

bool Playback::UpdateSeek()
{
#ifdef NOGUI
	printf( "Playback::UpdateSeek > Begin\n" );
	if( active_input_plugin  )
	{
		printf( "GetLength() > Begin\n" );
			const int ms_len = active_input_plugin->plugin->GetLength();
			//const int ms_cur = active_input_plugin->plugin->GetOutputTime();

			//const int ms_len = getlength();
			//const int ms_cur = getoutputtime();

			//printf( "Current position is <%i of %i>\n", ms_cur, ms_len );
	}
	return true;
#else
	static bool bSliderEnabledBefore = false;
	bool bSliderEnabledAfter;


	//if( !WindowSeek ) return false;

	int iVal = 0;
	
	// If it has not been set
	if( !active_input_plugin || !active_input_plugin->plugin )
	{
		if( bSliderEnabledBefore )
		{
			// Update slider
			PostMessage( WindowSeek, TBM_SETPOS, ( WPARAM )( TRUE ), iVal );

			// Disable slider
			EnableWindow( WindowSeek, FALSE );
			bSliderEnabledBefore = false;
		}
	}
	else
	{
		const int ms_len = active_input_plugin->plugin->GetLength();
		if( ms_len )
		{
			const int ms_cur = active_input_plugin->plugin->GetOutputTime();
			iVal = ( ms_cur * 1000 ) / ms_len;
			
			if( iVal > 1000 ) iVal = 0;

			// =======================================================================================
			//TCHAR szBuffer[ 5000 ];
			//_stprintf( szBuffer, TEXT( "Current position is <%i of %i>" ), ms_cur, ms_len );
			//Console::Append( szBuffer );
			//Console::Append( TEXT( " " ) );
			printf( "Current position is <%i of %i>\n", ms_cur, ms_len );
			// =======================================================================================
		}

		

		if( !bSliderEnabledBefore )
		{
			EnableWindow( WindowSeek, TRUE );
			bSliderEnabledBefore = true;
		}
		
		// Update slider
		PostMessage( WindowSeek, TBM_SETPOS, ( WPARAM )( TRUE ), iVal );
	}
	
	return true;
#endif
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playback::PercentToMs( float fPercent )
{
	if( !active_input_plugin ) return -1;
	if( !active_input_plugin->plugin ) return -1;

	const int ms_len = active_input_plugin->plugin->GetLength();
	const int ms_res = ( int )( ms_len * fPercent / 100.0f );
	return ms_res;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::SeekPercent( float fPercent )
{
	// TODO update slider, NOT HERE!!!
	
	if( !bPlaying ) return false;
	if( bPaused ) return false; // TODO: apply seek when unpausing
	if( !active_input_plugin ) return false;
	if( !active_input_plugin->plugin ) return false;
	
	if( fPercent < 0.0f )
		fPercent = 0.0f;
	else if( fPercent > 100.0f )
		fPercent = 100.0f;

	const int ms_len = active_input_plugin->plugin->GetLength();
	const int ms_cur = ( int )( ms_len * fPercent / 100.0f );
	active_input_plugin->plugin->SetOutputTime( ms_cur );

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool SeekRelative( int ms )
{
	if( !bPlaying ) return false;
	if( bPaused ) return false; // TODO: apply seek when unpausing
	if( !active_input_plugin ) return false;
	if( !active_input_plugin->plugin ) return false;

	const int ms_len = active_input_plugin->plugin->GetLength();
	const int ms_old = active_input_plugin->plugin->GetOutputTime();
	int ms_new = ms_old + ms;

	if( ms_new < 0 )
		ms_new = 0;
	else if( ms_new > ms_len )
		ms_new = ms_len;
	
	if( ms_new == ms_old ) return true;
	active_input_plugin->plugin->SetOutputTime( ms_new );

	/*
	// PROGRESS
	// PostMessage( hwnd, PBM_SETPOS , ( WPARAM )( iVal ), 0 );
	// TARCKBAR
	PostMessage( wnd_pos, TBM_SETPOS, ( WPARAM )( TRUE ), ms_cur * 1000 / ms_len );
	*/
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Forward()
{
	return SeekRelative( 5000 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Rewind()
{
	return SeekRelative( -5000 );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Playback::NotifyTrackEnd()
{
	bPlaying  = false;
	bPaused   = false;
	
	// Timer
	EnableTimer( false );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playback::Volume::Get()
{
	return iCurVol;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
inline void Playback_Volume_Set( int iVol )
{
	if( active_input_plugin && active_input_plugin->plugin )
	{
		active_input_plugin->plugin->SetVolume( iVol );
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Volume::Set( int iVol )
{
	const int iCurVolBackup = iCurVol;
	iCurVol = iVol;
	ciCurVol.MakeValidPull();
	
	if( iCurVol != iCurVolBackup )
	{
		Playback_Volume_Set( iCurVol );
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Volume::Up()
{
	if( ciCurVol.IsMax() ) return true;
	
	const int iCurVolBackup = iCurVol;
	iCurVol += VOLUME_STEP;
	ciCurVol.MakeValidPull();

	if( iCurVol != iCurVolBackup )
	{
		Console::Append( TEXT( "Volume UP" ) );
		Playback_Volume_Set( iCurVol );
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Volume::Down()
{
	if( ciCurVol.IsMin() ) return true;
	
	const int iCurVolBackup = iCurVol;
	iCurVol -= VOLUME_STEP;
	ciCurVol.MakeValidPull();

	if( iCurVol != iCurVolBackup )
	{
		Console::Append( TEXT( "Volume DOWN" ) );
		Playback_Volume_Set( iCurVol );
	}

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
int Playback::Pan::Get()
{
	return iCurPan;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Playback::Pan::Set( int iPan )
{
	const int iCurPanBackup = iCurPan;
	iCurPan = iPan;
	ciCurPan.MakeValidPull();
	
	if( ( iCurPan != iCurPanBackup ) && active_input_plugin && active_input_plugin->plugin )
	{
		active_input_plugin->plugin->SetPan( iCurPan );
	}

	return true;
}
