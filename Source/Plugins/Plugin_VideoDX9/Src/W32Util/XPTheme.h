// Windows Template Library - WTL version 7.0
// Copyright (C) 1997-2002 Microsoft Corporation
// All rights reserved.
//
// This file is a part of the Windows Template Library.
// The code and information is provided "as-is" without
// warranty of any kind, either expressed or implied.


#pragma once

#include <tchar.h>
//#include <tmschema.h>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

// Note: To create an application that also runs on older versions of Windows,
// use delay load of uxtheme.dll and ensure that no calls to the Theme API are
// made if theming is not supported. It is enough to check if m_hTheme is NULL.
// Example:
//  if(m_hTheme != NULL)
//  {
//      DrawThemeBackground(dc, BP_PUSHBUTTON, PBS_NORMAL, &rect, NULL);
//      DrawThemeText(dc, BP_PUSHBUTTON, PBS_NORMAL, L"Button", -1, DT_SINGLELINE | DT_CENTER | DT_VCENTER, 0, &rect);
//  }
//  else
//  {
//      dc.DrawFrameControl(&rect, DFC_BUTTON, DFCS_BUTTONPUSH);
//      dc.DrawText(_T("Button"), -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
//  }
//
// Delay load is NOT AUTOMATIC for VC++ 7, you have to link to delayimp.lib, 
// and add uxtheme.dll in the Linker.Input.Delay Loaded DLLs section of the 
// project properties.
#if (_MSC_VER < 1300) && !defined(_WTL_NO_THEME_DELAYLOAD)
#pragma comment(lib, "delayimp.lib")
#pragma comment(linker, "/delayload:uxtheme.dll")
#endif //(_MSC_VER < 1300) && !defined(_WTL_NO_THEME_DELAYLOAD)


// Classes in this file
//
// CTheme
// CThemeImpl<T, TBase>


namespace WTL
{

// CTheme - wrapper for theme handle

class CTheme
{
public:
// Data members
    HTHEME m_hTheme;
    static int m_nIsThemingSupported;

// Constructor
    CTheme() : m_hTheme(NULL)
    {
        IsThemingSupported();
    }

// Operators and helpers
    bool IsThemeNull() const
    {
        return (m_hTheme == NULL);
    }

    CTheme& operator =(HTHEME hTheme)
    {
        m_hTheme = hTheme;
        return *this;
    }

    operator HTHEME() const
    {
        return m_hTheme;
    }

    void Attach(HTHEME hTheme)
    {
        m_hTheme = hTheme;
    }

    HTHEME Detach()
    {
        HTHEME hTheme = m_hTheme;
        m_hTheme = NULL;
        return hTheme;
    }

// Theme support helper
    static bool IsThemingSupported()
    {
        if(m_nIsThemingSupported == -1)
        {
 //           ::EnterCriticalSection(&_Module.m_csStaticDataInit);
            if(m_nIsThemingSupported == -1)
            {
                HMODULE hThemeDLL = ::LoadLibrary(_T("uxtheme.dll"));
                m_nIsThemingSupported = (hThemeDLL != NULL) ? 1 : 0;
                if(hThemeDLL != NULL)
                    ::FreeLibrary(hThemeDLL);
            }
 //           ::LeaveCriticalSection(&_Module.m_csStaticDataInit);
        }

        //ATLASSERT(m_nIsThemingSupported != -1);
        return (m_nIsThemingSupported == 1);
    }

// Operations and theme properties
    HTHEME OpenThemeData(HWND hWnd, LPCWSTR pszClassList)
    {
        if(!IsThemingSupported())
            return NULL;

        //ATLASSERT(m_hTheme == NULL);
        m_hTheme = ::OpenThemeData(hWnd, pszClassList);
        return m_hTheme;
    }

    HRESULT CloseThemeData()
    {
        HRESULT hRet = S_FALSE;
        if(m_hTheme != NULL)
        {
            hRet = ::CloseThemeData(m_hTheme);
            if(SUCCEEDED(hRet))
                m_hTheme = NULL;
        }
        return hRet;
    }

    HRESULT DrawThemeBackground(HDC hDC, int nPartID, int nStateID, LPCRECT pRect, LPCRECT pClipRect = NULL)
    {
        return ::DrawThemeBackground(m_hTheme, hDC, nPartID, nStateID, pRect, pClipRect);
    }

