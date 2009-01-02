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


#include "Config.h"
#include "Console.h"
#include <map>

using namespace std;



map<TCHAR *, ConfVar *> * conf_map = NULL;


TCHAR * szIniPath = NULL;
const TCHAR * SECTION = TEXT( "Plainamp" );




////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfVar::ConfVar( TCHAR * szKey, ConfMode mode )
{
	// MessageBox( 0, TEXT( "no const @ ConfVar" ), TEXT( "" ), 0 );

	// Init
	const int iLen = ( int )_tcslen( szKey );
	m_szKey = new TCHAR[ iLen + 1 ];
	memcpy( m_szKey, szKey, iLen * sizeof( TCHAR ) );
	m_szKey[ iLen ] = TEXT( '\0' );

	m_bCopyKey  = true;
	m_Mode      = mode;
	
	m_bRead     = false;
	
	// Register
	if( !conf_map ) conf_map = new map<TCHAR *, ConfVar *>;
	conf_map->insert( pair<TCHAR *, ConfVar *>( m_szKey, this ) );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfVar::ConfVar( const TCHAR * szKey, ConfMode mode )
{
	// Init
	m_szKey     = ( TCHAR * )szKey;

	m_bCopyKey  = false;
	m_Mode      = mode;
	
	m_bRead     = false;
	
	// Register
	if( !conf_map ) conf_map = new map<TCHAR *, ConfVar *>;
	conf_map->insert( pair<TCHAR *, ConfVar *>( m_szKey, this ) );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfVar::~ConfVar()
{
	if( m_bCopyKey && m_szKey ) delete [] m_szKey;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfBool::ConfBool( bool * pbData, TCHAR * szKey, ConfMode mode, bool bDefault ) : ConfVar( szKey, mode )
{
	// MessageBox( 0, TEXT( "no const @ ConfBool" ), TEXT( "" ), 0 );

	m_pbData    = pbData;
	m_bDefault  = bDefault;

	// *pbData     = bDefault;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfBool::ConfBool( bool * pbData, const TCHAR * szKey, ConfMode mode, bool bDefault ) : ConfVar( szKey, mode )
{
	m_pbData    = pbData;
	m_bDefault  = bDefault;
	
	// *pbData     = bDefault;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfBool::Read()
{
	if( m_bRead || !szIniPath ) return;

	*m_pbData  = ( GetPrivateProfileInt( SECTION, m_szKey, ( int )m_bDefault, szIniPath ) != 0 );
	m_bRead    = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfBool::Write()
{
	WritePrivateProfileString( SECTION, m_szKey, *m_pbData ? TEXT( "1" ) : TEXT( "0" ), szIniPath );
	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfInt::ConfInt( int * piData, TCHAR * szKey, ConfMode mode, int iDefault ) : ConfVar( szKey, mode )
{
	MessageBox( 0, TEXT( "no const" ), TEXT( "" ), 0 );

	m_piData    = piData;
	m_iDefault  = iDefault;

	// *piData     = iDefault;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfInt::ConfInt( int * piData, const TCHAR * szKey, ConfMode mode, int iDefault ) : ConfVar( szKey, mode )
{
	m_piData    = piData;
	m_iDefault  = iDefault;

	// *piData     = iDefault;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfInt::Read()
{
	if( m_bRead || !szIniPath ) return;

	*m_piData  = GetPrivateProfileInt( SECTION, m_szKey, m_iDefault, szIniPath );
	m_bRead    = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfInt::Write()
{
	TCHAR szNumber[ 12 ] = TEXT( "" );
	_stprintf( szNumber, TEXT( "%i" ), *m_piData );
	WritePrivateProfileString( SECTION, m_szKey, szNumber, szIniPath );
	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfIntMinMax::ConfIntMinMax( int * piData, TCHAR * szKey, ConfMode mode, int iDefault, int iMin, int iMax ) : ConfInt( piData, szKey, mode, iDefault )
{
	MessageBox( 0, TEXT( "no const" ), TEXT( "" ), 0 );

	m_iMin = iMin;
	m_iMax = iMax;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfIntMinMax::ConfIntMinMax( int * piData, const TCHAR * szKey, ConfMode mode, int iDefault, int iMin, int iMax ) : ConfInt( piData, szKey, mode, iDefault )
{
	m_iMin = iMin;
	m_iMax = iMax;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfWinPlaceCallback::ConfWinPlaceCallback( WINDOWPLACEMENT * pwpData, TCHAR * szKey, RECT * prDefault, ConfCallback fpCallback ) : ConfVar( szKey, CONF_MODE_INTERNAL )
{
	MessageBox( 0, TEXT( "no const" ), TEXT( "" ), 0 );

	m_pwpData        = pwpData;
	m_prDefault      = prDefault;
	m_fpCallback     = fpCallback;
	
	pwpData->length  = sizeof( WINDOWPLACEMENT );
	pwpData->flags   = 0;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfWinPlaceCallback::ConfWinPlaceCallback( WINDOWPLACEMENT * pwpData, const TCHAR * szKey, RECT * prDefault, ConfCallback fpCallback ) : ConfVar( szKey, CONF_MODE_INTERNAL )
{
	m_pwpData        = pwpData;
	m_prDefault      = prDefault;
	m_fpCallback     = fpCallback;
	
	pwpData->length  = sizeof( WINDOWPLACEMENT );
	pwpData->flags   = 0;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfWinPlaceCallback::Read()
{
	if( m_bRead || !szIniPath ) return;

	POINT ptMinPosition = { -1, -1 };
	POINT ptMaxPosition = { -GetSystemMetrics( SM_CXBORDER ), -GetSystemMetrics( SM_CYBORDER ) };

	m_pwpData->length         = sizeof( WINDOWPLACEMENT );
	m_pwpData->flags          = 0;
	m_pwpData->ptMinPosition  = ptMinPosition;
	m_pwpData->ptMaxPosition  = ptMaxPosition;


	TCHAR szOut[ 111 ] = TEXT( "" );
	int iChars = GetPrivateProfileString( SECTION, m_szKey, TEXT( "" ), szOut, 110, szIniPath );
	bool bApplyDefault = true;
	if( iChars )
	{
		int iFields = _stscanf(
			szOut,
			TEXT( "(%u,(%i,%i,%i,%i))" ),
			&m_pwpData->showCmd,
			&m_pwpData->rcNormalPosition.left,
			&m_pwpData->rcNormalPosition.top,
			&m_pwpData->rcNormalPosition.right,
			&m_pwpData->rcNormalPosition.bottom
		);
		if( iFields == 5 )
		{
			bApplyDefault = false;
		}
	}
	
	if( bApplyDefault )
	{
		m_pwpData->showCmd           = SW_SHOWNORMAL;
		m_pwpData->rcNormalPosition  = *m_prDefault;
	}

	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfWinPlaceCallback::Write()
{
	// Refresh data
	if( m_fpCallback ) m_fpCallback( this );

	TCHAR szData[ 111 ] = TEXT( "" );
	_stprintf(
		szData,
		TEXT( "(%u,(%i,%i,%i,%i))" ),
		m_pwpData->showCmd,
		m_pwpData->rcNormalPosition.left,
		m_pwpData->rcNormalPosition.top,
		m_pwpData->rcNormalPosition.right,
		m_pwpData->rcNormalPosition.bottom
	);
	WritePrivateProfileString( SECTION, m_szKey, szData, szIniPath );
	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfBandInfoCallback::ConfBandInfoCallback( BandInfo * pbiData, TCHAR * szKey, BandInfo * pbiDefault, ConfCallback fpCallback ) : ConfVar( szKey, CONF_MODE_INTERNAL )
{
	MessageBox( 0, TEXT( "no const" ), TEXT( "" ), 0 );

	m_pbiData     = pbiData;
	m_pbiDefault  = pbiDefault;
	m_fpCallback  = fpCallback;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfBandInfoCallback::ConfBandInfoCallback( BandInfo * pbiData, const TCHAR * szKey, BandInfo * pbiDefault, ConfCallback fpCallback ) : ConfVar( szKey, CONF_MODE_INTERNAL )
{
	m_pbiData     = pbiData;
	m_pbiDefault  = pbiDefault;
	m_fpCallback  = fpCallback;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfBandInfoCallback::Read()
{
	if( m_bRead || !szIniPath ) return;

	int iBreak;
	int iVisible;
	
	TCHAR szOut[ 91 ] = TEXT( "" );
	int iChars = GetPrivateProfileString( SECTION, m_szKey, TEXT( "" ), szOut, 90, szIniPath );
	bool bApplyDefault = true;
	if( iChars )
	{
		int iFields = _stscanf(
			szOut,
			TEXT( "(%i,%i,%i,%i)" ),
			&m_pbiData->m_iIndex,
			&m_pbiData->m_iWidth,
			&iBreak,
			&iVisible
		);
		if( iFields == 4 )
		{
			m_pbiData->m_bBreak    = ( iBreak   != 0 );
			m_pbiData->m_bVisible  = ( iVisible != 0 );
			
			bApplyDefault = false;
		}
	}
	
	if( bApplyDefault )
	{
		*m_pbiData = *m_pbiDefault;
	}

	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfBandInfoCallback::Write()
{
	// Refresh data
	if( m_fpCallback ) m_fpCallback( this );

	TCHAR szData[ 91 ] = TEXT( "" );
	_stprintf(
		szData,
		TEXT( "(%i,%i,%i,%i)" ),
		m_pbiData->m_iIndex,
		m_pbiData->m_iWidth,
		( int )m_pbiData->m_bBreak,
		( int )m_pbiData->m_bVisible
	);
	WritePrivateProfileString( SECTION, m_szKey, szData, szIniPath );
	m_bRead = true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
bool ConfBandInfoCallback::Apply( HWND hRebar, int iBandId )
{
	const int iOldIndex = ( int )SendMessage( hRebar, RB_IDTOINDEX, iBandId, 0 );
	if( iOldIndex == -1 ) return false;

	int & iNewIndex = m_pbiData->m_iIndex;
	if( iOldIndex != iNewIndex )
	{
		// Move band
		if( !SendMessage( hRebar, RB_MOVEBAND, iOldIndex, iNewIndex ) )
		{
			return false;
		}
	}
	
	REBARBANDINFO rbbi;
	rbbi.cbSize  = sizeof( REBARBANDINFO );
	rbbi.fMask   = RBBIM_STYLE;

	// Get current info
	if( !SendMessage( hRebar, RB_GETBANDINFO, iNewIndex, ( LPARAM )&rbbi ) )
	{
		return false;
	}

	rbbi.fMask   = RBBIM_SIZE | RBBIM_STYLE;
	rbbi.cx      = m_pbiData->m_iWidth;
	if( ( rbbi.fStyle & RBBS_BREAK ) == RBBS_BREAK )
	{
		if( !m_pbiData->m_bBreak ) rbbi.fStyle -= RBBS_BREAK;
	}
	else
	{
		if( m_pbiData->m_bBreak ) rbbi.fStyle |= RBBS_BREAK;
	}

	// Update info
	if( !SendMessage( hRebar, RB_SETBANDINFO, iNewIndex, ( LPARAM )&rbbi ) )
	{
		return false;
	}
	
	// Show/hide band
	if( !SendMessage( hRebar, RB_SHOWBAND, iNewIndex, m_pbiData->m_bVisible ? TRUE : FALSE ) )
	{
		return false;
	}
	
	return true;
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfString::ConfString( TCHAR * szData, TCHAR * szKey, ConfMode mode, TCHAR * szDefault, int iMaxLen ) : ConfVar( szKey, mode )
{
	MessageBox( 0, TEXT( "no const" ), TEXT( "" ), 0 );

	m_szData     = szData;
	m_szDefault  = szDefault;
	m_iMaxLen    = iMaxLen;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfString::ConfString( TCHAR * szData, const TCHAR * szKey, ConfMode mode, TCHAR * szDefault, int iMaxLen ) : ConfVar( szKey, mode )
{
	m_szData     = szData;
	m_szDefault  = szDefault;
	m_iMaxLen    = iMaxLen;	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfString::Read()
{
	if( m_bRead || !szIniPath ) return;

	GetPrivateProfileString( SECTION, m_szKey, m_szDefault, m_szData, m_iMaxLen, szIniPath );
	m_bRead = true;
}


	
////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfString::Write()
{
	WritePrivateProfileString( SECTION, m_szKey, m_szData, szIniPath );
	m_bRead = true;
}	



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfCurDir::ConfCurDir( TCHAR * szData, TCHAR * szKey ) : ConfString( szData, szKey, CONF_MODE_INTERNAL, TEXT( "C:\\" ), MAX_PATH )
{
	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
ConfCurDir::ConfCurDir( TCHAR * szData, const TCHAR * szKey ) : ConfString( szData, szKey, CONF_MODE_INTERNAL, TEXT( "C:\\" ), MAX_PATH )
{
	
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfCurDir::Read()
{
	ConfString::Read();

	// MessageBox( 0, m_szData, TEXT( "CurDir" ), 0 );
	
	// Apply
	SetCurrentDirectory( m_szData );
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void ConfCurDir::Write()
{
	// Refresh
	GetCurrentDirectory( MAX_PATH, m_szData ); // Note: without trailing slash

	// MessageBox( 0, m_szData, TEXT( "CurDir" ), 0 );

	ConfString::Write();
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Conf::Init( HINSTANCE hInstance )
{
	if( szIniPath ) return;

	// Long filename

	szIniPath = new TCHAR[ _MAX_PATH ];

	TCHAR szFull[ _MAX_PATH ]    = TEXT( "" );
	TCHAR szDrive[ _MAX_DRIVE ]  = TEXT( "" );
	TCHAR szDir[ _MAX_DIR ]      = TEXT( "" );


	GetModuleFileName( hInstance, szFull, _MAX_PATH );

	_tsplitpath( szFull, szDrive, szDir, NULL, NULL );


	// Convert short to long path
	GetLongPathName( szDir, szDir, _MAX_DIR );

	// Convert short to long file
	WIN32_FIND_DATA fd;
	HANDLE h = FindFirstFile( szFull, &fd );

	// Search last dot
	TCHAR * szSearch = fd.cFileName + _tcslen( fd.cFileName ) - 1;
	while( ( *szSearch != TEXT( '.' ) ) && ( szSearch > fd.cFileName ) )
	{
		szSearch--;
	}

	// Replace extension
	_tcscpy( szSearch, TEXT( ".ini" ) );

	// Copy full filename
	_sntprintf( szIniPath, _MAX_PATH, TEXT( "%s%s%s" ), szDrive, szDir, fd.cFileName );



	// Read all
	map<TCHAR *, ConfVar *>::iterator iter = conf_map->begin();
	while( iter != conf_map->end() )
	{
		iter->second->Read();
		iter++;	
	}
}



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
void Conf::Write()
{
	if( !szIniPath ) return;

	map<TCHAR *, ConfVar *>::iterator iter = conf_map->begin();
	while( iter != conf_map->end() )
	{
		iter->second->Write();
		iter++;	
	}
}
