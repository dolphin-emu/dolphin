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


#include "Prefs.h"
#include "Util.h"
#include "Font.h"
#include "Console.h"
#include <map>

using namespace std;


#define CLASSNAME_PREFS  TEXT( "CLASSNAME_PREFS" )
#define TITLE_PREFS      TEXT( "Preferences" )


#define PAGE_WIDTH       409
#define PAGE_HEIGHT      400


#define GAP_LEFT         4
#define GAP_RIGHT        5
#define GAP_TOP          4
#define GAP_BOTTOM       5
#define SPLITTER_WIDTH   6
#define SPLITTER_HEIGHT  6

#define BOTTOM_SPACE     36

#define BUTTON_WIDTH     80


#define PREFS_WIDTH      606
#define PREFS_HEIGHT     ( GAP_TOP + PAGE_HEIGHT + BOTTOM_SPACE )



struct PrefRecCompare
{
	bool operator()( const prefsDlgRec * a, const prefsDlgRec * b ) const
	{
		if( a->hInst < b->hInst ) return true;
		if( a->dlgID < b->dlgID ) return true;
		return strcmp( a->name, b->name ) < 0;
	}
};


map<prefsDlgRec *, HTREEITEM, PrefRecCompare> rec_to_item;


struct AllWeNeed
{
	prefsDlgRec	* PageData;
	HWND hwnd;
};



HWND WindowPrefs = NULL;
HWND WindowPage = NULL;
HWND WindowTree = NULL;
HWND ButtonClose = NULL;

HTREEITEM root = NULL;

HWND hCurrentPage = NULL;
// bool bKeepFocus = false;


