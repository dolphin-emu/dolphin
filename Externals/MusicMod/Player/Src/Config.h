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


#ifndef PA_CONFIG_H
#define PA_CONFIG_H



#include "Global.h"
#include "OutputPlugin.h"



namespace Conf
{
	//void Init( HINSTANCE hInstance );
	void Init( );
	void Write();
};



enum ConfMode
{
	CONF_MODE_INTERNAL,  // Will not be shown to the user
	CONF_MODE_PUBLIC
};

class ConfVar;
typedef void ( * ConfCallback )( ConfVar * var );

struct BandInfo
{
	int   m_iIndex;
	int   m_iWidth;
	bool  m_bBreak;
	bool  m_bVisible;
};



////////////////////////////////////////////////////////////////////////////////
/// Config container
////////////////////////////////////////////////////////////////////////////////
class ConfVar
{
public:
	ConfVar( TCHAR * szKey, ConfMode mode );
	ConfVar( const TCHAR * szKey, ConfMode mode );
	~ConfVar();

protected:
	TCHAR *   m_szKey;     ///< Unique identifier
	ConfMode  m_Mode;      ///< Mode/visibility
	bool      m_bRead;     ///< Initilization flag
	
	virtual void Read() = 0;
	virtual void Write() = 0;
	
	// virtual void Backup() = 0;   ///< Creates a backup and deletes old backup if it exists
	// virtual void Restore() = 0;  ///< Restores settings from backup and destroys the backup

private:
	bool      m_bCopyKey;  ///< Keyname is copy (has to be freed on destruction)

	//friend void Conf::Init( HINSTANCE hInstance );
	friend void Conf::Init( );

	friend void Conf::Write();
};



////////////////////////////////////////////////////////////////////////////////
/// Boolean config container
////////////////////////////////////////////////////////////////////////////////
class ConfBool : public ConfVar
{
public:
	ConfBool( bool * pbData,       TCHAR * szKey, ConfMode mode, bool bDefault );
	ConfBool( bool * pbData, const TCHAR * szKey, ConfMode mode, bool bDefault );

private:
	bool *  m_pbData;    ///< Target
	bool    m_bDefault;  ///< Default value

	void Read();
	void Write();
	
	
	friend OutputPlugin::OutputPlugin( TCHAR * szDllpath, bool bKeepLoaded );
};



////////////////////////////////////////////////////////////////////////////////
/// Integer config container
////////////////////////////////////////////////////////////////////////////////
class ConfInt : public ConfVar
{
public:
	ConfInt( int * piData,       TCHAR * szKey, ConfMode mode, int iDefault );
	ConfInt( int * piData, const TCHAR * szKey, ConfMode mode, int iDefault );

protected:
	int *  m_piData;
	int    m_iDefault;

	void Read();
	void Write();
};



////////////////////////////////////////////////////////////////////////////////
/// Integer config container with restricted range
////////////////////////////////////////////////////////////////////////////////
class ConfIntMinMax : public ConfInt
{
public:
	ConfIntMinMax( int * piData,       TCHAR * szKey, ConfMode mode, int iDefault, int iMin, int iMax );
	ConfIntMinMax( int * piData, const TCHAR * szKey, ConfMode mode, int iDefault, int iMin, int iMax );
	
//	bool IsValid() { return ( ( *m_piData >= m_iMin ) && ( *m_piData <= m_iMax ) ); }
	inline bool IsMin() {  return ( *m_piData == m_iMin ); }
	inline bool IsMax() {  return ( *m_piData == m_iMax ); }
	
	inline void MakeValidDefault() { if( ( *m_piData < m_iMin ) || ( *m_piData > m_iMax ) ) *m_piData = m_iDefault; }
	inline void MakeValidPull() { if( *m_piData < m_iMin ) *m_piData = m_iMin; else if( *m_piData > m_iMax ) *m_piData = m_iMax; }

private:
	int m_iMin;
	int m_iMax;

	void Read() { ConfInt::Read(); MakeValidPull(); }
};



////////////////////////////////////////////////////////////////////////////////
/// Window placement config container
///
/// The callback funtion is called on write()
/// so the data written is up to date.
////////////////////////////////////////////////////////////////////////////////
class ConfWinPlaceCallback : public ConfVar
{
public:
	ConfWinPlaceCallback( WINDOWPLACEMENT * pwpData,       TCHAR * szKey, RECT * prDefault, ConfCallback fpCallback );
	ConfWinPlaceCallback( WINDOWPLACEMENT * pwpData, const TCHAR * szKey, RECT * prDefault, ConfCallback fpCallback );

	inline void TriggerCallback() { if( m_fpCallback ) m_fpCallback( this ); }
	inline void RemoveCallback() { m_fpCallback = NULL; }

private:
	WINDOWPLACEMENT *  m_pwpData;
	RECT *             m_prDefault;
	ConfCallback       m_fpCallback;

	void Read();
	void Write();
};



////////////////////////////////////////////////////////////////////////////////
/// Rebar band info config container
///
/// The callback funtion is called on write()
/// so the data written is up to date.
////////////////////////////////////////////////////////////////////////////////
class ConfBandInfoCallback : public ConfVar
{
public:
	ConfBandInfoCallback( BandInfo * pbiData,       TCHAR * szKey, BandInfo * pbiDefault, ConfCallback fpCallback );
	ConfBandInfoCallback( BandInfo * pbiData, const TCHAR * szKey, BandInfo * pbiDefault, ConfCallback fpCallback );

	inline void TriggerCallback() { if( m_fpCallback ) m_fpCallback( this ); }
	inline void RemoveCallback() { m_fpCallback = NULL; }

	bool Apply( HWND hRebar, int iBandId );

private:
	BandInfo *    m_pbiData;
	BandInfo *    m_pbiDefault;
	ConfCallback  m_fpCallback;

	void Read();
	void Write();
};



////////////////////////////////////////////////////////////////////////////////
/// String config container
////////////////////////////////////////////////////////////////////////////////
class ConfString : public ConfVar
{
public:
	ConfString( TCHAR * szData,       TCHAR * szKey, ConfMode mode, TCHAR * szDefault, int iMaxLen );
	ConfString( TCHAR * szData, const TCHAR * szKey, ConfMode mode, TCHAR * szDefault, int iMaxLen );

protected:
	TCHAR *  m_szData;
	int      m_iMaxLen;
	TCHAR *  m_szDefault;

	void Read();
	void Write();
};



////////////////////////////////////////////////////////////////////////////////
/// Current directory config container
////////////////////////////////////////////////////////////////////////////////
class ConfCurDir : public ConfString
{
public:
	ConfCurDir( TCHAR * szData,       TCHAR * szKey );
	ConfCurDir( TCHAR * szData, const TCHAR * szKey );

private:
	void Read();
	void Write();
};



#endif // PA_CONFIG_H
