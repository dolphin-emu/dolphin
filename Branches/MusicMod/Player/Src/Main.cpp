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


//////////////////////////////////////////////////////////////////////////////////////////
// Documentation
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

/* ---- The Seekbar ----
   Rebar.cpp handle the progress (playback position) bar. WndprocMain() is called every
   second during playback. And several times a second when the cursor is moving over the
   main window. If WM_TIMER called WndprocMain it calls Playback::UpdateSeek(); */


///////////////////////////////////


#include "Main.h"
#include "GlobalVersion.h"
#include "Playlist.h"
#include "Console.h"
#include "Status.h"
#include "Rebar.h"
#include "Playback.h"
#include "PluginManager.h"
#include "DspModule.h"
#include "VisModule.h"
#include "InputPlugin.h"
#include "OutputPlugin.h"
#include "InputPlugin.h"
#include "AddDirectory.h"
#include "AddFiles.h"
#include "Winamp.h"
#include "Winamp/wa_ipc.h"
#include "Config.h"
#include <shellapi.h>

#define CLASSNAME_MAIN      TEXT( "Winamp v1.x" )
#define MAIN_TITLE          PLAINAMP_LONG_TITLE

#define MAIN_WIDTH          731
#define MAIN_HEIGHT         562



HWND WindowMain = NULL; // extern
HMENU main_context_menu = NULL; // extern
HMENU play_context_menu = NULL;
HMENU opts_context_menu = NULL;
HMENU playback_context_menu = NULL;



LRESULT CALLBACK WndprocMain( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );



WINDOWPLACEMENT WinPlaceMain;

void WinPlaceMainCallback( ConfVar * var )
{
	if( !IsWindow( WindowMain ) ) return;
	GetWindowPlacement( WindowMain, &WinPlaceMain );
}

const int cxScreen = GetSystemMetrics( SM_CXFULLSCREEN );
const int cyScreen = GetSystemMetrics( SM_CYFULLSCREEN );

RECT rMainDefault = {
	( cxScreen - MAIN_WIDTH ) / 2,
	( cyScreen - MAIN_HEIGHT ) / 2,
	( cxScreen - MAIN_WIDTH ) / 2 + MAIN_WIDTH,
	( cyScreen - MAIN_HEIGHT ) / 2 + MAIN_HEIGHT
};

ConfWinPlaceCallback cwpcWinPlaceMain(
	&WinPlaceMain,
	TEXT( "WinPlaceMain" ),
	&rMainDefault,
	WinPlaceMainCallback
);