    HRESULT DrawThemeText(HDC hDC, int nPartID, int nStateID, LPCWSTR pszText, int nCharCount, DWORD dwTextFlags, DWORD dwTextFlags2, LPCRECT pRect)
    {
        return ::DrawThemeText(m_hTheme, hDC, nPartID, nStateID, pszText, nCharCount, dwTextFlags, dwTextFlags2, pRect);
    }

    HRESULT GetThemeBackgroundContentRect(HDC hDC, int nPartID, int nStateID,  LPCRECT pBoundingRect, LPRECT pContentRect) const
    {
        return ::GetThemeBackgroundContentRect(m_hTheme, hDC, nPartID, nStateID,  pBoundingRect, pContentRect);
    }

    HRESULT GetThemeBackgroundExtent(HDC hDC, int nPartID, int nStateID, LPCRECT pContentRect, LPRECT pExtentRect) const
    {
        return ::GetThemeBackgroundExtent(m_hTheme, hDC, nPartID, nStateID, pContentRect, pExtentRect);
    }

    HRESULT GetThemePartSize(HDC hDC, int nPartID, int nStateID, LPRECT pRect, enum THEMESIZE eSize, LPSIZE pSize) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemePartSize(m_hTheme, hDC, nPartID, nStateID, pRect, eSize, pSize);
    }

