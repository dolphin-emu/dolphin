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


#include "VisModule.h"
#include "Console.h"
#include "Unicode.h"
#include "Playback.h"
#include "VisCache.h"
#include "PluginManager.h"
#include <process.h>


VisModule ** active_vis_mods = NULL; // extern
int active_vis_count = 0; // extern


/*
BOOL CALLBACK EnumThreadWndProc( HWND hwnd, LPARAM lp )
{
//	MessageBox( 0, "EnumThreadWndProc", "", 0 );
	DestroyWindow( hwnd );
}
*/



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void PlugThread( PVOID pvoid )
{
	// TODO: cleanup!!!
	
	Console::Append( TEXT( "Visualization thread born" ) );
	Console::Append( " " );
	
	VisModule * mod = ( VisModule * )pvoid;
	if( !mod ) return;
	if( mod->mod->Init( mod->mod ) != 0 ) return;


	VisCache::Create();


////////////////////////////////////////////////////////////////////////////////
	VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////

	active_vis_count++;

////////////////////////////////////////////////////////////////////////////////
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	
	bool bKeepItGoing = true;
	
	
	bool bQuitCalled = false;
	
	int iLast = GetTickCount();
	
	// Message loop
	MSG msg;
	msg.message = WM_QUIT + 1; // Must be != WM_QUIT
	do
	{
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if( msg.message == WM_QUIT )
			{
////////////////////////////////////////////////////////////////////////////////						
				// Stop
				if( !bQuitCalled )
				{
					mod->mod->Quit( mod->mod );
					bQuitCalled = true;
				}
////////////////////////////////////////////////////////////////////////////////

				break;
			}

			TranslateMessage( &msg );
			DispatchMessage( &msg );
			
			if( msg.message == WM_CLOSE || ( ( msg.message == WM_SYSCOMMAND ) && ( msg.wParam == SC_CLOSE ) ) )
			{
////////////////////////////////////////////////////////////////////////////////						
				// Stop
				if( !bQuitCalled )
				{
					mod->mod->Quit( mod->mod );
					bQuitCalled = true;
				}
////////////////////////////////////////////////////////////////////////////////
			}
		}
		
		if( bKeepItGoing )
		{
			// Variant A
			const int iNow = GetTickCount();
			if( iNow - iLast > mod->mod->delayMs )
			{
				if( Playback::IsPlaying() )
				{
					if( mod->bAllowRender )
					{
						mod->bAllowRender = false;
		
		
						const int iOffset = VisCache::LatencyToOffset( mod->mod->latencyMs );
						
						switch( mod->mod->spectrumNch )
						{
							case 2:
								VisCache::GetSpecRight(  mod->mod->spectrumData[ 1 ], iOffset );	
							case 1:
								VisCache::GetSpecLeft(   mod->mod->spectrumData[ 0 ], iOffset );
						}
		
						switch( mod->mod->waveformNch )
						{
							case 2:
								VisCache::GetWaveRight(  mod->mod->waveformData[ 1 ], iOffset );
							case 1:
								VisCache::GetWaveLeft(   mod->mod->waveformData[ 0 ], iOffset );
						}
		
						if( mod->mod->Render( mod->mod ) != 0 )
						{
////////////////////////////////////////////////////////////////////////////////						
							// Stop
							if( !bQuitCalled )
							{
								// TODO:  milkdrop doesn#t save window position
								//        when quit using manual plugin stop

								mod->mod->Quit( mod->mod );
								bQuitCalled = true;
							}
////////////////////////////////////////////////////////////////////////////////
/*
							// Destroy all windows belonging to this thread
							// This will lead to WM_QUIT automatically
							EnumThreadWindows( GetCurrentThreadId(), EnumThreadWndProc, 0 );
							bKeepItGoing = false;
*/							
						}
						
						
						iLast = iNow;
					}
				}
				else
				{
					if( mod->mod->Render( mod->mod ) != 0 )
					{
////////////////////////////////////////////////////////////////////////////////						
						// Stop
						if( !bQuitCalled )
						{
							mod->mod->Quit( mod->mod );
							bQuitCalled = true;
						}
////////////////////////////////////////////////////////////////////////////////
/*
						// Destroy all windows belonging to this thread
						// This will lead to WM_QUIT automatically
						EnumThreadWindows( GetCurrentThreadId(), EnumThreadWndProc, 0 );
						bKeepItGoing = false;
*/
					}
					
					iLast = iNow;
				}
			}

////////////////////////////////////////////////////////////////////////////////
			bool bVisLockLeft = false;
			VisLock.Enter();
////////////////////////////////////////////////////////////////////////////////

			if( mod->bShouldQuit )
			{

////////////////////////////////////////////////////////////////////////////////						
				VisLock.Leave();
				bVisLockLeft = true;
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
				// Stop
				if( !bQuitCalled )
				{
					mod->mod->Quit( mod->mod );
					bQuitCalled = true;
				}
////////////////////////////////////////////////////////////////////////////////
/*
				// Destroy all windows belonging to this thread
				// This will lead to WM_QUIT automatically
				EnumThreadWindows( GetCurrentThreadId(), EnumThreadWndProc, 0 );
				bKeepItGoing = false;
*/
			}

////////////////////////////////////////////////////////////////////////////////
			if( !bVisLockLeft )
			{
				VisLock.Leave();
			}
////////////////////////////////////////////////////////////////////////////////
		}
		
		Sleep( 1 );
	}
	while( msg.message != WM_QUIT );

	mod->bShouldQuit = false;
	
