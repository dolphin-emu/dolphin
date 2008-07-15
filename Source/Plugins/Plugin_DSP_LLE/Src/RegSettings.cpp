// RegSettings.cpp
//
// Copyright (c) 2001 Magomed Abdurakhmanov
// maq@hotbox.ru, http://mickels.iwt.ru/en
//
//
//
// No warranties are given. Use at your own risk.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RegSettings.h"

//////////////////////////////////////////////////////////////////////
// CWindowSettings

#define S_WINDOW_PLACEMENT_VAL _T("WindowPlacement")

CWindowSettings::CWindowSettings()
{
	m_WindowPlacement.length = sizeof(m_WindowPlacement);
	m_WindowPlacement.flags = 0;
	m_WindowPlacement.ptMinPosition.x = 0;
	m_WindowPlacement.ptMinPosition.y = 0;
	m_WindowPlacement.ptMaxPosition.x = 0;
	m_WindowPlacement.ptMaxPosition.y = 0;

	CRect rc;
	SystemParametersInfo(SPI_GETWORKAREA, 0, rc, 0);
	rc.DeflateRect(100, 100);
	m_WindowPlacement.rcNormalPosition = rc;
	m_WindowPlacement.showCmd = SW_SHOWNORMAL;
}


bool CWindowSettings::Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/)
{
	CRegKey reg;
	DWORD err = reg.Open(hkRootKey, szRegKey, KEY_READ);

	if (err == ERROR_SUCCESS)
	{
		DWORD dwType = NULL;
		DWORD dwSize = sizeof(m_WindowPlacement);
		err = RegQueryValueEx(reg.m_hKey, CString(szPrefix) + S_WINDOW_PLACEMENT_VAL, NULL, &dwType,
				(LPBYTE)&m_WindowPlacement, &dwSize);
	}

	return(err == ERROR_SUCCESS);
}


bool CWindowSettings::Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/) const
{
	CRegKey reg;
	DWORD err = reg.Create(hkRootKey, szRegKey);

	if (err == ERROR_SUCCESS)
	{
		err = RegSetValueEx(reg.m_hKey, CString(szPrefix) + S_WINDOW_PLACEMENT_VAL, NULL, REG_BINARY,
				(LPBYTE)&m_WindowPlacement, sizeof(m_WindowPlacement));
	}

	return(err == ERROR_SUCCESS);
}


void CWindowSettings::GetFrom(CWindow& Wnd)
{
	ATLASSERT(Wnd.IsWindow());
	Wnd.GetWindowPlacement(&m_WindowPlacement);
}


void CWindowSettings::ApplyTo(CWindow& Wnd, int nCmdShow /* = SW_SHOWNORMAL*/) const
{
	ATLASSERT(Wnd.IsWindow());

	Wnd.SetWindowPlacement(&m_WindowPlacement);

	if (SW_SHOWNORMAL != nCmdShow)
	{
		Wnd.ShowWindow(nCmdShow);
	}
	else
	if (m_WindowPlacement.showCmd == SW_MINIMIZE || m_WindowPlacement.showCmd == SW_SHOWMINIMIZED)
	{
		Wnd.ShowWindow(SW_SHOWNORMAL);
	}
}


//////////////////////////////////////////////////////////////////////
// CReBarSettings

#define S_BAR_BANDCOUNT _T("BandCount")
#define S_BAR_ID_VAL _T("ID")
#define S_BAR_CX_VAL _T("CX")
#define S_BAR_BREAKLINE_VAL _T("BreakLine")

CReBarSettings::CReBarSettings()
{
	m_pBands = NULL;
	m_cbBandCount = 0;
}


CReBarSettings::~CReBarSettings()
{
	if (m_pBands != NULL)
	{
		delete[] m_pBands;
	}
}


