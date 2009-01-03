

// =======================================================================================
// WndprocRebar is called once every second during playback
// =======================================================================================
// In Toolbar::Create() the progress bar is called the Seekbar




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


#include "Rebar.h"
#include "Font.h"
#include "Main.h"
#include "Playlist.h"
#include "Playback.h"
#include "Console.h" 
#include "Util.h"
#include "Config.h"

#include "Winamp/wa_ipc.h"
#include "Resources/resrc1.h"



#define BAND_ORDER    1
#define BAND_EQ       2
#define BAND_SEEK     3
#define BAND_VOL      4
#define BAND_PAN      5
#define BAND_BUTTONS  6
#define BAND_VIS      7

#define BAND_FIRST   BAND_ORDER
#define BAND_LAST    BAND_VIS


#define IMAGE_PREV   0
#define IMAGE_PLAY   1
#define IMAGE_PAUSE  2
#define IMAGE_STOP   3
#define IMAGE_NEXT   4
#define IMAGE_OPEN   5


HWND WindowRebar = NULL; // extern
int iRebarHeight = 40; // extern


HMENU rebar_menu = NULL;


WNDPROC WndprocRebarBackup = NULL;
LRESULT CALLBACK WndprocRebar( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );


HWND WindowOrder = NULL; // extern
HWND WindowEq = NULL; // extern


HWND WindowSeek = NULL; // extern
WNDPROC WndprocSeekBackup = NULL;
LRESULT CALLBACK WndprocSeek( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );

HWND WindowVol = NULL;
WNDPROC WndprocVolBackup = NULL;
LRESULT CALLBACK WndprocVol( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );

HWND WindowPan = NULL;
WNDPROC WndprocPanBackup = NULL;
LRESULT CALLBACK WndprocPan( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );

HWND WindowButtons = NULL;

HWND WindowVis = NULL; // extern
WNDPROC WndprocVisBackup = NULL;
LRESULT CALLBACK WndprocVis( HWND hwnd, UINT message, WPARAM wp, LPARAM lp );




bool Rebar_BuildOrderBand();
bool Rebar_BuildEqBand();
bool Rebar_BuildSeekBand();
bool Rebar_BuildVolBand();
bool Rebar_BuildPanBand();
bool Rebar_BuildButtonsBand();
bool Rebar_BuildVisBand();

BandInfo biOrderBand    = { 0 };
BandInfo biEqBand       = { 0 };
BandInfo biSeekBand     = { 0 };
BandInfo biVolBand      = { 0 };
BandInfo biPanBand      = { 0 };
BandInfo biButtonsBand  = { 0 };
BandInfo biVisBand  = { 0 };

const BandInfo biOrderBandDefault    = { 0, 184, true,  true };
const BandInfo biVolBandDefault      = { 1, 450, false, true };
const BandInfo biPanBandDefault      = { 2, 138, false, true };
const BandInfo biEqBandDefault       = { 3, 170, true,  true };
const BandInfo biVisBandDefault      = { 4, 350, false, true };
const BandInfo biButtonsBandDefault  = { 5, 147, true,  true };
const BandInfo biSeekBandDefault     = { 6, 373, false, true };


void BandCallback( ConfVar * var );

ConfBandInfoCallback cbiOrderBand  ( &biOrderBand,   TEXT( "OrderBand" ),   ( BandInfo * )&biOrderBandDefault,   BandCallback );
ConfBandInfoCallback cbiEqBand     ( &biEqBand,      TEXT( "EqBand" ),      ( BandInfo * )&biEqBandDefault,      BandCallback );
ConfBandInfoCallback cbiSeekBand   ( &biSeekBand,    TEXT( "SeekBand" ),    ( BandInfo * )&biSeekBandDefault,    BandCallback );
ConfBandInfoCallback cbiVolBand    ( &biVolBand,     TEXT( "VolBand" ),     ( BandInfo * )&biVolBandDefault,     BandCallback );
ConfBandInfoCallback cbiPanBand    ( &biPanBand,     TEXT( "PanBand" ),     ( BandInfo * )&biPanBandDefault,     BandCallback );
ConfBandInfoCallback cbiButtonsBand( &biButtonsBand, TEXT( "ButtonsBand" ), ( BandInfo * )&biButtonsBandDefault, BandCallback );
ConfBandInfoCallback cbiVisBand    ( &biVisBand,     TEXT( "VisBand" ),     ( BandInfo * )&biVisBandDefault,     BandCallback );

bool bInvertPanSlider;
ConfBool cbInvertPanSlider( &bInvertPanSlider, TEXT( "InvertPanSlider" ), CONF_MODE_PUBLIC, false );


