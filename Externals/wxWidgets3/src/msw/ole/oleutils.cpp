///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/ole/oleutils.cpp
// Purpose:     implementation of OLE helper functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.02.98
// RCS-ID:      $Id: oleutils.cpp 70093 2011-12-22 19:08:43Z SJL $
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_OLE

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#ifndef __CYGWIN10__

#include "wx/msw/private.h"

#ifdef __WXWINCE__
    #include <winreg.h>
    #include <ole2.h>

    #define GUID_DEFINED
    #define UUID_DEFINED
#endif

// OLE
#ifndef __WXWINCE__
#include  "wx/msw/ole/uuid.h"
#endif

#include  "wx/msw/ole/oleutils.h"

#if defined(__VISUALC__) && (__VISUALC__ > 1000)
    #include  <docobj.h>
#endif

// ============================================================================
// Implementation
// ============================================================================

// return true if the iid is in the array
WXDLLEXPORT bool IsIidFromList(REFIID riid, const IID *aIids[], size_t nCount)
{
  for ( size_t i = 0; i < nCount; i++ ) {
    if ( riid == *aIids[i] )
      return true;
  }

  return false;
}

WXDLLEXPORT BSTR wxConvertStringToOle(const wxString& str)
{
    return wxBasicString(str).Get();
}

WXDLLEXPORT wxString wxConvertStringFromOle(BSTR bStr)
{
    // NULL BSTR is equivalent to an empty string (this is the convention used
    // by VB and hence we must follow it)
    if ( !bStr )
        return wxString();

    const int len = SysStringLen(bStr);

#if wxUSE_UNICODE
    wxString str(bStr, len);
#else
    wxString str;
    if (len)
    {
        wxStringBufferLength buf(str, len); // asserts if len == 0
        buf.SetLength(WideCharToMultiByte(CP_ACP, 0 /* no flags */,
                                  bStr, len /* not necessarily NUL-terminated */,
                                  buf, len,
                                  NULL, NULL /* no default char */));
    }
#endif

    return str;
}

// ----------------------------------------------------------------------------
// wxBasicString
// ----------------------------------------------------------------------------

wxBasicString::wxBasicString(const wxString& str)
{
    m_bstrBuf = SysAllocString(str.wc_str(*wxConvCurrent));
}

wxBasicString::wxBasicString(const wxBasicString& src)
{
    m_bstrBuf = src.Get();
}

wxBasicString& wxBasicString::operator=(const wxBasicString& src)
{
    SysReAllocString(&m_bstrBuf, src);
    return *this;
}

wxBasicString::~wxBasicString()
{
    SysFreeString(m_bstrBuf);
}


// ----------------------------------------------------------------------------
// Convert variants
// ----------------------------------------------------------------------------

#if wxUSE_VARIANT