bool bMinimizeToTray;
ConfBool cbMinimizeToTray( &bMinimizeToTray, TEXT( "MinimizeToTray" ), CONF_MODE_PUBLIC, true );



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool BuildMainWindow()
{

	//#ifndef NOGUI

		// =======================================================================================
		/* Disabling this window creation cause continuous "Error setting DirectSound cooperative level"
		   messages for some reason. So I leave it here for now. */

		// Register class
		WNDCLASS wc = {
			0,                                             // UINT style
			WndprocMain,                                   // WNDPROC lpfnWndProc
    		0,                                             // int cbClsExtra
    		0,                                             // int cbWndExtra
    		g_hInstance,                                   // HINSTANCE hInstance
    		LoadIcon( g_hInstance, TEXT( "IDI_ICON1" ) ),  // HICON hIcon
    		LoadCursor( NULL, IDC_ARROW ),                 // HCURSOR hCursor
    		( HBRUSH )COLOR_WINDOW,                        // HBRUSH hbrBackground
    		NULL,                                          // LPCTSTR lpszMenuName
    		CLASSNAME_MAIN                                 // LPCTSTR lpszClassName
		};
			
		if( !RegisterClass( &wc ) ) return false;


		// Create WindowMain
		WindowMain = CreateWindowEx(
			WS_EX_WINDOWEDGE,                        // DWORD dwExStyle
			CLASSNAME_MAIN,                          // LPCTSTR lpClassName
			MAIN_TITLE,                              // LPCTSTR lpWindowName
			WS_OVERLAPPED |                          // DWORD dwStyle
	//			WS_VISIBLE |                         //
				WS_CLIPCHILDREN |                    //
				WS_BORDER |                          //
				WS_SYSMENU |                         //
				WS_THICKFRAME |                      //
				WS_MINIMIZEBOX |                     //
				WS_MAXIMIZEBOX,                      //
			rMainDefault.left,                       // int x
			rMainDefault.top,                        // int y
			rMainDefault.right - rMainDefault.left,  // int nWidth
			rMainDefault.bottom - rMainDefault.top,  // int nHeight
			NULL,                                    // HWND hWndParent
			NULL,                                    // HMENU hMenu
			g_hInstance,                             // HINSTANCE hInstance
			NULL                                     // LPVOID lpParam
		);
		// =======================================================================================
	
	//#endif

	// =======================================================================================
	#ifdef NOGUI

		// If this is not called a crash occurs
		PlaylistView::Create();

		// We have now done what we wanted so we skip the rest of the file
		return true;

	#else
	// =======================================================================================


	if( !WindowMain )
	{
		UnregisterClass( CLASSNAME_MAIN, g_hInstance );
		return false;
	}
	
	

	
	// Build context menu
	HMENU main_menu      = CreateMenu();
	HMENU plainamp_menu  = CreatePopupMenu();
	HMENU playback_menu  = CreatePopupMenu();
	HMENU playlist_menu  = CreatePopupMenu();
	HMENU windows_menu   = CreatePopupMenu();


	// Plainamp
	AppendMenu( plainamp_menu, MF_STRING, WINAMP_OPTIONS_PREFS, TEXT( "Preferences   \tCtrl+P" ) );
	AppendMenu( plainamp_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	AppendMenu( plainamp_menu, MF_STRING, WINAMP_HELP_ABOUT, TEXT( "&About\tCtrl+F1" ) );
	AppendMenu( plainamp_menu, MF_STRING, WINAMP_FILE_QUIT, TEXT( "&Exit       \tAlt+F4" ) );

	// Playback
	AppendMenu( playback_menu, MF_STRING, WINAMP_BUTTON1, TEXT( "Pre&vious    \tZ" ) );
	AppendMenu( playback_menu, MF_STRING, WINAMP_BUTTON2, TEXT( "&Play\tX" ) );
	AppendMenu( playback_menu, MF_STRING, WINAMP_BUTTON3, TEXT( "P&ause\tC" ) );
	AppendMenu( playback_menu, MF_STRING, WINAMP_BUTTON4, TEXT( "&Stop\tV" ) );
	AppendMenu( playback_menu, MF_STRING, WINAMP_BUTTON5, TEXT( "&Next\tB" ) );

	// Playlist
	AppendMenu( playlist_menu, MF_STRING, ID_PE_OPEN, TEXT( "&Open\tCtrl+O" ) );
	AppendMenu( playlist_menu, MF_STRING, ID_PE_SAVEAS, TEXT( "&Save as\tCtrl+S" ) );
	AppendMenu( playlist_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	AppendMenu( playlist_menu, MF_STRING, WINAMP_FILE_PLAY, TEXT( "Add &files\tL" ) );
	AppendMenu( playlist_menu, MF_STRING, WINAMP_FILE_DIR, TEXT( "Add &directory\tShift+L" ) );
	AppendMenu( playlist_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	AppendMenu( playlist_menu, MF_STRING, PLAINAMP_PL_REM_SEL, TEXT( "Remove selected\tDel" ) );
	AppendMenu( playlist_menu, MF_STRING, PLAINAMP_PL_REM_CROP, TEXT( "Remove unselected     \tCtrl+Del" ) );
	AppendMenu( playlist_menu, MF_STRING, ID_PE_CLEAR, TEXT( "Remove all\tCtrl+Shift+Del" ) );
	AppendMenu( playlist_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	AppendMenu( playlist_menu, MF_STRING, ID_PE_SELECTALL, TEXT( "Select &all\tCtrl+A" ) );
	AppendMenu( playlist_menu, MF_STRING, ID_PE_NONE, TEXT( "Select &zero\tCtrl+Shift+A" ) );
	AppendMenu( playlist_menu, MF_STRING, ID_PE_INVERT, TEXT( "Select &invert\tCtrl+I" ) );

	// Windows
	AppendMenu( windows_menu, MF_STRING, MENU_MAIN_WINDOWS_CONSOLE, TEXT( "&Console" ) );
	AppendMenu( windows_menu, MF_STRING, MENU_MAIN_WINDOWS_MANAGER, TEXT( "Plugin &Manager" ) );

	// Main
	AppendMenu( main_menu, MF_STRING | MF_POPUP, ( UINT_PTR )plainamp_menu, TEXT( "&Plainamp" ) );	
	AppendMenu( main_menu, MF_STRING | MF_POPUP, ( UINT_PTR )playback_menu, TEXT( "Play&back" ) );	
	AppendMenu( main_menu, MF_STRING | MF_POPUP, ( UINT_PTR )playlist_menu, TEXT( "Play&list" ) );	
	AppendMenu( main_menu, MF_STRING | MF_POPUP, ( UINT_PTR )windows_menu, TEXT( "&Windows" ) );	

	SetMenu( WindowMain, main_menu );
	
	////////////////////////////////////////////////////////////////////////////////

	main_context_menu = CreatePopupMenu();
	AppendMenu( main_context_menu, MF_STRING, WINAMP_HELP_ABOUT, TEXT( "Plainamp" ) );

	AppendMenu( main_context_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );

		play_context_menu = CreatePopupMenu();
		AppendMenu( play_context_menu, MF_STRING, WINAMP_FILE_PLAY, TEXT( "Files   \tL" ) );
		AppendMenu( play_context_menu, MF_STRING, WINAMP_FILE_DIR, TEXT( "Folder   \tShift+L" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_POPUP, ( UINT_PTR )play_context_menu, TEXT( "Play" ) );
	
	AppendMenu( main_context_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED | MF_CHECKED, WINAMP_MAIN_WINDOW, TEXT( "Main Window\tAlt+W" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED | MF_CHECKED, WINAMP_OPTIONS_PLEDIT, TEXT( "Playlist Editor\tAlt+E" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED, WINAMP_OPTIONS_EQ, TEXT( "Equalizer\tAlt+G" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED, WINAMP_OPTIONS_VIDEO, TEXT( "Video\tAlt+V" ) );
	AppendMenu( main_context_menu, MF_STRING, PLAINAMP_TOGGLE_CONSOLE, TEXT( "Console" ) );
	AppendMenu( main_context_menu, MF_STRING, PLAINAMP_TOGGLE_MANAGER, TEXT( "Plugin Manager" ) );
	
	
	/*	
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED, MENU_MAIN_CONTEXT_MANAGER, TEXT( "Plugin Manager" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_DISABLED | MF_GRAYED, MENU_MAIN_CONTEXT_CONSOLE, TEXT( "Console" ) );
	*/

	AppendMenu( main_context_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );
	
		opts_context_menu = CreatePopupMenu();
		AppendMenu( opts_context_menu, MF_STRING, WINAMP_OPTIONS_PREFS, TEXT( "Preferences   \tCtrl+P" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_POPUP, ( UINT_PTR )opts_context_menu, TEXT( "Options" ) );

		playback_context_menu = CreatePopupMenu();
		AppendMenu( playback_context_menu, MF_STRING, WINAMP_BUTTON1, TEXT( "Previous    \tZ" ) );
		AppendMenu( playback_context_menu, MF_STRING, WINAMP_BUTTON2, TEXT( "Play\tX" ) );
		AppendMenu( playback_context_menu, MF_STRING, WINAMP_BUTTON3, TEXT( "Pause\tC" ) );
		AppendMenu( playback_context_menu, MF_STRING, WINAMP_BUTTON4, TEXT( "Stop\tV" ) );
		AppendMenu( playback_context_menu, MF_STRING, WINAMP_BUTTON5, TEXT( "Next\tB" ) );
	AppendMenu( main_context_menu, MF_STRING | MF_POPUP, ( UINT_PTR )playback_context_menu, TEXT( "Playback" ) );
	
	AppendMenu( main_context_menu, MF_SEPARATOR | MF_DISABLED | MF_GRAYED, ( UINT_PTR )-1, NULL );

	AppendMenu( main_context_menu, MF_STRING, WINAMP_FILE_QUIT, TEXT( "Exit" ) );


	Toolbar::Create(); // This removes all buttons and status bars
	//BuildMainStatus();


	// =======================================================================================
	// If this is not created a crash occurs
	PlaylistView::Create();
	
	Playlist::Create();


	SetWindowPlacement( WindowMain, &WinPlaceMain );

	
	return true;
#endif
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void About( HWND hParent )
{
	// For info goto
	// http://predef.sourceforge.net/precomp.html

	TCHAR szBuildDetails[ 1000 ] = "";

#ifdef __GNUC__
	_stprintf( szBuildDetails,
		TEXT( "\n\n\nGNU GCC " __VERSION__ "\n" __DATE__ )
	);
#else
# ifdef _MSC_VER
	_stprintf(
		szBuildDetails,
		TEXT( "\n\n\nMicrosoft Visual C++ %i.%i\n" __DATE__ ),
		_MSC_VER / 100 - 6,
		( _MSC_VER % 100 ) / 10
	);
# endif
#endif

	TCHAR szBuffer[ 1000 ];
	_stprintf(
		szBuffer,
		PLAINAMP_LONG_TITLE TEXT( "\n"
			"\n"
			"Copyright © 2005 Sebastian Pipping  \n"
			"<webmaster@hartwork.org>\n"
			"\n"
			"-->  http://www.hartwork.org"
			"%s"
		),
		szBuildDetails
	);

	MessageBox(
		hParent,
		szBuffer,
		TEXT( "About" ),
		MB_ICONINFORMATION
	);
}

#define TRAY_MAIN_ID  13
#define TRAY_MSG      ( WM_USER + 1 )



NOTIFYICONDATA nid;

bool AddTrayIcon( HWND hwnd )
{
	ZeroMemory( &nid, sizeof( NOTIFYICONDATA ) );
	nid.cbSize            = sizeof( NOTIFYICONDATA );
	nid.hWnd              = hwnd;
	nid.uID               = TRAY_MAIN_ID;
	nid.uFlags            = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage  = TRAY_MSG;
	nid.hIcon             = LoadIcon( g_hInstance, TEXT( "IDI_ICON1" ) );
	_tcscpy( nid.szTip, TEXT( "Plainamp" ) );

	return ( Shell_NotifyIcon( NIM_ADD, &nid ) != 0 );
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon( NIM_DELETE, &nid );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocMain( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	// Tool windows are hidden on minimize/re-shown on restore
	static bool bConsoleTodo = false;
	static bool bManagerTodo = false;
	static bool bRemoveIcon = false;

	#ifdef NOGUI
		//wprintf("DLL > Main.cpp:WndprocMain() was called. But nothing will be done. \n");
	#else
		Console::Append( TEXT( "Main.cpp:WndprocMain was called" ) );
	#endif	


	switch( message )
	{

	#ifdef NOGUI
		//printf(" > WndprocMain message: %i\n", message);
	#else

	case WM_SETFOCUS:
		// To re-"blue"
		SetFocus( WindowPlaylist );
		break;

	case WM_CREATE:
		// Note: [WindowMain] is not valid yet but [hwnd] is!
		Console::Create(); // make the console window
		PluginManager::Build(); // make the plugin window
		break;

	case WM_NOTIFY:
		{
			NMHDR * hdr = ( NMHDR * )lp;
			
			switch( hdr->code )
			{
			case LVN_GETDISPINFO:
				{
					LV_DISPINFO * lpdi = ( LV_DISPINFO * )lp;
					playlist->Fill( lpdi->item );
				}
				return 0;
/*				
			case LVN_ODCACHEHINT:
				{
					LPNMLVCACHEHINT   lpCacheHint = (LPNMLVCACHEHINT)lParam;
					/
					This sample doesn't use this notification, but this is sent when the 
					ListView is about to ask for a range of items. On this notification, 
					you should load the specified items into your local cache. It is still 
					possible to get an LVN_GETDISPINFO for an item that has not been cached, 
					therefore, your application must take into account the chance of this 
					occurring.
					/
				}
				return 0;

			case LVN_ODFINDITEM:
				{
					LPNMLVFINDITEM lpFindItem = (LPNMLVFINDITEM)lParam;
					/
					This sample doesn't use this notification, but this is sent when the 
					ListView needs a particular item. Return -1 if the item is not found.
					/
				}
				return 0;
*/				
			case NM_CUSTOMDRAW:
				{
					NMLVCUSTOMDRAW * custom = ( NMLVCUSTOMDRAW * )lp;

					switch( custom->nmcd.dwDrawStage )
					{
					case CDDS_PREPAINT:
						return CDRF_NOTIFYITEMDRAW;
						
					case CDDS_ITEMPREPAINT:
						return CDRF_NOTIFYSUBITEMDRAW;
						
					case ( CDDS_SUBITEM | CDDS_ITEMPREPAINT ):
						{
							// This is the prepaint stage for an item. Here's where we set the
							// item's text color. Our return value will tell Windows to draw the
							// item itself, but it will use the new color we set here.
							// We'll cycle the colors through red, green, and light blue.

							if( custom->nmcd.dwItemSpec == playlist->GetCurIndex() )
							{
								custom->clrTextBk = RGB( 225, 225, 225 );
							}
							else
							{
								if( custom->nmcd.dwItemSpec & 1 )
									custom->clrTextBk = RGB( 245, 248, 250 );
								else
									custom->clrTextBk = RGB( 255, 255, 255 );
							}

							if( custom->iSubItem == 0 )
								custom->clrText = RGB( 255, 0, 0 );
							else
								custom->clrText = RGB( 0, 0, 0 );
							
							
/*
							if ( (custom->nmcd.dwItemSpec % 3) == 0 )
								crText = RGB(255,0,0);
							else if ( (custom->nmcd.dwItemSpec % 3) == 1 )
								crText = RGB(0,255,0);
							else
								crText = RGB(128,128,255);

							// Store the color back in the NMLVCUSTOMDRAW struct.
							custom->clrText = crText;
*/
							// Tell Windows to paint the control itself.
						}
					/*
						custom->clrText = RGB( 190, 190, 190 );
						custom->clrTextBk = RGB( 255, 0, 0 );*/
						return CDRF_DODEFAULT;
						
						
					}
					break;

				}
				
				/*
			case RBN_CHILDSIZE:
				{
				NMREBARCHILDSIZE * chs = ( NMREBARCHILDSIZE * )lp;
				const int width_client = chs->rcChild.right - chs->rcChild.left;
				int diff = width_client - 120;
				if( diff > 0 )
				{
					const int width_band = chs->rcBand.right - chs->rcBand.left;
					// chs->rcChild.right = chs->rcChild.left + 120;
	
					DEBUGF( 1000, "CHILDSIZE [%i] [%i]", chs->uBand, width_band );
	
					const int client_band_diff = width_band - width_client;
					chs->rcBand.right = chs->rcBand.left + 120 + client_band_diff;
					// chs->uBand
	
					
					REBARBANDINFO rbbi;
					rbbi.cbSize = sizeof( REBARBANDINFO );
					rbbi.fMask = RBBIM_SIZE;
					rbbi.cx = 154; //width_band + diff;
					LRESULT lResult = SendMessage(
						rebar,
						RB_SETBANDINFO, 
						chs->uBand,
						( LPARAM )( REBARBANDINFO * )&rbbi
					);
					
				}
				break;
			}
			*/
			case RBN_HEIGHTCHANGE:
				{
					const int iRebarHeightBefore = iRebarHeight;
					RECT r;
					GetWindowRect( WindowRebar, &r );
					iRebarHeight = r.bottom - r.top;

					InvalidateRect( WindowRebar, NULL, TRUE );
					InvalidateRect( WindowPlaylist, NULL, TRUE );

					RECT client;
					GetClientRect( WindowMain, &client );
					PostMessage(
						hwnd,
						WM_SIZE,
						SIZE_RESTORED,
						( client.right - client.left ) << 16 |
							( client.bottom - client.top )
					);
					
					break;
				}
			}
			break;
		}
	
	case WM_SYSKEYDOWN:
		switch( wp ) // [Alt]+[...]
		{
		case VK_UP:
		case VK_DOWN:
			SetFocus( WindowPlaylist );
			SendMessage( WindowPlaylist, message, wp, lp );
			break;
		}
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
		SetFocus( WindowPlaylist );
		SendMessage( WindowPlaylist, message, wp, lp );
		break;

	case WM_COMMAND:
		{
			const int code = HIWORD( wp );
			switch( code )
			{
			case 1: // also == CBN_SELCHANGE
				{
					if( ( HWND )lp == WindowOrder )
					{
						LRESULT res = SendMessage( WindowOrder, CB_GETCURSEL, 0, 0 );
						if( res == CB_ERR ) break;
						Playback::Order::SetMode( ( int )res );
					}
					else if( ( HWND )lp == WindowEq )
					{
						LRESULT res = SendMessage( WindowEq, CB_GETCURSEL, 0, 0 );
						if( res == CB_ERR ) break;
						Playback::Eq::SetIndex( ( int )( res - 1 ) );
					}
					
					return WndprocWinamp( hwnd, message, wp, lp );
				}
				
			case 0:
				{
					// Menu
					const int id = LOWORD( wp );
					switch( id )
					{
					case MENU_MAIN_WINDOWS_CONSOLE:
						Console::Popup();
						break;
	
					case MENU_MAIN_WINDOWS_MANAGER:
						PluginManager::Popup();
						break;

					}
/*
					TCHAR szBuffer[ 5000 ];
					_stprintf( szBuffer, TEXT( "default 1 id = <%i>" ), id );
					MessageBox( 0, szBuffer, "", 0 );
*/						
					return WndprocWinamp( hwnd, message, wp, lp );
				}

			default:
				return WndprocWinamp( hwnd, message, wp, lp );
			}
			break;
		}

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO * mmi = ( MINMAXINFO * )lp;
			mmi->ptMinTrackSize.x = 400;
			mmi->ptMinTrackSize.y = 300;
			return 0;
		}
		
	case WM_SIZE:
		{
			// Resize children
			RECT client;
			GetClientRect( WindowMain, &client );
		
			const int iClientWidth = client.right - client.left;
			const int iClientHeight = client.bottom - client.top;
			const int iPlaylistHeight = iClientHeight - iRebarHeight - iStatusHeight;
	
			if( WindowRebar )
				MoveWindow( WindowRebar, 0, 0, iClientWidth, iRebarHeight, TRUE );
			if( WindowPlaylist )
			{
				MoveWindow( WindowPlaylist, 0, iRebarHeight, iClientWidth, iPlaylistHeight, TRUE );
				playlist->Resize( WindowMain );
			}
			if( WindowStatus )
				MoveWindow( WindowStatus, 0, iRebarHeight + iPlaylistHeight, iClientWidth, iStatusHeight, TRUE );
			break;
		}

	case WM_TIMER:
		Playback::UpdateSeek();
		break;
		
	case WM_CONTEXTMENU:
		PostMessage( hwnd, WM_COMMAND, WINAMP_MAINMENU, 0 );
		break;
		
	case WM_CLOSE:
		{
			// Clean shutdown
			
			// Stop
			Playback::Stop();
	
			// Dsp
			DspLock.Enter();
				for( int d = active_dsp_count - 1; d >= 0; d-- )
				{
					DspLock.Leave();
						active_dsp_mods[ d ]->Stop();
					DspLock.Enter();
				}
			DspLock.Leave();
	
			
			// Vis
			VisLock.Enter();
				for( int v = active_vis_count - 1; v >= 0; v-- )
				{
					VisLock.Leave();
						active_vis_mods[ v ]->Stop();
					VisLock.Enter();
				}
			VisLock.Leave();
		}
		break;

	case WM_DESTROY:
		{
			// =======================================================================================
			// Save playlist
			/*
			TCHAR * szPlaylistMind = new TCHAR[ iHomeDirLen + 12 + 1 ];
			memcpy( szPlaylistMind, szHomeDir, iHomeDirLen * sizeof( TCHAR ) );
			memcpy( szPlaylistMind + iHomeDirLen, TEXT( "Plainamp.m3u" ), 12 * sizeof( TCHAR ) );
			szPlaylistMind[ iHomeDirLen + 12 ] = TEXT( '\0' );

			Playlist::ExportPlaylistFile( szPlaylistMind );

			delete [] szPlaylistMind;
			*/
			// =======================================================================================

			cwpcWinPlaceMain.TriggerCallback();
			cwpcWinPlaceMain.RemoveCallback();
			
			Console::Destroy();
			PluginManager::Destroy();
			
			if( bRemoveIcon )
			{
				RemoveTrayIcon();
				bRemoveIcon = false;
			}
			
			PostQuitMessage( 0 );
			return 0;
		}
		
	case WM_ACTIVATEAPP:
		{
			if( wp != TRUE ) break;

			// Also bring console/manager to front
			const bool bConsoleVisible  = ( IsWindowVisible( WindowConsole ) == TRUE );
			const bool bManagerVisible  = ( IsWindowVisible( WindowManager ) == TRUE );
			const bool bMainTodo        = ( bConsoleVisible || bManagerVisible );

			if( bConsoleVisible ) BringWindowToTop( WindowConsole );
			if( bManagerVisible ) BringWindowToTop( WindowManager );
			if( bMainTodo       ) BringWindowToTop( WindowMain    );
			
			break;
		}

	case WM_SYSCOMMAND:
		switch( ( wp & 0xFFF0 ) )
		{
		case SC_CLOSE:
			if( !SendMessage( WindowMain, WM_WA_IPC, 0, IPC_HOOK_OKTOQUIT ) )
			{
				return 0;
			}
			break;

		case SC_MINIMIZE:
			{
				// Hide console/manager on minimize
				bConsoleTodo = ( IsWindowVisible( WindowConsole ) == TRUE );
				if( bConsoleTodo ) ShowWindow( WindowConsole, SW_HIDE );

				bManagerTodo = ( IsWindowVisible( WindowManager ) == TRUE );
				if( bManagerTodo ) ShowWindow( WindowManager, SW_HIDE );
				
				if( bMinimizeToTray )
				{
					if( !bRemoveIcon )
					{
						bRemoveIcon = AddTrayIcon( hwnd );
					}

					ShowWindow( hwnd, FALSE );
					return 0;
				}

				break;
			}

		case SC_RESTORE:
			{
				const LRESULT res = DefWindowProc( hwnd, message, wp, lp );

				// Unhide console/manager
				const bool bMainTodo = ( bConsoleTodo || bManagerTodo );
				if( bConsoleTodo ) ShowWindow( WindowConsole, SW_SHOW );
				if( bManagerTodo ) ShowWindow( WindowManager, SW_SHOW );
				if( bMainTodo    ) BringWindowToTop( WindowMain );
				
				return res;
			}
		}
		break;
	
	case TRAY_MSG:
		switch( lp )
		{
		case WM_RBUTTONDOWN: // TODO: context menu instead
		case WM_LBUTTONDOWN:
			if( IsWindowVisible( hwnd ) == FALSE )
			{
				ShowWindow( hwnd, TRUE );
			}
			break;
			
		case WM_RBUTTONUP: // TODO: context menu instead
		case WM_LBUTTONUP:
			if( bRemoveIcon )
			{
				RemoveTrayIcon();
				bRemoveIcon = false;
			}
			break;

		}
		return 0;
	#endif
	default:
		return WndprocWinamp( hwnd, message, wp, lp );
	}
	return DefWindowProc( hwnd, message, wp, lp );

}