    HRESULT GetThemeTextExtent(HDC hDC, int nPartID, int nStateID, LPCWSTR pszText, int nCharCount, DWORD dwTextFlags, LPCRECT  pBoundingRect, LPRECT pExtentRect) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeTextExtent(m_hTheme, hDC, nPartID, nStateID, pszText, nCharCount, dwTextFlags, pBoundingRect, pExtentRect);
    }

    HRESULT GetThemeTextMetrics(HDC hDC, int nPartID, int nStateID, PTEXTMETRIC pTextMetric) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeTextMetrics(m_hTheme, hDC, nPartID, nStateID, pTextMetric);
    }

    HRESULT GetThemeBackgroundRegion(HDC hDC, int nPartID, int nStateID, LPCRECT pRect, HRGN* pRegion) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeBackgroundRegion(m_hTheme, hDC, nPartID, nStateID, pRect, pRegion);
    }

    HRESULT HitTestThemeBackground(HDC hDC, int nPartID, int nStateID, DWORD dwOptions, LPCRECT pRect, HRGN hrgn, POINT ptTest, WORD* pwHitTestCode) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::HitTestThemeBackground(m_hTheme, hDC, nPartID, nStateID, dwOptions, pRect, hrgn, ptTest, pwHitTestCode);
    }

    HRESULT DrawThemeEdge(HDC hDC, int nPartID, int nStateID, LPCRECT pDestRect, UINT uEdge, UINT uFlags, LPRECT pContentRect = NULL)
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::DrawThemeEdge(m_hTheme, hDC, nPartID, nStateID, pDestRect, uEdge, uFlags, pContentRect);
    }

    HRESULT DrawThemeIcon(HDC hDC, int nPartID, int nStateID, LPCRECT pRect, HIMAGELIST himl, int nImageIndex)
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::DrawThemeIcon(m_hTheme, hDC, nPartID, nStateID, pRect, himl, nImageIndex);
    }

    BOOL IsThemePartDefined(int nPartID, int nStateID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::IsThemePartDefined(m_hTheme, nPartID, nStateID);
    }

    BOOL IsThemeBackgroundPartiallyTransparent(int nPartID, int nStateID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::IsThemeBackgroundPartiallyTransparent(m_hTheme, nPartID, nStateID);
    }

    HRESULT GetThemeColor(int nPartID, int nStateID, int nPropID, COLORREF* pColor) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeColor(m_hTheme, nPartID, nStateID, nPropID, pColor);
    }

    HRESULT GetThemeMetric(HDC hDC, int nPartID, int nStateID, int nPropID, int* pnVal) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeMetric(m_hTheme, hDC, nPartID, nStateID, nPropID, pnVal);
    }

    HRESULT GetThemeString(int nPartID, int nStateID, int nPropID, LPWSTR pszBuff, int cchMaxBuffChars) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeString(m_hTheme, nPartID, nStateID, nPropID, pszBuff, cchMaxBuffChars);
    }

    HRESULT GetThemeBool(int nPartID, int nStateID, int nPropID, BOOL* pfVal) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeBool(m_hTheme, nPartID, nStateID, nPropID, pfVal);
    }

    HRESULT GetThemeInt(int nPartID, int nStateID, int nPropID, int* pnVal) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeInt(m_hTheme, nPartID, nStateID, nPropID, pnVal);
    }

    HRESULT GetThemeEnumValue(int nPartID, int nStateID, int nPropID, int* pnVal) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeEnumValue(m_hTheme, nPartID, nStateID, nPropID, pnVal);
    }

    HRESULT GetThemePosition(int nPartID, int nStateID, int nPropID, LPPOINT pPoint) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemePosition(m_hTheme, nPartID, nStateID, nPropID, pPoint);
    }

    HRESULT GetThemeFont(int nPartID, HDC hDC, int nStateID, int nPropID, LOGFONT* pFont) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeFont(m_hTheme, hDC, nPartID, nStateID, nPropID, pFont);
    }

    HRESULT GetThemeRect(int nPartID, int nStateID, int nPropID, LPRECT pRect) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeRect(m_hTheme, nPartID, nStateID, nPropID, pRect);
    }

    HRESULT GetThemeMargins(HDC hDC, int nPartID, int nStateID, int nPropID, LPRECT pRect, PMARGINS pMargins) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeMargins(m_hTheme, hDC, nPartID, nStateID, nPropID, pRect, pMargins);
    }

    HRESULT GetThemeIntList(int nPartID, int nStateID, int nPropID, INTLIST* pIntList) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeIntList(m_hTheme, nPartID, nStateID, nPropID, pIntList);
    }

    HRESULT GetThemePropertyOrigin(int nPartID, int nStateID, int nPropID, enum PROPERTYORIGIN* pOrigin) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemePropertyOrigin(m_hTheme, nPartID, nStateID, nPropID, pOrigin);
    }

    HRESULT GetThemeFilename(int nPartID, int nStateID, int nPropID, LPWSTR pszThemeFileName, int cchMaxBuffChars) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeFilename(m_hTheme, nPartID, nStateID, nPropID, pszThemeFileName, cchMaxBuffChars);
    }

    COLORREF GetThemeSysColor(int nColorID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysColor(m_hTheme, nColorID);
    }

    HBRUSH GetThemeSysColorBrush(int nColorID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysColorBrush(m_hTheme, nColorID);
    }

    int GetThemeSysSize(int nSizeID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysSize(m_hTheme, nSizeID);
    }

    BOOL GetThemeSysBool(int nBoolID) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysBool(m_hTheme, nBoolID);
    }

    HRESULT GetThemeSysFont(int nFontID, LOGFONT* plf) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysFont(m_hTheme, nFontID, plf);
    }

    HRESULT GetThemeSysString(int nStringID, LPWSTR pszStringBuff, int cchMaxStringChars) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysString(m_hTheme, nStringID, pszStringBuff, cchMaxStringChars);
    }

    HRESULT GetThemeSysInt(int nIntID, int* pnValue) const
    {
        //ATLASSERT(m_hTheme != NULL);
        return ::GetThemeSysInt(m_hTheme, nIntID, pnValue);
    }
};

__declspec(selectany) int CTheme::m_nIsThemingSupported = -1;


// CThemeImpl - theme support implementation

// Derive from this class to implement window with theme support.
// Example:
//  class CMyThemeWindow : public CWindowImpl<CMyThemeWindow>, public CThemeImpl<CMyThemeWindow>
//  {
//  ...
//      BEGIN_MSG_MAP(CMyThemeWindow)
//          CHAIN_MSG_MAP(CThemeImpl<CMyThemeWindow>)
//          ...
//      END_MSG_MAP()
//  ...
//  };
//
// If you set theme class list, the class will automaticaly open/close/reopen theme data.