////////////////////////////////////////////////////////////////////////////////
	VisLock.Enter();
		if( ( active_vis_count > 1 ) && ( mod->iArrayIndex < active_vis_count - 1 ) )
		{
			active_vis_mods[ mod->iArrayIndex ] = active_vis_mods[ active_vis_count - 1 ];
			active_vis_mods[ mod->iArrayIndex ]->iArrayIndex = mod->iArrayIndex;
		}
		active_vis_count--;
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////
	
	mod->iArrayIndex = -1;

/*
	// Stop
	mod->mod->Quit( mod->mod );
*/

////////////////////////////////////////////////////////////////////////////////
	VisLock.Enter();
		mod->bActive = false;
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////
	
	UpdatePluginStatus( mod->plugin, true, mod->plugin->IsActive() );
	
	Console::Append( TEXT( "Visualization thread dead" ) );
	Console::Append( " " );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
VisModule::VisModule( char * szName, int iIndex, winampVisModule * mod, VisPlugin * plugin )
{
	iArrayIndex   = -1;
	bActive       = false;
	bShouldQuit   = false;
	bAllowRender  = false;

	iNameLen = ( int )strlen( szName );
	this->szName = new TCHAR[ iNameLen + 1 ];
	ToTchar( this->szName, szName, iNameLen );
	this->szName[ iNameLen ] = TEXT( '\0' );

	this->iIndex  = iIndex;
	this->mod     = mod;
	this->plugin  = plugin;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool VisModule::Start()
{
	if( !mod ) return false;
	if( bActive ) return false;
	if( plugin->IsActive() ) return false;
	
////////////////////////////////////////////////////////////////////////////////
	VisLock.Enter();
		if( !active_vis_count )
		{
			active_vis_mods = new VisModule * [ 1 ];
			active_vis_mods[ 0 ] = this;
		}
		else
		{
			VisModule ** new_active_vis_mods = new VisModule * [ active_vis_count + 1 ];
			memcpy( new_active_vis_mods, active_vis_mods, active_vis_count * sizeof( VisModule * ) );
			new_active_vis_mods[ active_vis_count ] = this;
			delete [] active_vis_mods;
			active_vis_mods = new_active_vis_mods;
		}
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	iArrayIndex = active_vis_count;


	// Start
	_beginthread( PlugThread, 1024 * 1024, ( PVOID )this );

	bActive = true;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool VisModule::Stop()
{
	if( !mod ) return false;
	if( !bActive ) return false;
	if( !plugin->IsActive() ) return false;
	
////////////////////////////////////////////////////////////////////////////////
	VisLock.Enter();
		bShouldQuit = true;
	VisLock.Leave();
////////////////////////////////////////////////////////////////////////////////

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool VisModule::Config()
{
	if( !mod ) return false;
	if( !mod->Config ) return false;
	
	mod->Config( mod );
	
	return true;	
}
