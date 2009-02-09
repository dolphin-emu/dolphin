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


#include "Embed.h"
#include "Console.h"



#define CLASSNAME_EMBED      TEXT( "Winamp Gen" )
#define TITLE_EMBED          TEXT( "Embed target" )

#define EMBED_WIDTH          320
#define EMBED_HEIGHT         240



const TCHAR * const szEmbedTitle = TITLE_EMBED;
bool bEmbedClassRegistered = false;


LRESULT CALLBACK WndprocEmbed( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );



////////////////////////////////////////////////////////////////////////////////
/// Creates a new embed window.
///
/// @param ews Embed window state
/// @return New embed window handle
////////////////////////////////////////////////////////////////////////////////
HWND Embed::Embed( embedWindowState * ews )
{
	// Register class
	if ( !bEmbedClassRegistered )
	{
		WNDCLASS wc = {
			0,                                     // UINT style
			WndprocEmbed,                          // WNDPROC lpfnWndProc
	    	0,                                     // int cbClsExtra
	    	0,                                     // int cbWndExtra
	    	g_hInstance,                           // HINSTANCE hInstance
	    	NULL,                                  // HICON hIcon
	    	LoadCursor( NULL, IDC_ARROW ),         // HCURSOR hCursor
	    	( HBRUSH )COLOR_WINDOW,                // HBRUSH hbrBackground
	    	NULL,                                  // LPCTSTR lpszMenuName
	    	CLASSNAME_EMBED                        // LPCTSTR lpszClassName
		};
			
		if( !RegisterClass( &wc ) ) return NULL;
		
		bEmbedClassRegistered = true;
	}

	// Create window
	HWND WindowEmbed = CreateWindowEx(
		WS_EX_WINDOWEDGE |               // DWORD dwExStyle
			WS_EX_TOOLWINDOW,            //
		CLASSNAME_EMBED,                 // LPCTSTR lpClassName
		szEmbedTitle,                    // LPCTSTR lpWindowName
		WS_OVERLAPPED |                  // DWORD dwStyle
			WS_CLIPCHILDREN |            //
			WS_BORDER |                  //
			WS_CAPTION |                 //
			WS_SYSMENU |                 //
			WS_THICKFRAME |              //
			WS_MINIMIZEBOX |             //
			WS_MAXIMIZEBOX,              //
		10,                              // int x
		10,                              // int y
		EMBED_WIDTH,                     // int nWidth
		EMBED_HEIGHT,                    // int nHeight
		NULL,                            // HWND hWndParent
		NULL,                            // HMENU hMenu
		g_hInstance,                     // HINSTANCE hInstance
		NULL                             // LPVOID lpParam
	);

	Console::Append( TEXT( "Embed window born" ) );
	Console::Append( TEXT( " " ) );

	if( !ews || !ews->me ) return WindowEmbed;

	SetParent( ews->me, WindowEmbed );

	return WindowEmbed;
}



////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////
inline bool SameThread( HWND hOther )
{
	const DWORD dwOtherThreadId = GetWindowThreadProcessId( hOther, NULL );
	const DWORD dwThisThreadId = GetCurrentThreadId();
	return ( dwOtherThreadId == dwThisThreadId );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocEmbed( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
//	static bool bAllowSizeMove = false;

	switch( message )
	{

	case WM_PARENTNOTIFY:
		switch( LOWORD( wp ) )
		{
		case WM_DESTROY:
			{
				const HWND hChild = GetWindow( hwnd, GW_CHILD );
				if( !SameThread( hChild ) )
				{
					// Vis plugin
					DestroyWindow( hwnd );
				}
				break;
			}
		
		}
		break;

	case WM_SIZE:
		{
			const HWND hChild = GetWindow( hwnd, GW_CHILD );
			if( !hChild ) break;
			MoveWindow( hChild, 0, 0, LOWORD( lp ), HIWORD( lp ), TRUE );
			break;
		}

/*
	case WM_ENTERSIZEMOVE:
		bAllowSizeMove = true;
		break;

	case WM_EXITSIZEMOVE:
		bAllowSizeMove = false;
		break;

	case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS * pos = ( WINDOWPOS * )lp;
	
			// Update child			
			if( IsWindow( WindowEmbedChild = GetWindow( WindowEmbed, GW_CHILD ) ) )
			{
				RECT r;
				GetClientRect( WindowEmbed, &r );
				MoveWindow( WindowEmbedChild, 0, 0, r.right, r.bottom, TRUE );
			}

			if( !bAllowSizeMove )
			{
				// Force SWP_NOMOVE
				if( ( pos->flags & SWP_NOMOVE ) == 0 )
				{
					pos->flags |= SWP_NOMOVE;
				}
				
				// Force SWP_NOSIZE
				if( ( pos->flags & SWP_NOSIZE ) == 0 )
				{
					pos->flags |= SWP_NOSIZE;
				}
	
				return 0;
			}

			break;
		}
*/
	case WM_SHOWWINDOW:
		{
			const HWND hChild = GetWindow( hwnd, GW_CHILD );
			if( wp ) // Shown
			{
				// Update child size
				RECT r;
				GetClientRect( hwnd, &r );
				MoveWindow( hChild, 0, 0, r.right, r.bottom, TRUE );
			}
			else // Hidden
			{
				ShowWindow( hChild, SW_HIDE );
				DestroyWindow( hChild );
			}
			break;
		}

	case WM_SYSCOMMAND:
		if( ( wp & 0xFFF0 ) == SC_CLOSE )
		{
			const HWND hChild = GetWindow( hwnd, GW_CHILD );
			if( SameThread( hChild ) )
			{
				// Not a vis plugin
				ShowWindow( hwnd, SW_HIDE );
				return 0;
			}
		}
		break;

	case WM_DESTROY:
		{
			const HWND hChild = GetWindow( hwnd, GW_CHILD );
			if( hChild && SameThread( hChild ) )
			{
				DestroyWindow( hChild );
			}
			
			Console::Append( TEXT( "Embed window dead" ) );
			Console::Append( TEXT( " " ) );
			break;
		}

	}
	return DefWindowProc( hwnd, message, wp, lp );
}
