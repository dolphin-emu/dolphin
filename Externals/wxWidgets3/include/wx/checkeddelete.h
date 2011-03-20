///////////////////////////////////////////////////////////////////////////////
// Name:        wx/checkeddelete.h
// Purpose:     wxCHECKED_DELETE() macro
// Author:      Vadim Zeitlin
// Created:     2009-02-03
// RCS-ID:      $Id: checkeddelete.h 58634 2009-02-03 12:01:46Z VZ $
// Copyright:   (c) 2002-2009 wxWidgets dev team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKEDDELETE_H_
#define _WX_CHECKEDDELETE_H_

// TODO: provide wxCheckedDelete[Array]() template functions too

// ----------------------------------------------------------------------------
// wxCHECKED_DELETE and wxCHECKED_DELETE_ARRAY macros
// ----------------------------------------------------------------------------

/*
   checked deleters are used to make sure that the type being deleted is really
   a complete type.: otherwise sizeof() would result in a compile-time error

   do { ... } while ( 0 ) construct is used to have an anonymous scope
   (otherwise we could have name clashes between different "complete"s) but
   still force a semicolon after the macro
*/

#ifdef __WATCOMC__
    #define wxFOR_ONCE(name)              for(int name=0; name<1; name++)
    #define wxPRE_NO_WARNING_SCOPE(name)  wxFOR_ONCE(wxMAKE_UNIQUE_NAME(name))
    #define wxPOST_NO_WARNING_SCOPE(name)
#else
    #define wxPRE_NO_WARNING_SCOPE(name)  do
    #define wxPOST_NO_WARNING_SCOPE(name) while ( wxFalse )
#endif

#define wxCHECKED_DELETE(ptr)                                                 \
    wxPRE_NO_WARNING_SCOPE(scope_var1)                                        \
    {                                                                         \
        typedef char complete[sizeof(*ptr)];                                  \
        delete ptr;                                                           \
    } wxPOST_NO_WARNING_SCOPE(scope_var1)

#define wxCHECKED_DELETE_ARRAY(ptr)                                           \
    wxPRE_NO_WARNING_SCOPE(scope_var2)                                        \
    {                                                                         \
        typedef char complete[sizeof(*ptr)];                                  \
        delete [] ptr;                                                        \
    } wxPOST_NO_WARNING_SCOPE(scope_var2)


#endif // _WX_CHECKEDDELETE_H_