// Helper for drawing theme client edge
#if 0
inline bool AtlDrawThemeClientEdge(HTHEME hTheme, HWND hWnd, HRGN hRgnUpdate = NULL, HBRUSH hBrush = NULL, int nPartID = 0, int nStateID = 0)
{
    //ATLASSERT(hTheme != NULL);
    //ATLASSERT(::IsWindow(hWnd));

    CWindowDC dc(hWnd);
    if(dc.IsNull())
        return false;

    // Get border size
    int cxBorder = GetSystemMetrics(SM_CXBORDER);
    int cyBorder = GetSystemMetrics(SM_CYBORDER);
    if(SUCCEEDED(::GetThemeInt(hTheme, nPartID, nStateID, TMT_SIZINGBORDERWIDTH, &cxBorder)))
        cyBorder = cxBorder;

    RECT rect;
    ::GetWindowRect(hWnd, &rect);            

    // Remove the client edge from the update region
    int cxEdge = GetSystemMetrics(SM_CXEDGE);
    int cyEdge = GetSystemMetrics(SM_CYEDGE);
    ::InflateRect(&rect, -cxEdge, -cyEdge);
    CRgn rgn;
    rgn.CreateRectRgnIndirect(&rect);
    if(rgn.IsNull())
        return false;

    if(hRgnUpdate != NULL)
        rgn.CombineRgn(hRgnUpdate, rgn, RGN_AND);

    ::OffsetRect(&rect, -rect.left, -rect.top);

    ::OffsetRect(&rect, cxEdge, cyEdge);
    dc.ExcludeClipRect(&rect);
    ::InflateRect(&rect, cxEdge, cyEdge);

    ::DrawThemeBackground(hTheme, dc, nPartID, nStateID, &rect, NULL);

    // Use background brush too, since theme border might not cover everything
    if(cxBorder < cxEdge && cyBorder < cyEdge)
    {
        if(hBrush == NULL)
// need conditional code because types don't match in winuser.h
#ifdef _WIN64
            hBrush = (HBRUSH)::GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND);
#else
            hBrush = (HBRUSH)UlongToPtr(::GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND));
#endif

        ::InflateRect(&rect, cxBorder - cxEdge, cyBorder - cyEdge);
        dc.FillRect(&rect, hBrush);
    }

    ::DefWindowProc(hWnd, WM_NCPAINT, (WPARAM)rgn.m_hRgn, 0L);

    return true;
}


// Theme extended styles
#define THEME_EX_3DCLIENTEDGE       0x00000001
#define THEME_EX_THEMECLIENTEDGE    0x00000002

template <class T, class TBase = CTheme>
class CThemeImpl : public TBase
{
public:
// Data members
    LPWSTR m_lpstrThemeClassList;
    DWORD m_dwExtendedStyle;    // theme specific extended styles

// Constructor & destructor
    CThemeImpl() : m_lpstrThemeClassList(NULL), m_dwExtendedStyle(0)
    { }

    ~CThemeImpl()
    {
        delete m_lpstrThemeClassList;
    }

// Attributes
    bool SetThemeClassList(LPCWSTR lpstrThemeClassList)
    {
        if(m_lpstrThemeClassList != NULL)
        {
            delete m_lpstrThemeClassList;
            m_lpstrThemeClassList = NULL;
        }

        if(lpstrThemeClassList == NULL)
            return true;

        int cchLen = lstrlenW(lpstrThemeClassList) + 1;
        ATLTRY(m_lpstrThemeClassList = new WCHAR[cchLen]);
        if(m_lpstrThemeClassList == NULL)
            return false;

        bool bRet = (lstrcpyW(m_lpstrThemeClassList, lpstrThemeClassList) != NULL);
        if(!bRet)
        {
            delete m_lpstrThemeClassList;
            m_lpstrThemeClassList = NULL;
        }
        return bRet;
    }

    bool GetThemeClassList(LPWSTR lpstrThemeClassList, int cchListBuffer) const
    {
        int cchLen = lstrlenW(m_lpstrThemeClassList) + 1;
        if(cchListBuffer < cchLen)
            return false;

        return (lstrcpyW(lpstrThemeClassList, m_lpstrThemeClassList) != NULL);
    }

    LPCWSTR GetThemeClassList() const
    {
        return m_lpstrThemeClassList;
    }

    DWORD SetThemeExtendedStyle(DWORD dwExtendedStyle, DWORD dwMask = 0)
    {
        DWORD dwPrevStyle = m_dwExtendedStyle;
        if(dwMask == 0)
            m_dwExtendedStyle = dwExtendedStyle;
        else
            m_dwExtendedStyle = (m_dwExtendedStyle & ~dwMask) | (dwExtendedStyle & dwMask);
        return dwPrevStyle;
    }

