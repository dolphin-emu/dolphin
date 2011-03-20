/*
 *  Name:        wx/cpp.h
 *  Purpose:     Various preprocessor helpers
 *  Author:      Vadim Zeitlin
 *  Created:     2006-09-30
 *  RCS-ID:      $Id: cpp.h 66054 2010-11-07 13:16:20Z VZ $
 *  Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
 *  Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_CPP_H_
#define _WX_CPP_H_

/* wxCONCAT works like preprocessor ## operator but also works with macros */
#define wxCONCAT_HELPER(text, line) text ## line
#define wxCONCAT(text, line)        wxCONCAT_HELPER(text, line)

#define wxCONCAT3(x1, x2, x3) wxCONCAT(wxCONCAT(x1, x2), x3)
#define wxCONCAT4(x1, x2, x3, x4) wxCONCAT(wxCONCAT3(x1, x2, x3), x4)
#define wxCONCAT5(x1, x2, x3, x4, x5) wxCONCAT(wxCONCAT4(x1, x2, x3, x4), x5)

/* wxSTRINGIZE works as the preprocessor # operator but also works with macros */
#define wxSTRINGIZE_HELPER(x)       #x
#define wxSTRINGIZE(x)              wxSTRINGIZE_HELPER(x)

/* a Unicode-friendly version of wxSTRINGIZE_T */
#define wxSTRINGIZE_T(x)            wxAPPLY_T(wxSTRINGIZE(x))

/*
    Special workarounds for compilers with broken "##" operator. For all the
    other ones we can just use it directly.
 */
#ifdef wxCOMPILER_BROKEN_CONCAT_OPER
    #define wxPREPEND_L(x)      L ## x
    #define wxAPPEND_i64(x)     x ## i64
    #define wxAPPEND_ui64(x)    x ## ui64
#endif /* wxCOMPILER_BROKEN_CONCAT_OPER */

/*
   Helper macros for wxMAKE_UNIQUE_NAME: normally this works by appending the
   current line number to the given identifier to reduce the probability of the
   conflict (it may still happen if this is used in the headers, hence you
   should avoid doing it or provide unique prefixes then) but we have to do it
   differently for VC++
  */
#if defined(__VISUALC__) && (__VISUALC__ >= 1300)
    /*
       __LINE__ handling is completely broken in VC++ when using "Edit and
       Continue" (/ZI option) and results in preprocessor errors if we use it
       inside the macros. Luckily VC7 has another standard macro which can be
       used like this and is even better than __LINE__ because it is globally
       unique.
     */
#   define wxCONCAT_LINE(text)         wxCONCAT(text, __COUNTER__)
#else /* normal compilers */
#   define wxCONCAT_LINE(text)         wxCONCAT(text, __LINE__)
#endif

/* Create a "unique" name with the given prefix */
#define wxMAKE_UNIQUE_NAME(text)    wxCONCAT_LINE(text)

/*
   This macro can be passed as argument to another macro when you don't have
   anything to pass in fact.
 */
#define wxEMPTY_PARAMETER_VALUE /* Fake macro parameter value */

/*
    Define __WXFUNCTION__ which is like standard __FUNCTION__ but defined as
    NULL for the compilers which don't support the latter.
 */
#ifndef __WXFUNCTION__
    /* TODO: add more compilers supporting __FUNCTION__ */
    #if defined(__DMC__)
        /*
           __FUNCTION__ happens to be not defined within class members
           http://www.digitalmars.com/drn-bin/wwwnews?c%2B%2B.beta/485
        */
        #define __WXFUNCTION__ (NULL)
    #elif defined(__GNUC__) || \
          (defined(_MSC_VER) && _MSC_VER >= 1300) || \
          defined(__FUNCTION__)
        #define __WXFUNCTION__ __FUNCTION__
    #else
        /* still define __WXFUNCTION__ to avoid #ifdefs elsewhere */
        #define __WXFUNCTION__ (NULL)
    #endif
#endif /* __WXFUNCTION__ already defined */

#endif /* _WX_CPP_H_ */

