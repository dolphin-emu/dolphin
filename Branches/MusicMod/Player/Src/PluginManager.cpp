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


#include "PluginManager.h"
#include "Plugin.h"
#include "InputPlugin.h"
#include "OutputPlugin.h"
#include "VisPlugin.h"
#include "DspPlugin.h"
#include "GenPlugin.h"
#include "Console.h"
#include "Main.h"
#include "Util.h"
#include "Config.h"



#define MENU_INPUT_CONFIG   1
#define MENU_INPUT_ABOUT    2

#define MENU_OUTPUT_CONFIG  1
#define MENU_OUTPUT_ABOUT   2
#define MENU_OUTPUT_SEP     3
#define MENU_OUTPUT_ACTIVE  4
#define MENU_OUTPUT_LOAD    5

#define MENU_GEN_CONFIG     1
#define MENU_GEN_SEP        2
#define MENU_GEN_LOAD       3



HWND WindowManager = NULL; // extern
HWND WindowListView = NULL;


WNDPROC WndprocListViewBackup = NULL;
LRESULT CALLBACK WndprocListView( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );

LRESULT CALLBACK WndprocManager( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );


HMENU input_menu = NULL;
HMENU output_menu = NULL;
HMENU gen_menu = NULL;



bool bManagerGrid;
ConfBool cbManagerGrid( &bManagerGrid, "ManagerGrid", CONF_MODE_PUBLIC, true );

bool bManagerVisible;
WINDOWPLACEMENT WinPlaceManager;

void WinPlaceManagerCallback( ConfVar * var )
{
	if( !IsWindow( WindowManager ) ) return;

	GetWindowPlacement( WindowManager, &WinPlaceManager );

	// MSDN:  If the window identified by the hWnd parameter
	//        is maximized, the showCmd member is SW_SHOWMAXIMIZED.
	//        If the window is minimized, showCmd is SW_SHOWMINIMIZED.
	//        Otherwise, it is SW_SHOWNORMAL.
	if( !bManagerVisible )
	{
		WinPlaceManager.showCmd = SW_HIDE;
	}
}

RECT rManagerDefault = { 500, 400, 1000, 700 };

ConfWinPlaceCallback cwpcWinPlaceManager(
	&WinPlaceManager,
	TEXT( "WinPlaceManager" ),
	&rManagerDefault,
	WinPlaceManagerCallback
);



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
#define CLASSNAME_PAPMC  TEXT( "PLAINAMP_PMC" )
#define PAPMC_TITLE      TEXT( "Plugin Manager" )