bool CReBarSettings::Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/)
{
	if (m_pBands != NULL)
	{
		delete[] m_pBands;
	}

	m_pBands = NULL;
	m_cbBandCount = 0;

	CRegKey reg;
	DWORD err = reg.Open(hkRootKey, szRegKey, KEY_READ);

	if (err == ERROR_SUCCESS)
	{
		reg.QueryDWORDValue(CString(szPrefix) + S_BAR_BANDCOUNT, m_cbBandCount);

		if (m_cbBandCount > 0)
		{
			m_pBands = new BandInfo[m_cbBandCount];
		}

		for (DWORD i = 0; i < m_cbBandCount; i++)
		{
			CString s;
			s.Format(_T("%s%i_"), szPrefix, i);
			reg.QueryDWORDValue(s + S_BAR_ID_VAL, m_pBands[i].ID);
			reg.QueryDWORDValue(s + S_BAR_CX_VAL, m_pBands[i].cx);

			DWORD dw;
			reg.QueryDWORDValue(s + S_BAR_BREAKLINE_VAL, dw);
			m_pBands[i].BreakLine = dw != 0;
		}
	}

	return(err == ERROR_SUCCESS);
}


bool CReBarSettings::Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/) const
{
	CRegKey reg;
	DWORD err = reg.Create(hkRootKey, szRegKey);

	if (err == ERROR_SUCCESS)
	{
		reg.SetDWORDValue(CString(szPrefix) + S_BAR_BANDCOUNT, m_cbBandCount);

		for (DWORD i = 0; i < m_cbBandCount; i++)
		{
			CString s;
			s.Format(_T("%s%i_"), szPrefix, i);
			reg.SetDWORDValue(s + S_BAR_ID_VAL, m_pBands[i].ID);
			reg.SetDWORDValue(s + S_BAR_CX_VAL, m_pBands[i].cx);

			DWORD dw = m_pBands[i].BreakLine;
			reg.SetDWORDValue(s + S_BAR_BREAKLINE_VAL, dw);
		}
	}

	return(err == ERROR_SUCCESS);
}


void CReBarSettings::GetFrom(CReBarCtrl& ReBar)
{
	ATLASSERT(ReBar.IsWindow());

	if (m_pBands != NULL)
	{
		delete[] m_pBands;
	}

	m_pBands = NULL;
	m_cbBandCount = ReBar.GetBandCount();

	if (m_cbBandCount > 0)
	{
		m_pBands = new BandInfo[m_cbBandCount];
	}

	for (UINT i = 0; i < m_cbBandCount; i++)
	{
		REBARBANDINFO rbi;
		rbi.cbSize = sizeof(rbi);
		rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
		ReBar.GetBandInfo(i, &rbi);
		m_pBands[i].ID = rbi.wID;
		m_pBands[i].cx = rbi.cx;
		m_pBands[i].BreakLine = (rbi.fStyle & RBBS_BREAK) != 0;
	}
}


void CReBarSettings::ApplyTo(CReBarCtrl& ReBar) const
{
	ATLASSERT(ReBar.IsWindow());

	for (UINT i = 0; i < m_cbBandCount; i++)
	{
		ReBar.MoveBand(ReBar.IdToIndex(m_pBands[i].ID), i);
		REBARBANDINFO rbi;
		rbi.cbSize = sizeof(rbi);
		rbi.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
		ReBar.GetBandInfo(i, &rbi);

		rbi.cx = m_pBands[i].cx;

		if (m_pBands[i].BreakLine)
		{
			rbi.fStyle |= RBBS_BREAK;
		}
		else
		{
			rbi.fStyle &= (~RBBS_BREAK);
		}

		ReBar.SetBandInfo(i, &rbi);
	}
}


//////////////////////////////////////////////////////////////////////
// CSplitterSettings

#define S_SPLITTER_POS _T("SplitterPos")

bool CSplitterSettings::Load(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/)
{
	CRegKey reg;
	DWORD err = reg.Open(hkRootKey, szRegKey, KEY_READ);

	if (err == ERROR_SUCCESS)
	{
		reg.QueryDWORDValue(CString(szPrefix) + S_SPLITTER_POS, m_dwPos);
	}

	return(err == ERROR_SUCCESS);
}


bool CSplitterSettings::Save(LPCTSTR szRegKey, LPCTSTR szPrefix, HKEY hkRootKey /* = HKEY_CURRENT_USER*/) const
{
	CRegKey reg;
	DWORD err = reg.Create(hkRootKey, szRegKey);

	if (err == ERROR_SUCCESS)
	{
		reg.SetDWORDValue(CString(szPrefix) + S_SPLITTER_POS, m_dwPos);
	}

	return(err == ERROR_SUCCESS);
}