////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void BandCallback( ConfVar * var )
{
	BandInfo * band;
	int id;

	if( var == &cbiOrderBand )
	{
		band  = &biOrderBand;
		id    = BAND_ORDER;
	}
	else if( var == &cbiEqBand )
	{
		band  = &biEqBand;
		id    = BAND_EQ;
	}
	else if( var == &cbiSeekBand )
	{
		band  = &biSeekBand;
		id    = BAND_SEEK;
	}
	else if( var == &cbiVolBand )
	{
		band  = &biVolBand;
		id    = BAND_VOL;
	}
	else if( var == &cbiPanBand )
	{
		band  = &biPanBand;
		id    = BAND_PAN;
	}
	else if( var == &cbiButtonsBand )
	{
		band  = &biButtonsBand;
		id    = BAND_BUTTONS;
	}
	else if( var == &cbiVisBand )
	{
		band  = &biVisBand;
		id    = BAND_VIS;
	}
	else
	{
		return;	
	}
		
	band->m_iIndex = ( int )SendMessage( WindowRebar, RB_IDTOINDEX, id, 0 );
	if( band->m_iIndex == -1 ) return;

	REBARBANDINFO rbbi;
	rbbi.cbSize  = sizeof( REBARBANDINFO );
	rbbi.fMask   = RBBIM_SIZE | RBBIM_STYLE;
	SendMessage( WindowRebar, RB_GETBANDINFO, band->m_iIndex, ( LPARAM )&rbbi );

	band->m_iWidth    = rbbi.cx;
	band->m_bBreak    = ( ( rbbi.fStyle & RBBS_BREAK  ) == RBBS_BREAK  );
	band->m_bVisible  = ( ( rbbi.fStyle & RBBS_HIDDEN ) != RBBS_HIDDEN );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Toolbar::Create()
{
	LoadCommonControls();

	WindowRebar = CreateWindowEx(
		WS_EX_TOOLWINDOW |
			WS_EX_LEFT |
			WS_EX_LTRREADING |
			WS_EX_RIGHTSCROLLBAR,
		REBARCLASSNAME,
		NULL,
		WS_CHILD |
			WS_VISIBLE |
			WS_CLIPSIBLINGS |
			WS_CLIPCHILDREN |
			WS_BORDER |
			RBS_VARHEIGHT |
			RBS_BANDBORDERS |
			RBS_DBLCLKTOGGLE | // RBS_AUTOSIZE
			CCS_TOP |
			CCS_NOPARENTALIGN |
			CCS_NODIVIDER, //  | CCS_ADJUSTABLE,
		0,
		0,
		0,
		0,
		WindowMain,
		NULL,
		g_hInstance,
		NULL
	);

/*
	#define STYLE 1
	#if STYLE == 1
		// Normal
		MoveWindow( rebar, 0, 0, width, 30, TRUE );
	#elif STYLE == 2
		// Left ONLY
		MoveWindow( rebar, 1, 0, width - 1, 30, TRUE );
	#endif
*/

	REBARINFO rbi = {
		sizeof( REBARINFO ),         // UINT cbSize;
		0,                           // UINT fMask
		NULL                         // HIMAGELIST himl
	};

	if( !SendMessage( WindowRebar, RB_SETBARINFO, 0, ( LPARAM )&rbi ) )
	{
		return false;
	}

	// Exchange window procedure
	WndprocRebarBackup = ( WNDPROC )GetWindowLong( WindowRebar, GWL_WNDPROC ); 
	//WndprocRebarBackup = ( WNDPROC )GetWindowLongPtr( WindowRebar, GWLP_WNDPROC ); // 64 bit
	if( WndprocRebarBackup != NULL )
	{
		SetWindowLong( WindowRebar, GWL_WNDPROC, ( LONG )WndprocRebar );
		//SetWindowLongPtr( WindowRebar, GWLP_WNDPROC, ( LONG )WndprocRebar );
	}

	rebar_menu = CreatePopupMenu();
	AppendMenu( rebar_menu, MF_STRING, BAND_BUTTONS, TEXT( "Buttons"       ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_EQ,      TEXT( "Equalizer"     ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_ORDER,   TEXT( "Order"         ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_PAN,     TEXT( "Panning"       ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_SEEK,    TEXT( "Seekbar"       ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_VIS,     TEXT( "Visualization" ) );
	AppendMenu( rebar_menu, MF_STRING, BAND_VOL,     TEXT( "Volume"        ) );
	
	Rebar_BuildOrderBand();
	Rebar_BuildEqBand();
	Rebar_BuildSeekBand();
	Rebar_BuildVolBand();
	Rebar_BuildPanBand();
	Rebar_BuildButtonsBand();
	Rebar_BuildVisBand();

	cbiOrderBand.Apply  ( WindowRebar, BAND_ORDER   );
	cbiEqBand.Apply     ( WindowRebar, BAND_EQ      );
	cbiSeekBand.Apply   ( WindowRebar, BAND_SEEK    );
	cbiVolBand.Apply    ( WindowRebar, BAND_VOL     );
	cbiPanBand.Apply    ( WindowRebar, BAND_PAN     );
	cbiButtonsBand.Apply( WindowRebar, BAND_BUTTONS );
	cbiVisBand.Apply    ( WindowRebar, BAND_VIS     );

	return true;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildOrderBand()
{
	if( !WindowRebar ) return false;
	
	WindowOrder = CreateWindowEx(
		0,
		TEXT( "COMBOBOX" ),
		TEXT( "" ), 
        CBS_HASSTRINGS |
			CBS_DROPDOWNLIST |
			WS_CHILD,
		0,
		0,
		133,
		200,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);
	
	if( !WindowOrder ) return false;
	
	// Add mode names	
	TCHAR * szName;
	for( int i = ORDER_FIRST; i <= ORDER_LAST; i++ )
	{
		szName = Playback::Order::GetModeName( i );
		SendMessage( WindowOrder, CB_ADDSTRING, 0, ( LPARAM )szName );
	}
	
	// Initial value
	SendMessage( WindowOrder, CB_SETCURSEL, Playback::Order::GetCurMode(), 0 );

	RECT rc;
	GetWindowRect( WindowOrder, &rc );
   
	REBARBANDINFO rbBand;
	rbBand.cbSize		= sizeof( REBARBANDINFO );
	rbBand.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbBand.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE |
								( biOrderBand.m_bBreak ? RBBS_BREAK : 0 );

	rbBand.lpText		= TEXT( " Order " );
	rbBand.hwndChild	= WindowOrder;
	rbBand.cxMinChild	= rc.right - rc.left;
	rbBand.cyMinChild	= 21;                      // IMP
	rbBand.cx			= biOrderBand.m_iWidth;
	rbBand.wID			= BAND_ORDER;
	rbBand.cyChild		= 21;
	rbBand.cyMaxChild	= 21;
	rbBand.cyIntegral	= 1;
	rbBand.cxIdeal		= 200;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbBand );
	
	Font::Apply( WindowOrder );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool AddPreset( TCHAR * szPresetName )
{
	const int iLen = _tcslen( szPresetName );
	TCHAR * szFinal = new TCHAR[ iLen + 2 + 1 ];
	szFinal[ 0 ] = TEXT( ' ' );
	szFinal[ iLen + 1 ] = TEXT( ' ' );
	szFinal[ iLen + 2 ] = TEXT( '\0' );
	memcpy( szFinal + 1, szPresetName, iLen * sizeof( TCHAR ) );

	SendMessage( WindowEq, CB_ADDSTRING, 0, ( LPARAM )szFinal );

	delete [] szFinal;
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildEqBand()
{
	if( !WindowRebar ) return false;
	
	WindowEq = CreateWindowEx(
		0,
		TEXT( "COMBOBOX" ),
		TEXT( "" ), 
        CBS_HASSTRINGS |
			CBS_DROPDOWNLIST |
			WS_CHILD,
		0,
		0,
		133,
		300,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);
	
	if( !WindowEq ) return false;
	
	// Add preset names	
	SendMessage( WindowEq, CB_ADDSTRING, 0, ( LPARAM )TEXT( " Disabled " ) );
	Playback::Eq::ReadPresets( AddPreset );
	
	// Initial value
	SendMessage( WindowEq, CB_SETCURSEL, Playback::Eq::GetCurIndex() + 1, 0 );
	
	// TODO Disabled
	// EnableWindow( WindowEq, FALSE );
	

	RECT rc;
	GetWindowRect( WindowEq, &rc );
   
	REBARBANDINFO rbBand;
	rbBand.cbSize		= sizeof( REBARBANDINFO );
	rbBand.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbBand.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE |
								( biEqBand.m_bBreak ? RBBS_BREAK : 0 );

	rbBand.lpText		= TEXT( " EQ " );
	rbBand.hwndChild	= WindowEq;
	rbBand.cxMinChild	= rc.right - rc.left;
	rbBand.cyMinChild	= 21;                      // IMP
	rbBand.cx			= biEqBand.m_iWidth;
	rbBand.wID			= BAND_EQ;
	rbBand.cyChild		= 21;
	rbBand.cyMaxChild	= 21;
	rbBand.cyIntegral	= 1;
	rbBand.cxIdeal		= 200;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbBand );
	
	Font::Apply( WindowEq );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildSeekBand()
{
	if( !WindowRebar ) return false;

	WindowSeek = CreateWindowEx(
		0,
		TRACKBAR_CLASS,
		TEXT( "" ), 
        WS_CHILD |
			WS_DISABLED |
			TBS_HORZ |
			TBS_NOTICKS |
			TBS_FIXEDLENGTH |
			TBS_BOTH,
		0,
		0,
		300,
		21,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);

	// Range
	SendMessage(
        WindowSeek,
		TBM_SETRANGE,
        TRUE,
        ( LPARAM )MAKELONG( 0, 999 )
	);

	// Thumb size
	SendMessage(
        WindowSeek,
		TBM_SETTHUMBLENGTH,
        14,
        0
	);

	// Exchange window procedure
	WndprocSeekBackup = ( WNDPROC )GetWindowLong( WindowSeek, GWL_WNDPROC );
	if( WndprocSeekBackup != NULL )
	{
		SetWindowLong( WindowSeek, GWL_WNDPROC, ( LONG )WndprocSeek );
	}
	
	
	REBARBANDINFO rbbi_seek;
	rbbi_seek.cbSize		= sizeof( REBARBANDINFO );
	rbbi_seek.fMask		    = 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi_seek.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE |
								( biSeekBand.m_bBreak ? RBBS_BREAK : 0 );

	rbbi_seek.lpText		= " Pos";
	rbbi_seek.hwndChild		= WindowSeek;
	rbbi_seek.cxMinChild	= 100;
	rbbi_seek.cyMinChild	= 21;        // IMP
	rbbi_seek.cx			= biSeekBand.m_iWidth;
	rbbi_seek.wID			= BAND_SEEK;
	rbbi_seek.cyChild		= 21;
	rbbi_seek.cyMaxChild	= 21;
	rbbi_seek.cyIntegral	= 1;
	rbbi_seek.cxIdeal		= 300;

   
	// Add band
	SendMessage(
		WindowRebar,
		RB_INSERTBAND,
		( WPARAM )-1,
		( LPARAM )&rbbi_seek
	);

	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildVolBand()
{
	if( !WindowRebar ) return false;

	WindowVol = CreateWindowEx(
		0,
		TRACKBAR_CLASS,
		TEXT( "" ), 
        WS_CHILD |
			TBS_HORZ |
			TBS_NOTICKS |
			TBS_FIXEDLENGTH |
			TBS_BOTH,
		0,
		0,
		200,
		21,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);

	if( !WindowVol ) return false;

	// Range
	SendMessage( WindowVol, TBM_SETRANGE, TRUE, ( LPARAM )MAKELONG( 0, 255 ) );
	
	// Initial value
	SendMessage( WindowVol, TBM_SETPOS, TRUE, Playback::Volume::Get() );

	// Thumbs size
	SendMessage( WindowVol, TBM_SETTHUMBLENGTH, 14, 0 );

	// Exchange window procedure
	WndprocVolBackup = ( WNDPROC )GetWindowLong( WindowVol, GWL_WNDPROC );
	if( WndprocVolBackup != NULL )
	{
		SetWindowLong( WindowVol, GWL_WNDPROC, ( LONG )WndprocVol );
	}

	
	REBARBANDINFO rbbi_vol;
	rbbi_vol.cbSize		= sizeof( REBARBANDINFO );
	rbbi_vol.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi_vol.fStyle     = RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE |
								( biVolBand.m_bBreak ? RBBS_BREAK : 0 );

	rbbi_vol.lpText     = TEXT( " Vol" );
	rbbi_vol.hwndChild  = WindowVol;
	rbbi_vol.cxMinChild	= 100;
	rbbi_vol.cyMinChild	= 21;               // IMP
	rbbi_vol.cx         = biVolBand.m_iWidth;
	rbbi_vol.wID        = BAND_VOL;
	rbbi_vol.cyChild    = 21;
	rbbi_vol.cyMaxChild	= 21;
	rbbi_vol.cyIntegral	= 1;
	rbbi_vol.cxIdeal    = 100;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbbi_vol );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildPanBand()
{
	if( !WindowRebar ) return false;

	WindowPan = CreateWindowEx(
		0,
		TRACKBAR_CLASS,
		TEXT( "" ), 
        WS_CHILD |
			TBS_HORZ |
			TBS_NOTICKS |
			TBS_FIXEDLENGTH |
			TBS_BOTH,
		0,
		0,
		200,
		21,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);

	if( !WindowPan ) return false;

	// Range
	SendMessage( WindowPan, TBM_SETRANGE, TRUE, ( LPARAM )MAKELONG( -127, 127 ) );

	// Initial value
	const int iCurPan = Playback::Pan::Get();
	SendMessage( WindowPan, TBM_SETPOS, TRUE, bInvertPanSlider ? -iCurPan : iCurPan );

	// Thumb size
	SendMessage( WindowPan, TBM_SETTHUMBLENGTH, 14, 0 );


	// Exchange window procedure
	WndprocPanBackup = ( WNDPROC )GetWindowLong( WindowPan, GWL_WNDPROC );
	if( WndprocPanBackup != NULL )
	{
		SetWindowLong( WindowPan, GWL_WNDPROC, ( LONG )WndprocPan );
	}

	
	REBARBANDINFO rbbi_pan;
	rbbi_pan.cbSize		= sizeof( REBARBANDINFO );
	rbbi_pan.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi_pan.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE |
								( biPanBand.m_bBreak ? RBBS_BREAK : 0 );

	rbbi_pan.lpText		= TEXT( " Pan" );
	rbbi_pan.hwndChild  = WindowPan;
	rbbi_pan.cxMinChild	= 100;
	rbbi_pan.cyMinChild	= 21;            // IMP
	rbbi_pan.cx         = biPanBand.m_iWidth;
	rbbi_pan.wID        = BAND_PAN;
	rbbi_pan.cyChild    = 21;
	rbbi_pan.cyMaxChild	= 21;
	rbbi_pan.cyIntegral	= 1;
	rbbi_pan.cxIdeal    = 100;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbbi_pan );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildButtonsBand()
{
	if( !WindowRebar ) return false;

	WindowButtons = CreateWindowEx(
		0,
		TOOLBARCLASSNAME,
		TEXT( "" ), 
        WS_CHILD |
			TBSTYLE_FLAT |
			CCS_NORESIZE |            // Means we care about the size ourselves
			CCS_NOPARENTALIGN |       // Make it work with the rebar control
			CCS_NODIVIDER,            // No divider on top
		0,
		0,
		100,
		21,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);

	if( !WindowButtons ) return false;

	SendMessage( WindowButtons, TB_BUTTONSTRUCTSIZE, ( WPARAM )sizeof( TBBUTTON ), 0 );

	TBBUTTON tb_button[ 7 ];

	// Make image list, TODO delete later
	HIMAGELIST hImages = ImageList_LoadBitmap(
		GetModuleHandle( NULL ),         // HINSTANCE hi
		MAKEINTRESOURCE( IDB_BITMAP1 ),  // LPCTSTR lpbmp
		14,                              // int cx
		6,                               // int cGrow
		RGB( 255, 000, 255 )             // COLORREF crMask
	);
	
	SendMessage( WindowButtons, TB_SETIMAGELIST, 0, ( LPARAM )hImages );
	// SendMessage( WindowButtons, TB_SETHOTIMAGELIST, 0, ( LPARAM )hImages );
	// SendMessage( WindowButtons, TB_SETDISABLEDIMAGELIST, 0, ( LPARAM )hImages );

	// Build buttons
	tb_button[ 0 ].iBitmap   = IMAGE_PREV;
	tb_button[ 0 ].idCommand = WINAMP_BUTTON1;
	tb_button[ 0 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 0 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 0 ].dwData    = 0;
	tb_button[ 0 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Previous" ));

	tb_button[ 1 ].iBitmap   = IMAGE_PLAY;
	tb_button[ 1 ].idCommand = WINAMP_BUTTON2;
	tb_button[ 1 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 1 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 1 ].dwData    = 0;
	tb_button[ 1 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Play" ));

	tb_button[ 2 ].iBitmap   = IMAGE_PAUSE;
	tb_button[ 2 ].idCommand = WINAMP_BUTTON3;
	tb_button[ 2 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 2 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 2 ].dwData    = 0;
	tb_button[ 2 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Pause" ));

	tb_button[ 3 ].iBitmap   = IMAGE_STOP;
	tb_button[ 3 ].idCommand = WINAMP_BUTTON4;
	tb_button[ 3 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 3 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 3 ].dwData    = 0;
	tb_button[ 3 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Stop" ));

	tb_button[ 4 ].iBitmap   = IMAGE_NEXT;
	tb_button[ 4 ].idCommand = WINAMP_BUTTON5;
	tb_button[ 4 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 4 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 4 ].dwData    = 0;
	tb_button[ 4 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Next" ));

	tb_button[ 5 ].iBitmap   = 0;
	tb_button[ 5 ].idCommand = -1;
	tb_button[ 5 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 5 ].fsStyle   = TBSTYLE_SEP;
	tb_button[ 5 ].dwData    = 0;
	tb_button[ 5 ].iString   = 0;

	tb_button[ 6 ].iBitmap   = IMAGE_OPEN;
	tb_button[ 6 ].idCommand = WINAMP_FILE_PLAY;
	tb_button[ 6 ].fsState   = TBSTATE_ENABLED;
	tb_button[ 6 ].fsStyle   = TBSTYLE_BUTTON;
	tb_button[ 6 ].dwData    = 0;
	tb_button[ 6 ].iString   = SendMessage( WindowButtons, TB_ADDSTRING, 0, ( LPARAM )TEXT( "Open" ));

	// Add buttons
	SendMessage( WindowButtons, TB_SETBUTTONSIZE, 0, MAKELONG( 14, 14 ) );
	SendMessage( WindowButtons, TB_ADDBUTTONS, 7, ( LPARAM )( LPTBBUTTON )&tb_button );

	// Disable labels
	SendMessage( WindowButtons, TB_SETMAXTEXTROWS, 0, 0 );

	// Resize
	RECT r;
	GetWindowRect( WindowButtons, &r );
	SetWindowPos( WindowButtons, NULL, 0, 0, 134, r.bottom - r.top, SWP_NOMOVE );
	GetWindowRect( WindowButtons, &r );

	REBARBANDINFO rbbi_buttons;
	rbbi_buttons.cbSize		= sizeof( REBARBANDINFO );
	rbbi_buttons.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		// RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi_buttons.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE | 
								( biButtonsBand.m_bBreak ? RBBS_BREAK : 0 );

	// rbbi_buttons.lpText		= TEXT( " Playback" );
	rbbi_buttons.hwndChild  = WindowButtons;
	rbbi_buttons.cxMinChild	= r.right - r.left;
	rbbi_buttons.cyMinChild	= 21;            // IMP
	rbbi_buttons.cx         = r.right - r.left;
	rbbi_buttons.wID        = BAND_BUTTONS;
	rbbi_buttons.cyChild    = 21;
	rbbi_buttons.cyMaxChild	= 21;
	rbbi_buttons.cyIntegral	= 1;
	rbbi_buttons.cxIdeal    = r.right - r.left;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbbi_buttons );
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool Rebar_BuildVisBand()
{
	if( !WindowRebar ) return false;

	WindowVis = CreateWindowEx(
		WS_EX_STATICEDGE,
		TEXT( "STATIC" ),
		TEXT( "" ), 
        WS_CHILD | SS_CENTER,
		0,
		0,
		100,
		21,
		WindowRebar,
		NULL,
		g_hInstance,
		NULL
	);
	
	if( !WindowVis ) return false;
	Font::Apply( WindowVis );

	// Resize
	RECT r;
	GetWindowRect( WindowButtons, &r );
//	SetWindowPos( WindowButtons, NULL, 0, 0, 146, r.bottom - r.top, SWP_NOMOVE );
//	GetWindowRect( WindowButtons, &r );

	REBARBANDINFO rbbi_buttons;
	rbbi_buttons.cbSize		= sizeof( REBARBANDINFO );
	rbbi_buttons.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
		RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
		RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi_buttons.fStyle		= RBBS_GRIPPERALWAYS |
								RBBS_CHILDEDGE | 
								( biVisBand.m_bBreak ? RBBS_BREAK : 0 );

	rbbi_buttons.lpText		= TEXT( " Vis " );
	rbbi_buttons.hwndChild  = WindowVis;
	rbbi_buttons.cxMinChild	= r.right - r.left;
	rbbi_buttons.cyMinChild	= 21;            // IMP
	rbbi_buttons.cx         = r.right - r.left;
	rbbi_buttons.wID        = BAND_VIS;
	rbbi_buttons.cyChild    = 21;
	rbbi_buttons.cyMaxChild	= 21;
	rbbi_buttons.cyIntegral	= 1;
	rbbi_buttons.cxIdeal    = r.right - r.left;
   
	// Add band
	SendMessage( WindowRebar, RB_INSERTBAND, ( WPARAM )-1, ( LPARAM )&rbbi_buttons );

	// Exchange window procedure
	WndprocVisBackup = ( WNDPROC )GetWindowLong( WindowVis, GWL_WNDPROC );
	if( WndprocVisBackup != NULL )
	{
		SetWindowLong( WindowVis, GWL_WNDPROC, ( LONG )WndprocVis );
	}
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ContextMenuRebar( POINT * p )
{
	REBARBANDINFO rbbi;
	rbbi.cbSize  = sizeof( REBARBANDINFO );
	rbbi.fMask   = RBBIM_STYLE;
	
	bool bBandVisible[ BAND_LAST - BAND_FIRST + 1 ] = { 0 }; // ID to visibility
	int  iBandIndex  [ BAND_LAST - BAND_FIRST + 1 ] = { 0 }; // ID to Index

	for( int i = BAND_FIRST; i <= BAND_LAST; i++ )
	{
		const int iArrayIndex = i - BAND_FIRST;
		
		// Get index
		iBandIndex[ iArrayIndex ] = ( int )SendMessage( WindowRebar, RB_IDTOINDEX, i, 0 );
		if( iBandIndex[ iArrayIndex ] == -1 ) break;
		
		// Get info
		if( !SendMessage( WindowRebar, RB_GETBANDINFO, iBandIndex[ iArrayIndex ], ( LPARAM )&rbbi ) )
		{
			break;
		}
		
		bBandVisible[ iArrayIndex ] = ( ( rbbi.fStyle & RBBS_HIDDEN ) != RBBS_HIDDEN );
		
		// Update menu item
		CheckMenuItem(
			rebar_menu,
			i,
			bBandVisible[ iArrayIndex ] ? MF_CHECKED : MF_UNCHECKED
		);
	}
	
	BOOL iIndex = TrackPopupMenu(
		rebar_menu,
		TPM_LEFTALIGN |
			TPM_TOPALIGN |
			TPM_NONOTIFY |
			TPM_RETURNCMD |
			TPM_RIGHTBUTTON,
		p->x,
		p->y,
		0,
		WindowRebar,
		NULL
	);

	// Toggle visibility
	if( ( iIndex >= BAND_FIRST ) && ( iIndex <= BAND_LAST ) )
	{
		const int iArrayIndex = iIndex - BAND_FIRST;
		SendMessage( WindowRebar, RB_SHOWBAND, iBandIndex[ iArrayIndex ], bBandVisible[ iArrayIndex ] ? FALSE : TRUE );
		
		// Turn off vis child
		if( iIndex == BAND_VIS )
		{
			HWND hChild = GetWindow( WindowVis, GW_CHILD );
			if( IsWindow( hChild ) )
			{
				SendMessage( hChild, WM_DESTROY, 0, 0 );
			}
		}
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocRebar( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	//Console::Append( TEXT("Rebar.cpp: WndprocRebar called") );

	switch( message )
	{
	case WM_CONTEXTMENU: // This affects the placement of the progress bar
		{
			//POINT p;
			//GetCursorPos( &p );
			//ContextMenuRebar( &p );
			return 0;
		}

	case WM_DESTROY:
		cbiOrderBand.TriggerCallback();
		cbiOrderBand.RemoveCallback();
		cbiEqBand.TriggerCallback();
		cbiEqBand.RemoveCallback();
		cbiSeekBand.TriggerCallback();
		cbiSeekBand.RemoveCallback();
		cbiVolBand.TriggerCallback();
		cbiVolBand.RemoveCallback();
		cbiPanBand.TriggerCallback();
		cbiPanBand.RemoveCallback();
		cbiButtonsBand.TriggerCallback();
		cbiButtonsBand.RemoveCallback();
		cbiVisBand.TriggerCallback();
		cbiVisBand.RemoveCallback();
		break;
	
	}
	return CallWindowProc( WndprocRebarBackup, hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocSeek( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	static bool bDown = false;
	static int iLastVal = 0;

	//Console::Append( TEXT("Rebar.cpp: WndprocSeek called") );

	switch( message )
	{
	case WM_LBUTTONDOWN:
	{
		SetFocus( hwnd );

		// Capture mouse
		SetCapture( hwnd );
		bDown = true;

		// NO BREAK!
	}
	case WM_MOUSEMOVE:
	{
		if( !bDown ) break;
		if( !Playback::IsPlaying() ) return 0; // Deny slider move

		RECT r;
		GetWindowRect( hwnd, &r );

		const int iWidth = r.right - r.left;
		if( !iWidth ) return 0;
		int iVal = 1000 * MAX( 0, MIN( ( short )LOWORD( lp ), iWidth ) ) / iWidth;

		// Snap
		if( ( short )LOWORD( lp ) < 7 )
		{
			iVal = 0;
		}
		else if( abs( iWidth - ( short )LOWORD( lp ) ) < 7 )
		{
			iVal = 999;
		}

		// Update slider
		PostMessage( hwnd, TBM_SETPOS, ( WPARAM )( TRUE ), iVal );

		// Seek
		iLastVal = iVal;
		Playback::SeekPercent( iLastVal / 10.0f );
		
		// TODO: Seek on slide ON/OFF
		

		return 0;
	}
	case WM_LBUTTONUP:
	case WM_NCLBUTTONUP:
		{
			// Release capture
			bDown = false;
			ReleaseCapture();

			if( !Playback::IsPlaying() ) return 0;

			int ms = Playback::PercentToMs( iLastVal / 10.0f );
			if( ms < 0 ) break;
			
			int h	= ms / 3600000;
			int m	= ( ms / 60000 ) % 60;
			int s	= ( ms / 1000 ) % 60;
			    ms  = ms % 1000;
			
			
			TCHAR szBuffer[ 5000 ];

			/*
			bool bShowMs = false;
			
			if( h )
			{
				if( bShowMs )
				{
					*/

					// 00:00:00.000
					_stprintf( szBuffer, TEXT( "Jumped to %02i:%02i:%02i.%03i" ), h, m, s, ms );
					Console::Append( szBuffer );
					Console::Append( " " );
					
					/*						
					
				}
				else
				{
					// 00:00:00
				}
			}
			else if( m )
			{
				if( bShowMs )
				{
					// 00:00.000
				}
				else
				{
					// 00:00
				}
			}
			else
			{
				if( bShowMs )
				{
					// 00.000
				}
				else
				{
					// 00s
				}
			}
			*/
	
			return 0;
		}

	case WM_KEYDOWN:
	case WM_KEYUP:
		SendMessage( WindowPlaylist, message, wp, lp );
		return 0;

	}
	return CallWindowProc( WndprocSeekBackup, hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocVol( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	static bool bDown = false;
	static int iLastVal = 0;

	switch( message )
	{
	case WM_LBUTTONDOWN:
		SetFocus( hwnd );

		// Capture mouse
		SetCapture( hwnd );
		bDown = true;

		// NO BREAK!

	case WM_MOUSEMOVE:
		{
			if( !bDown ) break;

			RECT r;
			GetWindowRect( hwnd, &r );

			const int iWidth = r.right - r.left;
			if( !iWidth ) return 0;
			const int x = ( int )( short )LOWORD( lp );
			int iVal = 255 * x / ( int )iWidth;

			if( iVal < 0 )
				iVal = 0;
			else if( ( iVal > 255 ) || ( abs( iWidth - x ) < 7 ) ) // Snap
				iVal = 255;

			// Update slider
			PostMessage( hwnd, TBM_SETPOS , ( WPARAM )( TRUE ), iVal );

			// Apply volume
			iLastVal = iVal;
			Playback::Volume::Set( iLastVal );

			return 0;
		}
	case WM_LBUTTONUP:
	case WM_NCLBUTTONUP:
		{
			// Release capture
			bDown = false;
			ReleaseCapture();

			TCHAR szBuffer[ 5000 ];
			_stprintf( szBuffer, TEXT( "Volume changed to %i%%" ), iLastVal * 100 / 255 );
			Console::Append( szBuffer );
			Console::Append( " " );

			return 0;
		}
	}
	return CallWindowProc( WndprocVolBackup, hwnd, message, wp, lp );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocPan( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	static bool bDown = false;
	static int iLastVal = 0;

	switch( message )
	{
	case WM_LBUTTONDOWN:
		SetFocus( hwnd );

		// Capture mouse
		SetCapture( hwnd );
		bDown = true;

		// NO BREAK!

	case WM_MOUSEMOVE:
		{
			if( !bDown ) break;
	
			RECT r;
			GetWindowRect( hwnd, &r );
	
			const int iWidth = r.right - r.left;
			if( !iWidth ) return 0;
			const int x = ( int )( short )LOWORD( lp );
			int iVal = 254 * x / ( int )iWidth - 127;
	
			if( iVal < -127 )
				iVal = -127;
			else if( iVal > 127 )
				iVal = 127;
			else if( abs( ( int )iWidth / 2 - x ) < 7 ) // Snap
				iVal = 0;
			
			// Update slider
			PostMessage( hwnd, TBM_SETPOS , ( WPARAM )( TRUE ), iVal );

			// Invert if needed (slider needs original value!)
			if( bInvertPanSlider )
			{
				iVal = -iVal;
			}
	
			// Apply panning
			iLastVal = iVal;
			Playback::Pan::Set( iVal );
			
			return 0;
		}
	case WM_LBUTTONUP:
	case WM_NCLBUTTONUP:
		{
			// Release capture
			bDown = false;
			ReleaseCapture();

			TCHAR szBuffer[ 5000 ];
			if( iLastVal < 0 )
				_stprintf( szBuffer, TEXT( "Panning changed to %i%% left" ), -iLastVal * 100 / 127 );
			else if( iLastVal == 0 )
				_stprintf( szBuffer, TEXT( "Panning changed to center" ) );
			else // if( iLastVal > 0 )
				_stprintf( szBuffer, TEXT( "Panning changed to %i%% right" ), iLastVal * 100 / 127 );
			Console::Append( szBuffer );
			Console::Append( " " );

			return 0;
		}
	}
	return CallWindowProc( WndprocPanBackup, hwnd, message, wp, lp );
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndprocVis( HWND hwnd, UINT message, WPARAM wp, LPARAM lp )
{
	switch( message )
	{
	case WM_SHOWWINDOW:
		{
			if( wp == FALSE )
			{
				const HWND hChild = GetWindow( hwnd, GW_CHILD );
				if( !IsWindow( hChild ) ) break;

				// Strange workaround
				ShowWindow( hChild, FALSE );
				SetParent( hChild, NULL );
				PostMessage( hChild, WM_SYSCOMMAND, SC_CLOSE, 0 );
			}
		}
		break;

	case WM_SIZE:
		{
			// Resize vis child
			HWND hChild = GetWindow( hwnd, GW_CHILD );
			if( !IsWindow( hChild ) ) break;
			MoveWindow( hChild, 0, 0, LOWORD( lp ), HIWORD( lp ), TRUE );
		}
		break;
	}
	return CallWindowProc( WndprocVisBackup, hwnd, message, wp, lp );
}


/////////////////////////


   
   /*
   // Create the toolbar control to be added.
   // hwndTB = CreateToolbar(hwndOwner, dwStyle);
   
   // Get the height of the toolbar.
   dwBtnSize = SendMessage(hwndTB, TB_GETBUTTONSIZE, 0,0);

   // Set values unique to the band with the toolbar.
   rbBand.lpText     = "Tool Bar";
   rbBand.hwndChild  = hwndTB;
   rbBand.cxMinChild = 0;
   rbBand.cyMinChild = HIWORD(dwBtnSize);
   rbBand.cx         = 250;

   // Add the band that has the toolbar.
   SendMessage(rebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);
	*/


///////////////////////////////////////

/*
void band_dummy()
{
	REBARBANDINFO rbbi3;
	rbbi3.cbSize		= sizeof( REBARBANDINFO );
	rbbi3.fMask		= 0 |
	//	RBBIM_BACKGROUND |	// The hbmBack member is valid or must be filled.
	//	RBBIM_CHILD |		// The hwndChild member is valid or must be filled.
	//	RBBIM_CHILDSIZE |	// The cxMinChild, cyMinChild, cyChild, cyMaxChild, and cyIntegral members are valid or must be filled.
	//	RBBIM_COLORS |		// The clrFore and clrBack members are valid or must be filled.
	//	RBBIM_HEADERSIZE |	// Version 4.71. The cxHeader member is valid or must be filled.
		RBBIM_IDEALSIZE |	// Version 4.71. The cxIdeal member is valid or must be filled.
		RBBIM_ID |			// The wID member is valid or must be filled.
	//	RBBIM_IMAGE |		// The iImage member is valid or must be filled.
	//	RBBIM_LPARAM |		// Version 4.71. The lParam member is valid or must be filled.
		RBBIM_SIZE |		// The cx member is valid or must be filled.
		RBBIM_STYLE |		// The fStyle member is valid or must be filled.
		RBBIM_TEXT |		// The lpText member is valid or must be filled.
		0;									

	rbbi3.fStyle		= RBBS_GRIPPERALWAYS; //  | RBBS_BREAK;
	rbbi3.lpText		= " "; //NULL; // "Dummy";
	rbbi3.hwndChild		= NULL;
	rbbi3.cxMinChild	= 10; //rc.right - rc.left;
	rbbi3.cyMinChild	= 21; // IMP
		rbbi3.cx			= 300;
	rbbi3.wID			= BAND_ID_DUMMY;
	rbbi3.cyChild		= 21; //rc.bottom - rc.top;
	rbbi3.cyMaxChild	= 21; // rc.bottom - rc.top;
	rbbi3.cyIntegral	= 1;
	rbbi3.cxIdeal		= 700; //rc.right - rc.left;
   
	// Add the band that has the combo box.
	SendMessage(
		rebar,
		RB_INSERTBAND,
		( WPARAM )-1,
		( LPARAM )&rbbi3
	);
}
*/