bool PluginManager::Build()
{
	LoadCommonControls();

	// Register class
	WNDCLASS wc = {
		0,                                     // UINT style
		WndprocManager,                           // WNDPROC lpfnWndProc
    	0,                                     // int cbClsExtra
    	0,                                     // int cbWndExtra
    	g_hInstance,                           // HINSTANCE hInstance
    	NULL,  // HICON hIcon
    	LoadCursor( NULL, IDC_ARROW ),         // HCURSOR hCursor
    	( HBRUSH )COLOR_WINDOW,                // HBRUSH hbrBackground
    	NULL,                                  // LPCTSTR lpszMenuName
    	CLASSNAME_PAPMC                        // LPCTSTR lpszClassName
	};
		
	if( !RegisterClass( &wc ) ) return false;

	WindowManager = CreateWindowEx(
		WS_EX_TOOLWINDOW,                              // DWORD dwExStyle
		CLASSNAME_PAPMC,                               // LPCTSTR lpClassName
		PAPMC_TITLE,                                   // LPCTSTR lpWindowName
		WS_OVERLAPPEDWINDOW |                          // DWORD dwStyle
			WS_CLIPCHILDREN,                            //
		rManagerDefault.left,                          // int x
		rManagerDefault.top,                           // int y
		rManagerDefault.right - rManagerDefault.left,  // int nWidth
		rManagerDefault.bottom - rManagerDefault.top,  // int nHeight
		NULL,                                          // HWND hWndParent
		NULL,                                          // HMENU hMenu
		g_hInstance,                                   // HINSTANCE hInstance
		NULL                                           // LPVOID lpParam
	);

	RECT r;
	GetClientRect( WindowManager, &r );
	
	WindowListView = CreateWindowEx(
		WS_EX_CLIENTEDGE,                              // DWORD dwExStyle
		WC_LISTVIEW,                                   // LPCTSTR lpClassName
		NULL,                                          // LPCTSTR lpWindowName
		WS_VSCROLL |                                   // DWORD dwStyle
			LVS_REPORT  |                              //
			LVS_SORTASCENDING |                        //
			LVS_SINGLESEL |                            //
			WS_CHILD |                                 //
			WS_VISIBLE,                                //
		0,                                             // int x
		0,                                             // int y
		r.right - r.left,                              // int nWidth
		r.bottom - r.top,                              // int nHeight
		WindowManager,                                 // HWND hWndParent
		NULL,                                          // HMENU hMenu
		g_hInstance,                                   // HINSTANCE hInstance
		NULL                                           // LPVOID lpParam
	);
	
	if( !WindowListView ) return false;

	ListView_SetExtendedListViewStyle(
		WindowListView,
	    LVS_EX_FULLROWSELECT  |
			( bManagerGrid ? LVS_EX_GRIDLINES : 0 ) |
			LVS_EX_HEADERDRAGDROP
	);

	// (0) File
	LVCOLUMN lvc = {
		LVCF_TEXT | LVCF_WIDTH | LVCF_FMT,  // UINT mask; 
		LVCFMT_LEFT,                        // int fmt; 
		100,                                // int cx; 
		TEXT( "File" ),                     // LPTSTR pszText; 
		5,                                  // int cchTextMax; 
		0,                                  // int iSubItem; 
		0,                                  // int iOrder;
		0                                   // int iImage;
	};
	ListView_InsertColumn( WindowListView, 0, &lvc );
	
	// (1) Status
	lvc.pszText     = TEXT( "Status" );
	lvc.cchTextMax  = 7;
	ListView_InsertColumn( WindowListView, 1, &lvc );

	// (2) Name
	lvc.cx          = 220;
	lvc.pszText     = TEXT( "Name" );
	lvc.cchTextMax  = 5;
	ListView_InsertColumn( WindowListView, 2, &lvc );
		
	
	// (3) Type
	lvc.pszText     = TEXT( "Type" );
	lvc.cchTextMax  = 5;
	ListView_InsertColumn( WindowListView, 3, &lvc );


	bManagerVisible = ( WinPlaceManager.showCmd != SW_HIDE );
	SetWindowPlacement( WindowManager, &WinPlaceManager );


	// Exchange window procedure
	WndprocListViewBackup = ( WNDPROC )GetWindowLong( WindowListView, GWL_WNDPROC );
	if( WndprocListViewBackup != NULL )
	{
		SetWindowLong( WindowListView, GWL_WNDPROC, ( LONG )WndprocListView );
	}


	// Build context menu
	input_menu = CreatePopupMenu();
	AppendMenu( input_menu, MF_STRING, MENU_INPUT_CONFIG, TEXT( "Config" ) );
	AppendMenu( input_menu, MF_STRING, MENU_INPUT_ABOUT,  TEXT( "About" ) );

	output_menu = CreatePopupMenu();
	AppendMenu( output_menu, MF_STRING, MENU_OUTPUT_CONFIG, TEXT( "Config" ) );
	AppendMenu( output_menu, MF_STRING, MENU_OUTPUT_ABOUT,  TEXT( "About" ) );
	AppendMenu( output_menu, MF_SEPARATOR | MF_GRAYED | MF_DISABLED, MENU_OUTPUT_SEP, NULL );
	AppendMenu( output_menu, MF_STRING, MENU_OUTPUT_ACTIVE, TEXT( "Active" ) );
	AppendMenu( output_menu, MF_STRING, MENU_OUTPUT_LOAD, TEXT( " " ) );

	gen_menu = CreatePopupMenu();
	AppendMenu( gen_menu, MF_STRING, MENU_GEN_CONFIG, TEXT( "Config" ) );
	AppendMenu( gen_menu, MF_SEPARATOR | MF_GRAYED | MF_DISABLED, MENU_GEN_SEP, NULL );
	AppendMenu( gen_menu, MF_STRING, MENU_GEN_LOAD, TEXT( " " ) );

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool PluginManager::Fill()
{
	if( !WindowListView ) return false;
	
	LVITEM lvi;
	memset( &lvi, 0, sizeof( LVITEM ) );
	lvi.mask = LVIF_TEXT | LVIF_PARAM;

	Plugin * plugin;
	int iIndex;
	vector<Plugin *>::iterator iter = plugins.begin();
	while( iter != plugins.end() )
	{
lvi.mask = LVIF_TEXT | LVIF_PARAM;

		plugin          = *iter;
		lvi.iItem       = 0;
		lvi.lParam      = ( LPARAM )plugin;

		// (0) File		

		lvi.iSubItem    = 0;
		lvi.pszText     = plugin->GetFilename();
		lvi.cchTextMax  = plugin->GetFilenameLen() + 1;
		iIndex = ListView_InsertItem( WindowListView, &lvi );
		lvi.iItem       = iIndex;
lvi.mask = LVIF_TEXT;
		
		// (1) Status
		lvi.iSubItem    = 1;
		if( plugin->IsLoaded() )
		{
			if( plugin->IsActive() )
			{
				lvi.pszText     = TEXT( "Active" );
			}
			else
			{
				lvi.pszText     = TEXT( "Loaded" );
			}
			lvi.cchTextMax      = 7;
		}
		else
		{
			lvi.pszText     = TEXT( "Not loaded" );
			lvi.cchTextMax  = 11;
		}
		ListView_SetItem( WindowListView, &lvi );

		// (2) Name
		lvi.iSubItem    = 2;
		lvi.pszText     = plugin->GetName();
		lvi.cchTextMax  = plugin->GetNameLen() + 1;
		ListView_SetItem( WindowListView, &lvi );
		
		// (3) Type
		lvi.iSubItem    = 3;
		lvi.pszText     = plugin->GetTypeString();
		lvi.cchTextMax  = plugin->GetTypeStringLen() + 1;
		ListView_SetItem( WindowListView, &lvi );
		
		iter++;
	}

	return true;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool PluginManager::Destroy()
{
	if( !WindowListView ) return false;
	
	DestroyWindow( WindowManager );
	DestroyMenu( input_menu	);
	DestroyMenu( output_menu );
	DestroyMenu( gen_menu );

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool PluginManager::Popup()
{
	if( !WindowListView ) return false;	
	if( !IsWindowVisible( WindowManager ) )
		ShowWindow( WindowManager, SW_SHOW );
	
	SetActiveWindow( WindowManager );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void UpdatePluginStatus( Plugin * plugin, bool bLoaded, bool bActive )
{
	LVFINDINFO lvfi = {
		LVFI_PARAM,        // UINT flags
		NULL,              // LPCTSTR psz
		( LPARAM )plugin,  // LPARAM lParam
		POINT(),           // POINT pt
		0                  // UINT vkDirection
	};
	
	int iIndex = ListView_FindItem( WindowListView, -1, &lvfi );
	if( iIndex != -1 )
	{
		LVITEM lvi;
		memset( &lvi, 0, sizeof( LVITEM ) );
		lvi.mask = LVIF_TEXT;
		lvi.iItem       = iIndex;
		lvi.iSubItem    = 1;
		if( bLoaded )
		{
			if( bActive )
			{
				lvi.pszText     = TEXT( "Active" );
			}
			else
			{
				lvi.pszText     = TEXT( "Loaded" );
			}
			lvi.cchTextMax      = 7;
		}
		else
		{
				lvi.pszText     = TEXT( "Not loaded" );
				lvi.cchTextMax  = 11;
		}
		ListView_SetItem( WindowListView, &lvi );
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuInput( InputPlugin * input, POINT * p )
{
	BOOL iIndex = TrackPopupMenu(
		input_menu,
		TPM_LEFTALIGN |
			TPM_TOPALIGN |
			TPM_NONOTIFY |
			TPM_RETURNCMD |
			TPM_RIGHTBUTTON,
		p->x,
		p->y,
		0,
		WindowManager,
		NULL
	);
	
	switch( iIndex )
	{
	case MENU_INPUT_ABOUT:
		input->About( WindowManager );
		break;

	case MENU_INPUT_CONFIG:
		input->Config( WindowManager );
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuOutput( OutputPlugin * output, POINT * p )
{
	const bool bLoaded = output->IsLoaded();
	const bool bActive = output->IsActive();

	EnableMenuItem(
		output_menu,
		MENU_OUTPUT_CONFIG,
		bLoaded ? MF_ENABLED : MF_GRAYED | MF_DISABLED
	);
	EnableMenuItem(
		output_menu,
		MENU_OUTPUT_ABOUT,
		bLoaded ? MF_ENABLED : MF_GRAYED | MF_DISABLED
	);
	EnableMenuItem(
		output_menu,
		MENU_OUTPUT_ACTIVE,
		bLoaded ? MF_ENABLED : MF_GRAYED | MF_DISABLED
	);
	CheckMenuItem(
		output_menu,
		MENU_OUTPUT_ACTIVE,
		bActive ? MF_CHECKED : MF_UNCHECKED
	);
	

	MENUITEMINFO mii = { 0 };
	mii.cbSize      = sizeof( MENUITEMINFO );
	mii.fMask       = MIIM_STATE | MIIM_STRING;
	mii.fState      = bActive ? MFS_GRAYED | MFS_DISABLED : MFS_ENABLED;
	mii.dwTypeData  = ( LPTSTR )( bLoaded ? TEXT( "Unload" ) : TEXT( "Load" ) );
	mii.cch         = bLoaded ? 7 : 5;
	
	SetMenuItemInfo(
		output_menu,       // HMENU hMenu
		MENU_OUTPUT_LOAD,  // UINT uItem
		FALSE,             // BOOL fByPosition,
		&mii               // LPMENUITEMINFO lpmii
	);
	

	BOOL iIndex = TrackPopupMenu(
		output_menu,
		TPM_LEFTALIGN |
			TPM_TOPALIGN |
			TPM_NONOTIFY |
			TPM_RETURNCMD |
			TPM_RIGHTBUTTON,
		p->x,
		p->y,
		0,
		WindowManager,
		NULL
	);

	switch( iIndex )
	{
	case MENU_OUTPUT_CONFIG:
		output->Config( WindowManager );
		break;

	case MENU_OUTPUT_ABOUT:
		output->About( WindowManager );
		break;

	case MENU_OUTPUT_ACTIVE:
		{
			if( Playback::IsPlaying() )
			{
				MessageBox( WindowManager, TEXT( "Cannot do this while playing!" ), TEXT( "Error" ), MB_ICONERROR );
				break;
			}
			
			if( bActive )
				output->Stop();
			else
				output->Start();
			
				const bool bActiveNew = output->IsActive();
				if( bActiveNew != bActive )
				{
					UpdatePluginStatus( output, bLoaded, bActiveNew );
				}

			break;
		}

	case MENU_OUTPUT_LOAD:
		{
			if( bLoaded )
				output->Unload();
			else
				output->Load();
				
			const bool bLoadedNew = output->IsLoaded();
			if( bLoadedNew != bLoaded )
			{
				UpdatePluginStatus( output, bLoadedNew, bActive );
			}

			break;
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuVis( VisPlugin * vis, POINT * p )
{
	HMENU vis_menu = CreatePopupMenu();

	const bool bLoaded = vis->IsLoaded();
	const bool bPluginActive = vis->IsActive();

	VisModule * mod;
	HMENU * menus = NULL;

	const int iModCount = ( int )vis->modules.size();
	if( iModCount == 1 )
	{
		// Single module
		mod = vis->modules[ 0 ];
		const UINT uFlagsActive = MF_STRING |
			( mod->IsActive() ? MF_CHECKED : MF_UNCHECKED ) |
			( bLoaded ? MF_ENABLED : MF_DISABLED | MF_GRAYED );
		const UINT uFlagsConfig = MF_STRING |
			( bLoaded ? MF_ENABLED : MF_DISABLED | MF_GRAYED );
		AppendMenu( vis_menu, uFlagsActive, 1, TEXT( "Active" ) );
		AppendMenu( vis_menu, uFlagsConfig, 2, TEXT( "Config" ) );
	}
	else
	{
		// Two or more	
		menus = new HMENU[ iModCount ];
		int iIndex = 0;
		vector <VisModule *>::iterator iter = vis->modules.begin();
		while( iter != vis->modules.end() )
		{
			mod = *iter;

			menus[ iIndex ] = CreatePopupMenu();
			UINT uFlags = MF_STRING | ( mod->IsActive() ? MF_CHECKED : MF_UNCHECKED );
			AppendMenu( menus[ iIndex ], uFlags,    iIndex * 2 + 1, TEXT( "Active" ) );
			AppendMenu( menus[ iIndex ], MF_STRING, iIndex * 2 + 2, TEXT( "Config" ) );

			uFlags = MF_STRING | MF_POPUP | ( ( !bLoaded || ( bPluginActive && !mod->IsActive() ) ) ? MF_GRAYED | MF_DISABLED : 0 );
			AppendMenu( vis_menu, uFlags, ( UINT_PTR )menus[ iIndex ], mod->GetName() );
			
			iIndex++;
			iter++;	
		}
	}



	const int iPos = ( int )vis->modules.size() * 2 + 1;
	AppendMenu( vis_menu, MF_SEPARATOR | MF_GRAYED | MF_DISABLED, iPos, NULL );
	
	AppendMenu(
		vis_menu,
		MF_STRING | ( bPluginActive ? MF_DISABLED | MF_GRAYED : MF_ENABLED ),
		iPos + 1,
		bLoaded ? TEXT( "Unload" ) : TEXT( "Load" )
	);

	
	BOOL iIndex = TrackPopupMenu(
		vis_menu,                // HMENU hMenu
		TPM_LEFTALIGN |          // UINT uFlags
			TPM_TOPALIGN |       // .
			TPM_NONOTIFY |       // .
			TPM_RETURNCMD |      // .
			TPM_RIGHTBUTTON,     // .
		p->x,                    // int x
		p->y,                    // int y
		0,                       // int nReserved
		WindowManager,           // HWND hWnd
		NULL                     // HWND prcRect
	);
	
	if( iIndex )
	{
		if( iIndex >= iPos )
		{
			// Load/unload
			if( bLoaded )
				vis->Unload();
			else
				vis->Load();
				
			const bool bLoadedNew = vis->IsLoaded();
				
			if( bLoadedNew != bLoaded )
				UpdatePluginStatus( vis, bLoadedNew, FALSE );
		}
		else
		{
			int iWhich = ( iIndex - 1 ) / 2;
			if( iIndex & 1 )
			{
				// Active
				const bool bPluginActive = vis->IsActive();
				
				if( !vis->modules[ iWhich ]->IsActive() )
					vis->modules[ iWhich ]->Start();
				else
					vis->modules[ iWhich ]->Stop();
					
				const bool bPluginActiveNew = vis->IsActive();
				
				if( bPluginActiveNew && ( bPluginActiveNew != bPluginActive ) )
					UpdatePluginStatus( vis, TRUE, bPluginActiveNew );
			}
			else
			{
				// Config	
				vis->modules[ iWhich ]->Config();
			}
		}
	}
	
	DestroyMenu( vis_menu );
	if( iModCount > 1 )
	{
		for( int i = 0; i < iModCount; i++ )
			DestroyMenu( menus[ i ] );
		delete [] menus;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuDsp( DspPlugin * dsp, POINT * p )
{
	HMENU context_menu = CreatePopupMenu();
	
	const bool bLoaded = dsp->IsLoaded();
	const bool bPluginActive = dsp->IsActive();
	
	DspModule * mod;

	HMENU * menus = NULL;
	
	HMENU * add_rem_menus = NULL;

	const int iEach = ( 3 * active_dsp_count + 2 );

	const int iModCount = ( int )dsp->modules.size();
	if( iModCount == 1 )
	{
		// Single module
		mod = dsp->modules[ 0 ];
		
		add_rem_menus = new HMENU[ 2 ];
		add_rem_menus[ 0 ] = CreatePopupMenu();
		add_rem_menus[ 1 ] = CreatePopupMenu();

		int i;
		TCHAR szHere[ 12 ];
		for( i = 0; i < active_dsp_count; i++ )
		{
			TCHAR * szName = active_dsp_mods[ i ]->GetName();
			
			// Entry for "Add"
			_stprintf( szHere, TEXT( "Here (%i)" ), i + 1 );
			AppendMenu(
				add_rem_menus[ 0 ],
				MF_STRING,
				2 * i + 1,
				szHere
			);
			AppendMenu(
				add_rem_menus[ 0 ],
				MF_STRING | MF_DISABLED | MF_GRAYED,
				2 * i + 2,
				szName
			);
			
			// Entry for "Remove"
			AppendMenu(
				add_rem_menus[ 1 ],
				MF_STRING,
				2 * active_dsp_count + 2 + i,
				szName
			);
		}
		_stprintf( szHere, TEXT( "Here (%i)" ), i + 1 );
		AppendMenu(
			add_rem_menus[ 0 ],
			MF_STRING,
			2 * i + 1,
			szHere
		);


		// Main entries
		const UINT uFlagsAdd = MF_STRING | MF_POPUP |
			( ( bLoaded && !mod->IsActive() ) ? 0 : MF_DISABLED | MF_GRAYED );
		const UINT uFlagsRemove = MF_STRING | MF_POPUP |
			( ( bLoaded && active_dsp_count ) ? 0 : MF_GRAYED | MF_DISABLED );
		const UINT uFlagsConfig = MF_STRING |
			( ( bLoaded && mod->IsActive() ) ? 0 : MF_DISABLED | MF_GRAYED );

		AppendMenu(
			context_menu,
			uFlagsAdd,
			( UINT_PTR )add_rem_menus[ 0 ],
			TEXT( "Add" )
		);
		AppendMenu(
			context_menu,
			uFlagsRemove,
			( UINT_PTR )add_rem_menus[ 1 ],
			TEXT( "Remove" )
		);
		AppendMenu(
			context_menu,
			uFlagsConfig,
			3 * active_dsp_count + 1 + 1,
			TEXT( "Config" )
		);
	}
	else
	{
		// Two or more	
		menus = new HMENU[ iModCount ];
		add_rem_menus = new HMENU[ 2 * iModCount ];

		int iIndex = 0;
		vector <DspModule *>::iterator iter = dsp->modules.begin();
		while( iter != dsp->modules.end() )
		{
			mod = *iter;

			menus[ iIndex ] = CreatePopupMenu();
			
			add_rem_menus[ 2 * iIndex     ] = CreatePopupMenu();
			add_rem_menus[ 2 * iIndex + 1 ] = CreatePopupMenu();

			int i;
			TCHAR szHere[ 12 ];
			for( i = 0; i < active_dsp_count; i++ )
			{
				TCHAR * szName = active_dsp_mods[ i ]->GetName();
				
				// Entry for "Add"
				_stprintf( szHere, TEXT( "Here (%i)" ), i + 1 );
				AppendMenu(
					add_rem_menus[ 2 * iIndex     ],
					MF_STRING,
					( iIndex * iEach ) + ( 2 * i + 1 ),
					szHere
				);
				AppendMenu(
					add_rem_menus[ 2 * iIndex     ],
					MF_STRING | MF_DISABLED | MF_GRAYED,
					( iIndex * iEach ) + ( 2 * i + 2 ),
					szName
				);
				
				// Entry for "Remove"
				AppendMenu(
					add_rem_menus[ 2 * iIndex + 1 ],
					MF_STRING,
					( iIndex * iEach ) + ( 2 * active_dsp_count + 2 + i ),
					szName
				);
			}
			_stprintf( szHere, TEXT( "Here (%i)" ), i + 1 );
			AppendMenu(
				add_rem_menus[ 2 * iIndex ],
				MF_STRING,
				( iIndex * iEach ) + ( 2 * i + 1 ),
				szHere
			);
				
			AppendMenu(
				menus[ iIndex ],
				MF_STRING | MF_POPUP | ( mod->IsActive() ? MF_DISABLED | MF_GRAYED : 0 ),
				( UINT_PTR )add_rem_menus[ 2 * iIndex ],
				TEXT( "Add" )
			);
			AppendMenu(
				menus[ iIndex ],
				MF_STRING | MF_POPUP | ( active_dsp_count ? 0 : MF_GRAYED | MF_DISABLED ),
				( UINT_PTR )add_rem_menus[ 2 * iIndex + 1 ],
				TEXT( "Remove" )
			);
			AppendMenu(
				menus[ iIndex ],
				MF_STRING | ( mod->IsActive() ? 0 : MF_DISABLED | MF_GRAYED ),
				( iIndex * iEach ) + ( 3 * active_dsp_count + 1 + 1 ),
				TEXT( "Config" )
			);
			AppendMenu( context_menu, MF_STRING | MF_POPUP | ( bLoaded ? 0 : ( MF_DISABLED | MF_GRAYED ) ), ( UINT_PTR )menus[ iIndex ], mod->GetName() );
			
			iIndex++;
			iter++;	
		}
	}

	
	const int iPos = ( int )dsp->modules.size() * ( 3 * active_dsp_count + 1 + 1 ) + 1;
	AppendMenu( context_menu, MF_SEPARATOR | MF_GRAYED | MF_DISABLED, iPos, NULL );
	
	UINT uFlags = MF_STRING;

	AppendMenu(
		context_menu,
		uFlags | ( bPluginActive ? MF_DISABLED | MF_GRAYED : MF_ENABLED ),
		iPos + 1,
		bLoaded ? TEXT( "Unload" ) : TEXT( "Load" )
	);

	
	BOOL iIndex = TrackPopupMenu(
		context_menu,            // HMENU hMenu
		TPM_LEFTALIGN |          // UINT uFlags
			TPM_TOPALIGN |       // .
			TPM_NONOTIFY |       // .
			TPM_RETURNCMD |      // .
			TPM_RIGHTBUTTON,     // .
		p->x,                    // int x
		p->y,                    // int y
		0,                       // int nReserved
		WindowManager,           // HWND hWnd
		NULL                     // HWND prcRect
	);

	/*

	Example menu

	Mod1
		Add
			Here     [ 1]
			Active1  [ 2]
			Here     [ 3]
			Active2  [ 4]
			Here     [ 5]
		Remove
			Active1  [ 6]
			Active2  [ 7]
		Config       [ 8]
	Mod2
		Add
			Here     [ 9]
			Active1  [10]
			...
	---
	Load/Unload      [11]

	*/


	if( iIndex )
	{
		if( iIndex >= iPos )
		{
			// Load/unload
			if( bLoaded )
				dsp->Unload();
			else
				dsp->Load();
				
			const bool bLoadedNew = dsp->IsLoaded();
				
			if( bLoadedNew != bLoaded )
				UpdatePluginStatus( dsp, bLoadedNew, FALSE );
		}
		else
		{
			int iWhichMod = ( iIndex - 1 ) / iEach;
			int iRem = iIndex % iEach;
			if( iRem == 0 )
			{
				// Config
				dsp->modules[ iWhichMod ]->Config();

			}
			else if( iRem < 2 * active_dsp_count + 2 )
			{
				// Add module
				const bool bPluginActive = dsp->IsActive();
				
				dsp->modules[ iWhichMod ]->Start( iRem / 2 );
				
				const bool bPluginActiveNew = dsp->IsActive();
				if( bPluginActiveNew != bPluginActive )
				{
					UpdatePluginStatus( dsp, TRUE, bPluginActiveNew );
				}
			}
			else
			{
				// Remove module
				const bool bPluginActive = dsp->IsActive();
				
				iWhichMod = iRem - ( 2 * active_dsp_count + 2 );
/*
				TCHAR szBuffer[ 5000 ];
				_stprintf( szBuffer, TEXT( "REM <%i> <%i>" ), iIndex, iWhichMod );
				Console::Append( szBuffer );
*/
				active_dsp_mods[ iWhichMod ]->Stop();

				const bool bPluginActiveNew = dsp->IsActive();
				if( bPluginActiveNew != bPluginActive )
				{
					UpdatePluginStatus( dsp, TRUE, bPluginActiveNew );
				}
			}
		}
	}

	DestroyMenu( context_menu );
	if( iModCount > 1 )
	{
		for( int i = 0; i < iModCount; i++ )
			DestroyMenu( menus[ i ] );
		delete [] menus;
	}
	
	for( int j = 0; j < iModCount; j++ )
		DestroyMenu( add_rem_menus[ j ] );
	delete [] add_rem_menus;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuGen( GenPlugin * gen, POINT * p )
{
	const bool bLoaded = gen->IsLoaded();

	EnableMenuItem(
		gen_menu,
		MENU_GEN_CONFIG,
		bLoaded ? MF_ENABLED : MF_GRAYED | MF_DISABLED
	);


	MENUITEMINFO mii = { 0 };
	mii.cbSize      = sizeof( MENUITEMINFO );
	mii.fMask       = MIIM_STATE | MIIM_STRING;
	mii.fState      = gen->AllowRuntimeUnload() ? MFS_ENABLED : MFS_GRAYED | MFS_DISABLED;
	mii.dwTypeData  = ( LPTSTR )( bLoaded ? TEXT( "Unload" ) : TEXT( "Load and activate" ) );
	mii.cch         = bLoaded ? 7 : 18;
	
	SetMenuItemInfo(
		gen_menu,          // HMENU hMenu
		MENU_GEN_LOAD,     // UINT uItem
		FALSE,             // BOOL fByPosition,
		&mii               // LPMENUITEMINFO lpmii
	);
	

	BOOL iIndex = TrackPopupMenu(
		gen_menu,
		TPM_LEFTALIGN |
			TPM_TOPALIGN |
			TPM_NONOTIFY |
			TPM_RETURNCMD |
			TPM_RIGHTBUTTON,
		p->x,
		p->y,
		0,
		WindowManager,
		NULL
	);

	switch( iIndex )
	{
	case MENU_GEN_CONFIG:
		gen->Config();
		break;

	case MENU_GEN_LOAD:
		{
			if( bLoaded )
				gen->Unload();
			else
				gen->Load();
				
			const bool bLoadedNew = gen->IsLoaded();
			if( bLoadedNew != bLoaded )
			{
				UpdatePluginStatus( gen, bLoadedNew, bLoadedNew );
			}

			break;
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenu( Plugin * plugin, POINT * p )
{
	if( !plugin ) return;
	
	switch( plugin->GetType() )
	{
	case PLUGIN_TYPE_INPUT:
		ContextMenuInput( ( InputPlugin * )plugin, p );
		break;

	case PLUGIN_TYPE_OUTPUT:
		ContextMenuOutput( ( OutputPlugin * )plugin, p );
		break;

	case PLUGIN_TYPE_VIS:
		ContextMenuVis( ( VisPlugin * )plugin, p );
		break;

	case PLUGIN_TYPE_DSP:
		ContextMenuDsp( ( DspPlugin * )plugin, p );
		break;

	case PLUGIN_TYPE_GEN:
		ContextMenuGen( ( GenPlugin * )plugin, p );
		break;
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocListView( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	static bool bContextOpen = false;

	switch( message )
	{
	case WM_CONTEXTMENU:
		{
			if( ( HWND )wp != WindowListView ) break;
			if( bContextOpen ) break;
			bContextOpen = true;
////////////////////////////////////////////////////////////////////////////////
			
			POINT p = { LOWORD( lp ), HIWORD( lp ) };
			ScreenToClient( WindowListView, &p );
	
			// Which item?
			LVHITTESTINFO hti;
			memset( &hti, 0, sizeof( LVHITTESTINFO ) );
			hti.pt = p;
			ListView_HitTest( WindowListView, &hti );
		
			// Which plugin?
			LVITEM lvi;
			memset( &lvi, 0, sizeof( LVITEM ) );
			lvi.mask      = LVIF_PARAM;
			lvi.iItem     = hti.iItem;
			lvi.iSubItem  = 0;
			
			if( !ListView_GetItem( WindowListView, &lvi ) )
			{
				bContextOpen = false;
				break;
			}
			Plugin * plugin = ( Plugin * )lvi.lParam;
	
			// Context menu
			p.x = LOWORD( lp );
			p.y = HIWORD( lp );
			ContextMenu( plugin, &p );
	
////////////////////////////////////////////////////////////////////////////////
			bContextOpen = false;
			break;
		}
	}
	return CallWindowProc( WndprocListViewBackup, hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocManager( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
	case WM_SIZE:
		{
			// Resize children
			RECT client;
			GetClientRect( WindowManager, &client );
		
			if( WindowListView )
				MoveWindow( WindowListView, 0, 0, client.right - client.left, client.bottom - client.top, TRUE );
			break;
		}
		
	case WM_SYSCOMMAND:
		if( ( wp & 0xFFF0 ) == SC_CLOSE )
		{
			ShowWindow( hwnd, SW_HIDE );
			return 0;
		}
		break;

	case WM_DESTROY:
		cwpcWinPlaceManager.TriggerCallback();
		cwpcWinPlaceManager.RemoveCallback();
		break;

	case WM_SHOWWINDOW:
		bManagerVisible = ( wp == TRUE );
		break;
	}
	return DefWindowProc( hwnd, message, wp, lp );
}