WXDLLEXPORT bool wxConvertVariantToOle(const wxVariant& variant, VARIANTARG& oleVariant)
{
    VariantInit(&oleVariant);
    if (variant.IsNull())
    {
        oleVariant.vt = VT_NULL;
        return true;
    }

    wxString type(variant.GetType());


    if (type == wxT("long"))
    {
        oleVariant.vt = VT_I4;
        oleVariant.lVal = variant.GetLong() ;
    }
    else if (type == wxT("char"))
    {
        oleVariant.vt=VT_I1;            // Signed Char
        oleVariant.cVal=variant.GetChar();
    }
    else if (type == wxT("double"))
    {
        oleVariant.vt = VT_R8;
        oleVariant.dblVal = variant.GetDouble();
    }
    else if (type == wxT("bool"))
    {
        oleVariant.vt = VT_BOOL;
        oleVariant.boolVal = variant.GetBool() ? VARIANT_TRUE : VARIANT_FALSE;
    }
    else if (type == wxT("string"))
    {
        wxString str( variant.GetString() );
        oleVariant.vt = VT_BSTR;
        oleVariant.bstrVal = wxConvertStringToOle(str);
    }
#if wxUSE_DATETIME
    else if (type == wxT("datetime"))
    {
        wxDateTime date( variant.GetDateTime() );
        oleVariant.vt = VT_DATE;

        SYSTEMTIME st;
        date.GetAsMSWSysTime(&st);

        SystemTimeToVariantTime(&st, &oleVariant.date);
    }
#endif
    else if (type == wxT("void*"))
    {
        oleVariant.vt = VT_DISPATCH;
        oleVariant.pdispVal = (IDispatch*) variant.GetVoidPtr();
    }
    else if (type == wxT("list") || type == wxT("stringlist"))
    {
        oleVariant.vt = VT_VARIANT | VT_ARRAY;

        SAFEARRAY *psa;
        SAFEARRAYBOUND saBound;
        VARIANTARG *pvargBase;
        VARIANTARG *pvarg;
        int i, j;

        int iCount = variant.GetCount();

        saBound.lLbound = 0;
        saBound.cElements = iCount;

        psa = SafeArrayCreate(VT_VARIANT, 1, &saBound);
        if (psa == NULL)
            return false;

        SafeArrayAccessData(psa, (void**)&pvargBase);

        pvarg = pvargBase;
        for (i = 0; i < iCount; i++)
        {
            // copy each string in the list of strings
            wxVariant eachVariant(variant[i]);
            if (!wxConvertVariantToOle(eachVariant, * pvarg))
            {
                // memory failure:  back out and free strings alloc'ed up to
                // now, and then the array itself.
                pvarg = pvargBase;
                for (j = 0; j < i; j++)
                {
                    SysFreeString(pvarg->bstrVal);
                    pvarg++;
                }
                SafeArrayDestroy(psa);
                return false;
            }
            pvarg++;
        }

        SafeArrayUnaccessData(psa);

        oleVariant.parray = psa;
    }
    else
    {
        oleVariant.vt = VT_NULL;
        return false;
    }
    return true;
}

#ifndef VT_TYPEMASK
#define VT_TYPEMASK 0xfff
#endif

WXDLLEXPORT bool
wxConvertOleToVariant(const VARIANTARG& oleVariant, wxVariant& variant)
{
    bool ok = true;
    if ( oleVariant.vt & VT_ARRAY )
    {

        // Compute the total number of elements in all array dimensions
        int cElements = 1;
        for ( int cDims = 0; cDims < oleVariant.parray->cDims; cDims++ )
            cElements *= oleVariant.parray->rgsabound[cDims].cElements;

        // Get a pointer to the data
        void* pvdata;
        HRESULT hr = SafeArrayAccessData(oleVariant.parray, &pvdata);
        if ( FAILED(hr) )
            return false;

        switch (oleVariant.vt & VT_TYPEMASK)
        {
            case VT_VARIANT:
                {
                    variant.ClearList();
                    VARIANTARG *variant_data=(VARIANTARG*)pvdata;
                    for ( int i = 0; i < cElements; i++ )
                    {
                        VARIANTARG& oleElement = variant_data[i];
                        wxVariant vElement;
                        if ( !wxConvertOleToVariant(oleElement, vElement) )
                        {
                            ok = false;
                            variant.ClearList();
                            break;
                        }

                        variant.Append(vElement);
                    }
                }
                break;

            case VT_BSTR:
                {
                    wxArrayString strings;
                    BSTR *string_val=(BSTR*)pvdata;
                    for ( int i = 0; i < cElements; ++i )
                    {
                        wxString str=wxConvertStringFromOle(*string_val);
                        strings.Add(str);
                        ++string_val;
                    }
                    variant=strings;
                }
                break;

            default:
                wxLogDebug(wxT("unhandled VT_ARRAY type %x in wxConvertOleToVariant"),
                           oleVariant.vt & VT_TYPEMASK);
                variant = wxVariant();
                ok = false;
                break;
        }

        SafeArrayUnaccessData(oleVariant.parray);
    }
    else if ( oleVariant.vt & VT_BYREF )
    {
        switch ( oleVariant.vt & VT_TYPEMASK )
        {
            case VT_VARIANT:
                {
                    VARIANTARG& oleReference = *((LPVARIANT)oleVariant.byref);
                    if (!wxConvertOleToVariant(oleReference,variant))
                        return false;
                    break;
                }

            default:
                wxLogError(wxT("wxAutomationObject::ConvertOleToVariant: [as yet] unhandled reference %X"),
                            oleVariant.vt);
                return false;
        }
    }
    else // simply type (not array or reference)
    {
        switch ( oleVariant.vt & VT_TYPEMASK )
        {
            case VT_BSTR:
                {
                    wxString str(wxConvertStringFromOle(oleVariant.bstrVal));
                    variant = str;
                }
                break;

            case VT_DATE:
#if wxUSE_DATETIME
                {
                    SYSTEMTIME st;
                    VariantTimeToSystemTime(oleVariant.date, &st);

                    wxDateTime date;
                    date.SetFromMSWSysTime(st);
                    variant = date;
                }
#endif // wxUSE_DATETIME
                break;

            case VT_I4:
                variant = (long) oleVariant.lVal;
                break;

            case VT_I2:
                variant = (long) oleVariant.iVal;
                break;

            case VT_BOOL:
                variant = oleVariant.boolVal != 0;
                break;

            case VT_R4:
                variant = oleVariant.fltVal;
                break;

            case VT_R8:
                variant = oleVariant.dblVal;
                break;

            case VT_DISPATCH:
                variant = (void*) oleVariant.pdispVal;
                break;

            case VT_NULL:
                variant.MakeNull();
                break;

            case VT_EMPTY:
                break;    // Ignore Empty Variant, used only during destruction of objects

            default:
                wxLogError(wxT("wxAutomationObject::ConvertOleToVariant: Unknown variant value type %X -> %X"),
                           oleVariant.vt,oleVariant.vt&VT_TYPEMASK);
                return false;
        }
    }

    return ok;
}

