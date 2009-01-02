////////////////////////////////////////////////////////////////////////////////
// Plainamp Toolbar Vis Plugin
// 
// Copyright © 2006  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#define WIN32_LEAN_AND_MEAN
#include <windows.h>


#include "../Winamp/vis.h"
#include "../Winamp/wa_ipc.h"



#define PLUGIN_NAME     "Plainamp Toolbar Vis Plugin"
#define PLUGIN_VERSION  "v0.5"

#define PLUGIN_DESC     PLUGIN_NAME " " PLUGIN_VERSION



static char * szClassName = "PlainbarClass";



winampVisModule * getModule( int which );


void config( struct winampVisModule * this_mod );
int init( struct winampVisModule * this_mod );
int render_spec( struct winampVisModule * this_mod );
void quit( struct winampVisModule * this_mod );


// Double buffering data
HDC memDC = NULL;      // Memory device context
HBITMAP	memBM = NULL;  // Memory bitmap (for memDC)
HBITMAP	oldBM = NULL;  // Old bitmap (from memDC)


HWND hRender = NULL;
int width = 0;  
int height = 0;  
bool bRunning = false;
HPEN pen = NULL;


WNDPROC WndprocTargetBackup = NULL;
LRESULT CALLBACK WndprocTarget( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );



winampVisHeader hdr = {
	VIS_HDRVER,
	PLUGIN_DESC,
	getModule
};



winampVisModule mod_spec =
{
	"Default",
	NULL,	// hwndParent
	NULL,	// hDllInstance
	0,		// sRate
	0,		// nCh
	25,		// latencyMS
	25,		// delayMS
	2,		// spectrumNch
	0,		// waveformNch
	{ 0, },	// spectrumData
	{ 0, },	// waveformData
	config,
	init,
	render_spec, 
	quit
};



#ifdef __cplusplus
extern "C" {
#endif
__declspec( dllexport ) winampVisHeader * winampVisGetHeader()
{
	return &hdr;
}
#ifdef __cplusplus
}
#endif



winampVisModule * getModule( int which )
{
	return which ? NULL : &mod_spec;
}



void config( struct winampVisModule * this_mod )
{
	MessageBox(
		this_mod->hwndParent,
		PLUGIN_DESC "\n"
		"\n"
		"Copyright © 2006 Sebastian Pipping  \n"
		"<webmaster@hartwork.org>\n"
		"\n"
		"-->  http://www.hartwork.org",
		"About",
		MB_ICONINFORMATION
	);
}



LRESULT CALLBACK WndprocTarget( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
	case WM_SIZE:
		width = LOWORD( lp );
		height = HIWORD( lp );
		break;

	case WM_DESTROY:
		bRunning = false;
		PostQuitMessage( 0 );
		return 0;
	
	}
	
	return DefWindowProc( hwnd, message, wp, lp );
}



int init( struct winampVisModule * this_mod )
{
	if( !this_mod ) return 1;

	// Register message
	const int IPC_GETPLAINBARTARGET = ( int )SendMessage( this_mod->hwndParent, WM_WA_IPC, ( WPARAM )"IPC_GETPLAINBARTARGET", IPC_REGISTER_WINAMP_IPCMESSAGE );
	if( IPC_GETPLAINBARTARGET <= 0 ) return 1;
	
	// Get render parent
	HWND hRenderParent = ( HWND )SendMessage( this_mod->hwndParent, WM_WA_IPC, 0, IPC_GETPLAINBARTARGET );
	if( !IsWindow( hRenderParent ) ) return 1;

	// Plug our child in
	WNDCLASS wc;
	ZeroMemory( &wc, sizeof( WNDCLASS ) );
	wc.lpfnWndProc    = WndprocTarget;
	wc.hInstance      = this_mod->hDllInstance;
	wc.hCursor        = LoadCursor( NULL, IDC_ARROW );
	wc.lpszClassName  = szClassName;

	if( !RegisterClass( &wc ) ) return 1;

	RECT r;
	GetClientRect( hRenderParent, &r );
	width = r.right - r.left;
	height = r.bottom - r.top;

	hRender = CreateWindowEx(
		0,
		szClassName,
		"",
		WS_CHILD | WS_VISIBLE,
		0,
		0,
		width,
		height,
		hRenderParent,
		NULL,
		this_mod->hDllInstance,
		0
	);
	
	if( !hRender )
	{
		UnregisterClass( szClassName, this_mod->hDllInstance );
		return 1;
	}
	
	// Create doublebuffer
	const HDC hdc = GetDC( hRender );
		memDC = CreateCompatibleDC( hdc );
		memBM = CreateCompatibleBitmap( hdc, 576, 256 );
		oldBM = ( HBITMAP )SelectObject( memDC, memBM );
	ReleaseDC( hRender, hdc );

	pen = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_APPWORKSPACE ) );

	bRunning = true;
	return 0;
}



int render_spec( struct winampVisModule * this_mod )
{
	// Clear background
	RECT rect = { 0, 0, 576, 256 };
	FillRect(memDC, &rect, ( HBRUSH )( COLOR_3DFACE + 1 ) );
	
	// Draw analyser
	SelectObject( memDC, pen );
	for( int x = 0; x < 576; x++ )
	{
		int val = 0;
		
		for( int y = 0; y < this_mod->nCh; y++ )
		{
			if( this_mod->spectrumData[ y ][ x ] > val )
			{
				val = this_mod->spectrumData[ y ][ x ];
			}
		}
		
		MoveToEx( memDC, x, 256, NULL );
		LineTo( memDC, x, 256 - val );
	}

	// Copy doublebuffer to window
	HDC hdc = GetDC( hRender );
		StretchBlt( hdc, 0, 0, width, height, memDC, 0, 0, 576, 256, SRCCOPY );
	ReleaseDC( hRender, hdc );

	return bRunning ? 0 : 1;
}



void quit( struct winampVisModule * this_mod )
{
	if( bRunning )
	{
		DestroyWindow( hRender );
	}
	
	UnregisterClass( szClassName, this_mod->hDllInstance );
	
	// Delete doublebuffer
	SelectObject( memDC, oldBM );
	DeleteObject( memDC );
	DeleteObject( memBM );
	
	DeleteObject( pen );
}