LRESULT CALLBACK WndprocPrefs( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );
LRESULT CALLBACK WndprocTree( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );
WNDPROC WndprocTreeBackup = NULL;


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs::Create()
{
	if( WindowPrefs ) return false;
	
	// Register class
	WNDCLASS wc = {
		0,                                     // UINT style
		WndprocPrefs,                          // WNDPROC lpfnWndProc
    	0,                                     // int cbClsExtra
    	0,                                     // int cbWndExtra
    	g_hInstance,                           // HINSTANCE hInstance
    	NULL,                                  // HICON hIcon
    	LoadCursor( NULL, IDC_ARROW ),         // HCURSOR hCursor
    	( HBRUSH )COLOR_WINDOW,                // HBRUSH hbrBackground
    	NULL,                                  // LPCTSTR lpszMenuName
    	CLASSNAME_PREFS                        // LPCTSTR lpszClassName
	};
		
	if( !RegisterClass( &wc ) ) return false;
	
	const int cxScreen = GetSystemMetrics( SM_CXFULLSCREEN );
	const int cyScreen = GetSystemMetrics( SM_CYFULLSCREEN );

	// Create window
	WindowPrefs = CreateWindowEx(
		WS_EX_WINDOWEDGE |                // DWORD dwExStyle
			WS_EX_TOOLWINDOW,             //
		CLASSNAME_PREFS,                  // LPCTSTR lpClassName
		TITLE_PREFS,                      // LPCTSTR lpWindowName
		WS_OVERLAPPED |                   // DWORD dwStyle
//			WS_VISIBLE |                  //
			WS_CLIPCHILDREN |             //
			WS_SYSMENU,                   //
		( cxScreen - PREFS_WIDTH ) / 2,   // int x
		( cyScreen - PREFS_HEIGHT ) / 2,  // int y
		PREFS_WIDTH,                      // int nWidth
		PREFS_HEIGHT,                     // int nHeight
		NULL,                             // HWND hWndParent
		NULL,                             // HMENU hMenu
		g_hInstance,                      // HINSTANCE hInstance
		NULL                              // LPVOID lpParam
	);
	

	if( !WindowPrefs )
	{
		UnregisterClass( CLASSNAME_PREFS, g_hInstance );
		return false;
	}


	RECT r;
	GetClientRect( WindowPrefs, &r );


////////////////////////////////////////////////////////////////////////////////
	const int iWidth = PREFS_WIDTH  * 2 - r.right;
	const int iHeight = PREFS_HEIGHT * 2 - r.bottom;
	SetWindowPos(
		WindowPrefs,
		NULL,
		( cxScreen - iWidth ) / 2,
		( cyScreen - iHeight ) / 2,
		iWidth,
		iHeight,
		SWP_NOZORDER
	);
	GetClientRect( WindowPrefs, &r );
////////////////////////////////////////////////////////////////////////////////


	LoadCommonControls();

	const int iTreeWidth = ( r.right - r.left ) - PAGE_WIDTH - GAP_LEFT - GAP_RIGHT - SPLITTER_WIDTH;

	WindowTree = CreateWindowEx(
		WS_EX_CLIENTEDGE,                     // DWORD dwExStyle
		WC_TREEVIEW,                          // LPCTSTR lpClassName
		NULL,                                 // LPCTSTR lpWindowName
		WS_VSCROLL |                          // DWORD dwStyle
			WS_VISIBLE |                      //
			WS_CHILD |                        //
			WS_TABSTOP |                      //
			TVS_HASLINES |                    //
			TVS_LINESATROOT |                 //
			TVS_HASBUTTONS |                  //
			TVS_SHOWSELALWAYS |               //
			TVS_DISABLEDRAGDROP |             //
			TVS_NONEVENHEIGHT,                //
		GAP_LEFT,                             // int x
		GAP_TOP,                              // int y
		iTreeWidth,                           // int nWidth
		PAGE_HEIGHT,                          // int nHeight
		WindowPrefs,                          // HWND hWndParent
		NULL,                                 // HMENU hMenu
		g_hInstance,                          // HINSTANCE hInstance
		NULL                                  // LPVOID lpParam
	);
	
	if( !WindowTree )
	{
		; //...
	}
	
	Font::Apply( WindowTree );
	
	// Exchange window procedure
	WndprocTreeBackup = ( WNDPROC )GetWindowLong( WindowTree, GWL_WNDPROC );
	if( WndprocTreeBackup != NULL )
	{
		SetWindowLong( WindowTree, GWL_WNDPROC, ( LONG )WndprocTree );
	}
	
	
	const int iPageLeft = GAP_LEFT + iTreeWidth + SPLITTER_WIDTH;
	
	WindowPage = CreateWindowEx(
		0,                                    // DWORD dwExStyle
		TEXT( "Static" ),                     // LPCTSTR lpClassName
		TEXT( "Nothing to see here" ),        // LPCTSTR lpWindowName
		WS_TABSTOP |                          // DWORD dwStyle
			WS_VISIBLE |                      //
			WS_CHILD |                        //
			SS_CENTER |                       //
			SS_CENTERIMAGE,                   //
		iPageLeft,                            // int x
		GAP_TOP,                              // int y
		PAGE_WIDTH,                           // int nWidth
		PAGE_HEIGHT,                          // int nHeight
		WindowPrefs,                          // HWND hWndParent
		NULL,                                 // HMENU hMenu
		g_hInstance,                          // HINSTANCE hInstance
		NULL                                  // LPVOID lpParam
	);

	if( !WindowPage )
	{
		; //...
	}

	Font::Apply( WindowPage );


	const int iButtonLeft = ( r.right - r.left ) - GAP_RIGHT - BUTTON_WIDTH - 1;
	const int iButtonTop = ( r.bottom - r.top ) - BOTTOM_SPACE + SPLITTER_HEIGHT;


	ButtonClose = CreateWindowEx(
		0,                                    // DWORD dwExStyle
		TEXT( "Button" ),                     // LPCTSTR lpClassName
		TEXT( "Close" ),                      // LPCTSTR lpWindowName
		WS_TABSTOP |                          // DWORD dwStyle
			WS_VISIBLE |                      //
			WS_CHILD |                        //
			BS_PUSHBUTTON |                   //
			BS_TEXT |                         //
			BS_VCENTER,                       //
		iButtonLeft,                          // int x
		iButtonTop,                           // int y
		BUTTON_WIDTH,                         // int nWidth
		BOTTOM_SPACE - SPLITTER_HEIGHT - GAP_BOTTOM,  // int nHeight
		WindowPrefs,                          // HWND hWndParent
		NULL,                                 // HMENU hMenu
		g_hInstance,                          // HINSTANCE hInstance
		NULL                                  // LPVOID lpParam
	);
	
	Font::Apply( ButtonClose );
	
	AllWeNeed * awn_root = new AllWeNeed;
//	memset( &awn_root->PageData, 0, sizeof( prefsDlgRec ) );
	awn_root->PageData = NULL;
	awn_root->hwnd = NULL;

	TV_INSERTSTRUCT tvi = {
		root,                                     // HTREEITEM hParent
		TVI_SORT,                                 // HTREEITEM hInsertAfter
		{
			TVIF_PARAM | TVIF_STATE | TVIF_TEXT,  // UINT mask
			NULL,                                 // HTREEITEM hItem
			TVIS_EXPANDED | TVIS_SELECTED,        // UINT state
			0,                                    // UINT stateMask
			TEXT( "General" ),                    // LPSTR pszText
			8,                                    // int cchTextMax
			0,                                    // int iImage
			0,                                    // int iSelectedImage
			0,                                    // int cChildren
			( LPARAM )awn_root                    // LPARAM lParam
		}
	};
	
	root = TreeView_InsertItem( WindowTree, &tvi );
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs::Destroy()
{
	if( !WindowPrefs ) return false;
	
	DestroyWindow( WindowPrefs );
	UnregisterClass( CLASSNAME_PREFS, g_hInstance );
	WindowPrefs = NULL;
	
	DestroyWindow( WindowTree );	
	WindowTree = NULL;

	DestroyWindow( WindowPage );
	WindowPage = NULL;

	DestroyWindow( ButtonClose );
	ButtonClose = NULL;

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs_Show( HTREEITEM item )
{
	if( !WindowPrefs ) return false;

	// Select and load associated page
	TreeView_SelectItem( WindowTree, item );

	if( !IsWindowVisible( WindowPrefs ) )
	{
		ShowWindow( WindowPrefs, SW_SHOW );
	}
	
	SetActiveWindow( WindowPrefs );
	SetFocus( WindowTree );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs::Show()
{
	return Prefs_Show( root );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs::Show( prefsDlgRec * PageData )
{
	map<prefsDlgRec *, HTREEITEM, PrefRecCompare>::iterator iter = rec_to_item.find( PageData );
	if( iter != rec_to_item.end() )
	{
		return Prefs_Show( iter->second );
	}
	else
	{
		return Prefs_Show( root );
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Prefs::AddPage( prefsDlgRec * PageData )
{
	// TODO unicode!
	if( !WindowPrefs ) return false;
	
	// Backup
	char * NameBackup = new char[ strlen( PageData->name ) + 1 ];
	strcpy( NameBackup, PageData->name );
	prefsDlgRec * PageDataBackup = new prefsDlgRec;
	memcpy( PageDataBackup, PageData, sizeof( prefsDlgRec ) );
	PageDataBackup->name = NameBackup;
	
	AllWeNeed * awn = new AllWeNeed;
	awn->PageData  = PageDataBackup;
	awn->hwnd      = NULL;
	
	TV_INSERTSTRUCT tvi = {
		root,                                            // HTREEITEM hParent
		TVI_SORT,                                        // HTREEITEM hInsertAfter
		{                                                //
			TVIF_PARAM | TVIF_STATE | TVIF_TEXT,         // UINT mask
			NULL,                                        // HTREEITEM hItem
			TVIS_EXPANDED | TVIS_SELECTED,               // UINT state
			0,                                           // UINT stateMask
			PageDataBackup->name,                        // LPSTR pszText
			( int )strlen( PageDataBackup->name ) + 1,   // int cchTextMax
			0,                                           // int iImage
			0,                                           // int iSelectedImage
			0,                                           // int cChildren
			( LPARAM )awn                                // LPARAM lParam
		}
	};
	
	HTREEITEM new_item = TreeView_InsertItem( WindowTree, &tvi );
	
	TreeView_Expand( WindowTree, root, TVE_EXPAND );
	
	rec_to_item.insert( pair<prefsDlgRec *, HTREEITEM>( PageDataBackup, new_item ) );
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocPrefs( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
	case WM_NOTIFY:
		{
			NMHDR * nmhdr = ( NMHDR * )lp;
			if( ( nmhdr->hwndFrom == WindowTree ) && ( nmhdr->code == TVN_SELCHANGING ) )
			{
				NMTREEVIEW * nmtv = ( NMTREEVIEW * )lp;
				TVITEM * OldPage = &nmtv->itemOld;
				TVITEM * NewPage = &nmtv->itemNew;
				
				// Destroy old window
				AllWeNeed * old_awn = ( AllWeNeed * )OldPage->lParam;
				if( old_awn && old_awn->hwnd && IsWindow( old_awn->hwnd ) )
				{
					DestroyWindow( old_awn->hwnd );
					old_awn->hwnd = NULL;
				}
				
				// Create new window
				AllWeNeed * new_awn = ( AllWeNeed * )NewPage->lParam;
				if( new_awn )
				{
					prefsDlgRec * PageData = new_awn->PageData;
					if( PageData && PageData->hInst ) // root has NULL here
					{
						if( !PageData->proc )
						{
							MessageBox( 0, TEXT( "proc NULL" ), TEXT( "" ), 0 );
							PageData->proc = ( void * )WndprocPrefs;
						}
						
/*
						RECT r;
						GetWindowRect( WindowPage, &r );
						const int iWidth = r.right - r.left;
						const int iHeight = r.bottom - r.top;
						POINT p = { r.left, r.top };
						ScreenToClient( WindowPrefs, &p );
						MoveWindow( WindowPage, p.x, p.y, iWidth - 10, iHeight - 10, FALSE );
*/
//						bKeepFocus = true;

						HWND hPage = CreateDialog(
							PageData->hInst,             // HINSTANCE hInstance,
							( LPCTSTR )PageData->dlgID,  // LPCTSTR lpTemplate,
							WindowPage,                  // HWND hWndParent,
							( DLGPROC )PageData->proc    // DLGPROC lpDialogFunc
						);
						new_awn->hwnd = hPage;
						
//						MoveWindow( WindowPage, p.x, p.y, iWidth, iHeight, FALSE );

						ShowWindow( hPage, SW_SHOW );
						UpdateWindow( hPage );
						SetFocus( WindowTree );
/*
						SetActiveWindow( hPage );
						SetActiveWindow( hwnd );
*/						
						hCurrentPage = hPage;
//						bKeepFocus = false;
					}
/*
					else
					{
						MessageBox( 0, TEXT( "hInst NULL" ), TEXT( "" ), 0 );
					}
*/
				}
				else
				{
					MessageBox( 0, TEXT( "awn NULL" ), TEXT( "" ), 0 );
				}
			}
			break;
		}

	case WM_COMMAND:
		switch( HIWORD( wp ) )
		{
		case BN_CLICKED:
			if( ( HWND )lp == ButtonClose )
			{
				PostMessage( hwnd, WM_SYSCOMMAND, SC_CLOSE, 0 );
			}
			break;
		}
		break;

	case WM_SYSCOMMAND:
		if( ( wp & 0xFFF0 ) == SC_CLOSE )
		{
			ShowWindow( hwnd, SW_HIDE );

			// Destroy current page so the settings are saved
			// (currently be selecting the empty page
			TreeView_SelectItem( WindowTree, root );

/*		
			if( hCurrentPage && IsWindow( hCurrentPage ) )
			{
				DestroyWindow( hCurrentPage );
				hCurrentPage = NULL;
			}
*/			
			return 0;
		}
		break;
		
	case WM_KEYDOWN:
		switch( wp )
		{
		case VK_ESCAPE:
			PostMessage( WindowPrefs, WM_SYSCOMMAND, SC_CLOSE, 0 );
			break;
	
		}
		break;
/*		
	case WM_KILLFOCUS:
		if( bKeepFocus )
		{
			return 1;
		}
		break;
*/
	}

	return DefWindowProc( hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocTree( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
	case WM_KEYDOWN:
		switch( wp )
		{
		case VK_ESCAPE:
			PostMessage( WindowPrefs, WM_SYSCOMMAND, SC_CLOSE, 0 );
			break;
	
		}
		break;
		
	}
	return CallWindowProc( WndprocTreeBackup, hwnd, message, wp, lp );
}