#endif // wxUSE_VARIANT


// ----------------------------------------------------------------------------
// Debug support
// ----------------------------------------------------------------------------

#if wxUSE_DATAOBJ

#if wxDEBUG_LEVEL && ( ( defined(__VISUALC__) && (__VISUALC__ > 1000) ) || defined(__MWERKS__) )
static wxString GetIidName(REFIID riid)
{
  // an association between symbolic name and numeric value of an IID
  struct KNOWN_IID {
    const IID  *pIid;
    const wxChar *szName;
  };

  // construct the table containing all known interfaces
  #define ADD_KNOWN_IID(name) { &IID_I##name, wxT(#name) }

  static const KNOWN_IID aKnownIids[] = {
    ADD_KNOWN_IID(AdviseSink),
    ADD_KNOWN_IID(AdviseSink2),
    ADD_KNOWN_IID(BindCtx),
    ADD_KNOWN_IID(ClassFactory),
#if ( !defined( __VISUALC__) || (__VISUALC__!=1010) ) && !defined(__MWERKS__)
    ADD_KNOWN_IID(ContinueCallback),
    ADD_KNOWN_IID(EnumOleDocumentViews),
    ADD_KNOWN_IID(OleCommandTarget),
    ADD_KNOWN_IID(OleDocument),
    ADD_KNOWN_IID(OleDocumentSite),
    ADD_KNOWN_IID(OleDocumentView),
    ADD_KNOWN_IID(Print),
#endif
    ADD_KNOWN_IID(DataAdviseHolder),
    ADD_KNOWN_IID(DataObject),
    ADD_KNOWN_IID(Debug),
    ADD_KNOWN_IID(DebugStream),
    ADD_KNOWN_IID(DfReserved1),
    ADD_KNOWN_IID(DfReserved2),
    ADD_KNOWN_IID(DfReserved3),
    ADD_KNOWN_IID(Dispatch),
    ADD_KNOWN_IID(DropSource),
    ADD_KNOWN_IID(DropTarget),
    ADD_KNOWN_IID(EnumCallback),
    ADD_KNOWN_IID(EnumFORMATETC),
    ADD_KNOWN_IID(EnumGeneric),
    ADD_KNOWN_IID(EnumHolder),
    ADD_KNOWN_IID(EnumMoniker),
    ADD_KNOWN_IID(EnumOLEVERB),
    ADD_KNOWN_IID(EnumSTATDATA),
    ADD_KNOWN_IID(EnumSTATSTG),
    ADD_KNOWN_IID(EnumString),
    ADD_KNOWN_IID(EnumUnknown),
    ADD_KNOWN_IID(EnumVARIANT),
    ADD_KNOWN_IID(ExternalConnection),
    ADD_KNOWN_IID(InternalMoniker),
    ADD_KNOWN_IID(LockBytes),
    ADD_KNOWN_IID(Malloc),
    ADD_KNOWN_IID(Marshal),
    ADD_KNOWN_IID(MessageFilter),
    ADD_KNOWN_IID(Moniker),
    ADD_KNOWN_IID(OleAdviseHolder),
    ADD_KNOWN_IID(OleCache),
    ADD_KNOWN_IID(OleCache2),
    ADD_KNOWN_IID(OleCacheControl),
    ADD_KNOWN_IID(OleClientSite),
    ADD_KNOWN_IID(OleContainer),
    ADD_KNOWN_IID(OleInPlaceActiveObject),
    ADD_KNOWN_IID(OleInPlaceFrame),
    ADD_KNOWN_IID(OleInPlaceObject),
    ADD_KNOWN_IID(OleInPlaceSite),
    ADD_KNOWN_IID(OleInPlaceUIWindow),
    ADD_KNOWN_IID(OleItemContainer),
    ADD_KNOWN_IID(OleLink),
    ADD_KNOWN_IID(OleManager),
    ADD_KNOWN_IID(OleObject),
    ADD_KNOWN_IID(OlePresObj),
    ADD_KNOWN_IID(OleWindow),
    ADD_KNOWN_IID(PSFactory),
    ADD_KNOWN_IID(ParseDisplayName),
    ADD_KNOWN_IID(Persist),
    ADD_KNOWN_IID(PersistFile),
    ADD_KNOWN_IID(PersistStorage),
    ADD_KNOWN_IID(PersistStream),
    ADD_KNOWN_IID(ProxyManager),
    ADD_KNOWN_IID(RootStorage),
    ADD_KNOWN_IID(RpcChannel),
    ADD_KNOWN_IID(RpcProxy),
    ADD_KNOWN_IID(RpcStub),
    ADD_KNOWN_IID(RunnableObject),
    ADD_KNOWN_IID(RunningObjectTable),
    ADD_KNOWN_IID(StdMarshalInfo),
    ADD_KNOWN_IID(Storage),
    ADD_KNOWN_IID(Stream),
    ADD_KNOWN_IID(StubManager),
    ADD_KNOWN_IID(Unknown),
    ADD_KNOWN_IID(ViewObject),
    ADD_KNOWN_IID(ViewObject2),
  };

  // don't clobber preprocessor name space
  #undef ADD_KNOWN_IID

  // try to find the interface in the table
  for ( size_t ui = 0; ui < WXSIZEOF(aKnownIids); ui++ ) {
    if ( riid == *aKnownIids[ui].pIid ) {
      return aKnownIids[ui].szName;
    }
  }

#ifndef __WXWINCE__
  // unknown IID, just transform to string
  Uuid uuid(riid);
  return wxString((const wxChar *)uuid);
#else
  return wxEmptyString;
#endif
}

WXDLLEXPORT void wxLogQueryInterface(const wxChar *szInterface, REFIID riid)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("%s::QueryInterface (iid = %s)"),
             szInterface, GetIidName(riid).c_str());
}

WXDLLEXPORT void wxLogAddRef(const wxChar *szInterface, ULONG cRef)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("After %s::AddRef: m_cRef = %d"), szInterface, cRef + 1);
}

WXDLLEXPORT void wxLogRelease(const wxChar *szInterface, ULONG cRef)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("After %s::Release: m_cRef = %d"), szInterface, cRef - 1);
}

#endif  // wxDEBUG_LEVEL

#endif // wxUSE_DATAOBJ

#endif // __CYGWIN10__

#endif // wxUSE_OLE