    DWORD GetThemeExtendedStyle() const
    {
        return m_dwExtendedStyle;
    }

// Operations
    HTHEME OpenThemeData()
    {
        T* pT = static_cast<T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        //ATLASSERT(m_lpstrThemeClassList != NULL);
        if(m_lpstrThemeClassList == NULL)
            return NULL;
        CloseThemeData();
        return TBase::OpenThemeData(pT->m_hWnd, m_lpstrThemeClassList);
    }

    HTHEME OpenThemeData(LPCWSTR pszClassList)
    {
        if(!SetThemeClassList(pszClassList))
            return NULL;
        return OpenThemeData();
    }

    HRESULT SetWindowTheme(LPCWSTR pszSubAppName, LPCWSTR pszSubIDList)
    {
        if(!IsThemingSupported())
            return S_FALSE;

        T* pT = static_cast<T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        return ::SetWindowTheme(pT->m_hWnd, pszSubAppName, pszSubIDList);
    }

    HTHEME GetWindowTheme() const
    {
        if(!IsThemingSupported())
            return NULL;

        const T* pT = static_cast<const T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        return ::GetWindowTheme(pT->m_hWnd);
    }

    HRESULT EnableThemeDialogTexture(BOOL bEnable)
    {
        if(!IsThemingSupported())
            return S_FALSE;

        T* pT = static_cast<T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        return ::EnableThemeDialogTexture(pT->m_hWnd, bEnable);
    }

    BOOL IsThemeDialogTextureEnabled() const
    {
        if(!IsThemingSupported())
            return FALSE;

        const T* pT = static_cast<const T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        return ::IsThemeDialogTextureEnabled(pT->m_hWnd);
    }

    HRESULT DrawThemeParentBackground(HDC hDC, LPRECT pRect = NULL)
    {
        if(!IsThemingSupported())
            return S_FALSE;

        T* pT = static_cast<T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        return ::DrawThemeParentBackground(pT->m_hWnd, hDC, pRect);
    }

// Message map and handlers
    // Note: If you handle any of these messages in your derived class,
    // it is better to put CHAIN_MSG_MAP at the start of your message map.
    BEGIN_MSG_MAP(CThemeImpl)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_THEMECHANGED, OnThemeChanged)
        MESSAGE_HANDLER(WM_NCPAINT, OnNcPaint)
    END_MSG_MAP()

    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        if(m_lpstrThemeClassList != NULL)
            OpenThemeData();
        bHandled = FALSE;
        return 1;
    }

    LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
    {
        CloseThemeData();
        bHandled = FALSE;
        return 1;
    }

    LRESULT OnThemeChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
    {
        CloseThemeData();
        if(m_lpstrThemeClassList != NULL)
            OpenThemeData();
        bHandled = FALSE;
        return 1;
    }

    LRESULT OnNcPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        T* pT = static_cast<T*>(this);
        //ATLASSERT(::IsWindow(pT->m_hWnd));
        LRESULT lRet = 0;
        bHandled = FALSE;
        if(IsThemingSupported() && ((pT->GetExStyle() & WS_EX_CLIENTEDGE) != 0))
        {
            if((m_dwExtendedStyle & THEME_EX_3DCLIENTEDGE) != 0)
            {
                lRet = ::DefWindowProc(pT->m_hWnd, uMsg, wParam, lParam);
                bHandled = TRUE;
            }
            else if((m_hTheme != NULL) && ((m_dwExtendedStyle & THEME_EX_THEMECLIENTEDGE) != 0))
            {
                HRGN hRgn = (wParam != 1) ? (HRGN)wParam : NULL;
                if(pT->DrawThemeClientEdge(hRgn))
                    bHandled = TRUE;
            }
        }
        return lRet;
    }

// Drawing helper
    bool DrawThemeClientEdge(HRGN hRgnUpdate)
    {
        T* pT = static_cast<T*>(this);
        return AtlDrawThemeClientEdge(m_hTheme, pT->m_hWnd, hRgnUpdate, NULL, 0, 0);
    }
};

#endif
}; //namespace WTL

