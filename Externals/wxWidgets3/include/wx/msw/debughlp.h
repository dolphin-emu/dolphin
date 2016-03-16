///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/debughlp.h
// Purpose:     wraps dbghelp.h standard file
// Author:      Vadim Zeitlin, Suzumizaki-kimitaka
// Created:     2005-01-08 (extracted from msw/crashrpt.cpp)
// Copyright:   (c) 2003-2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_DEBUGHLPH_H_
#define _WX_MSW_DEBUGHLPH_H_

#include "wx/dynlib.h"

#include "wx/msw/wrapwin.h"

#ifdef __VISUALC__
    // Disable a warning that we can do nothing about: we get it at least for
    // imagehlp.h from 8.1 Windows kit when using VC14.
    #pragma warning(push)

    // 'typedef ': ignored on left of '' when no variable is declared
    #pragma warning(disable:4091)
#endif

#include <imagehlp.h>

#ifdef __VISUALC__
  #pragma warning(pop)
#endif

#include "wx/msw/private.h"

// wxUSE_DBGHELP can be predefined on the compiler command line to force using
// dbghelp.dll even if it's not detected or, on the contrary, avoid using even
// if it's available.
#ifndef wxUSE_DBGHELP
    // The only compiler which is known to have the necessary headers is MSVC.
    #ifdef __VISUALC__
        // MSVC7.1 shipped with API v9 and we don't support anything earlier
        // anyhow.
        #if API_VERSION_NUMBER >= 9
            #define wxUSE_DBGHELP 1
        #else
            #define wxUSE_DBGHELP 0
        #endif
    #else
        #define wxUSE_DBGHELP 0
    #endif
#endif

#if wxUSE_DBGHELP

/*

The table below shows which functions are exported by dbghelp.dll.

On 64 bit Windows, there seems to be no difference between 32bit dll and 64bit
one. Vista-64 and Win8-64 look the same, but "Ex" and "ExW" versions exist only
in Windows 8.

Note that SymGetLineFromAddrW and EnumerateLoadedModulesW DON'T exist at all.

function | Windows     | XP-32 Vista-64 Win8-64
SymEnumSymbolsW            n/a     v       v
SymFromAddrW               n/a     v       v
SymInitializeW             n/a     v       v

SymEnumSymbols              v      v       v
SymFromAddr                 v      v       v
SymInitialize               v      v       v

SymGetLineFromAddrW64      n/a     v       v
SymGetLineFromAddr64        v      v       v
SymGetLineFromAddrW        n/a    n/a     n/a
SymGetLineFromAddr          v      v       v

EnumerateLoadedModulesW64  n/a     v       v
EnumerateLoadedModules64    v      v       v
EnumerateLoadedModulesW    n/a    n/a     n/a
EnumerateLoadedModules      v      v       v

*/

// It's not really clear whether API v10 is used by anything as VC8 still used
// v9, just as MSVC7.1, while VC9 already used v11, but provide support for it
// just in case.
#if API_VERSION_NUMBER < 10/*{{{*/

typedef BOOL
(CALLBACK *PENUMLOADED_MODULES_CALLBACKW64)(PWSTR ModuleName,
                                            DWORD64 ModuleBase,
                                            ULONG ModuleSize,
                                            PVOID UserContext);

typedef struct _IMAGEHLP_LINEW64
{
    DWORD    SizeOfStruct;
    PVOID    Key;
    DWORD    LineNumber;
    PWSTR    FileName;
    DWORD64  Address;
} IMAGEHLP_LINEW64, *PIMAGEHLP_LINEW64;

typedef struct _SYMBOL_INFOW
{
    ULONG       SizeOfStruct;
    ULONG       TypeIndex;
    ULONG64     Reserved[2];
    ULONG       Index;
    ULONG       Size;
    ULONG64     ModBase;
    ULONG       Flags;
    ULONG64     Value;
    ULONG64     Address;
    ULONG       Register;
    ULONG       Scope;
    ULONG       Tag;
    ULONG       NameLen;
    ULONG       MaxNameLen;
    WCHAR       Name[1];
} SYMBOL_INFOW, *PSYMBOL_INFOW;

typedef BOOL
(CALLBACK *PSYM_ENUMERATESYMBOLS_CALLBACKW)(PSYMBOL_INFOW pSymInfo,
                                            ULONG SymbolSize,
                                            PVOID UserContext);

typedef BOOL
(CALLBACK *PSYM_ENUMERATESYMBOLS_CALLBACK)(PSYMBOL_INFO pSymInfo,
                                           ULONG SymbolSize,
                                           PVOID UserContext);

#endif // API_VERSION_NUMBER < 10/*}}}*/

// wx-prefixed types map to either the ANSI or Unicode ("W") version depending
// on the build of wx itself.
#ifdef UNICODE
    #define wxPSYM_ENUMERATESYMBOLS_CALLBACK PSYM_ENUMERATESYMBOLS_CALLBACKW
#else // !UNICODE
    #define wxPSYM_ENUMERATESYMBOLS_CALLBACK PSYM_ENUMERATESYMBOLS_CALLBACK
#endif // UNICODE/!UNICODE

// This one could be already defined by wx/msw/stackwalk.h
#ifndef wxSYMBOL_INFO
    #ifdef UNICODE
        #define wxSYMBOL_INFO SYMBOL_INFOW
    #else // !UNICODE
        #define wxSYMBOL_INFO SYMBOL_INFO
    #endif // UNICODE/!UNICODE
#endif // !defined(wxSYMBOL_INFO)

typedef wxSYMBOL_INFO* wxPSYMBOL_INFO;

// This differs from PENUMLOADED_MODULES_CALLBACK[W]64 in that it always uses
// "const" for its first argument when the SDK used to pass a non-const string
// here until API_VERSION_NUMBER==11, so we can't just define it as an existing
// typedef.
typedef BOOL
(CALLBACK *wxPENUMLOADED_MODULES_CALLBACK)(const wxChar* moduleName,
                                           DWORD64 moduleBase,
                                           ULONG moduleSize,
                                           void *userContext);

// ----------------------------------------------------------------------------
// wxDbgHelpDLL: dynamically load dbghelp.dll functions
// ----------------------------------------------------------------------------

// wrapper for some functions from dbghelp.dll
//
// MT note: this class is not MT safe and should be only used from a single
//          thread at a time (this is so because dbghelp.dll is not MT-safe
//          itself anyhow)
class wxDbgHelpDLL
{
public:
    // some useful constants not present in debughlp.h (stolen from DIA SDK)
    enum BasicType
    {
        BASICTYPE_NOTYPE = 0,
        BASICTYPE_VOID = 1,
        BASICTYPE_CHAR = 2,
        BASICTYPE_WCHAR = 3,
        BASICTYPE_INT = 6,
        BASICTYPE_UINT = 7,
        BASICTYPE_FLOAT = 8,
        BASICTYPE_BCD = 9,
        BASICTYPE_BOOL = 10,
        BASICTYPE_LONG = 13,
        BASICTYPE_ULONG = 14,
        BASICTYPE_CURRENCY = 25,
        BASICTYPE_DATE = 26,
        BASICTYPE_VARIANT = 27,
        BASICTYPE_COMPLEX = 28,
        BASICTYPE_BIT = 29,
        BASICTYPE_BSTR = 30,
        BASICTYPE_HRESULT = 31,
        BASICTYPE_MAX
    };

    enum SymbolTag
    {
        SYMBOL_TAG_NULL,
        SYMBOL_TAG_EXE,
        SYMBOL_TAG_COMPILAND,
        SYMBOL_TAG_COMPILAND_DETAILS,
        SYMBOL_TAG_COMPILAND_ENV,
        SYMBOL_TAG_FUNCTION,
        SYMBOL_TAG_BLOCK,
        SYMBOL_TAG_DATA,
        SYMBOL_TAG_ANNOTATION,
        SYMBOL_TAG_LABEL,
        SYMBOL_TAG_PUBLIC_SYMBOL,
        SYMBOL_TAG_UDT,
        SYMBOL_TAG_ENUM,
        SYMBOL_TAG_FUNCTION_TYPE,
        SYMBOL_TAG_POINTER_TYPE,
        SYMBOL_TAG_ARRAY_TYPE,
        SYMBOL_TAG_BASE_TYPE,
        SYMBOL_TAG_TYPEDEF,
        SYMBOL_TAG_BASE_CLASS,
        SYMBOL_TAG_FRIEND,
        SYMBOL_TAG_FUNCTION_ARG_TYPE,
        SYMBOL_TAG_FUNC_DEBUG_START,
        SYMBOL_TAG_FUNC_DEBUG_END,
        SYMBOL_TAG_USING_NAMESPACE,
        SYMBOL_TAG_VTABLE_SHAPE,
        SYMBOL_TAG_VTABLE,
        SYMBOL_TAG_CUSTOM,
        SYMBOL_TAG_THUNK,
        SYMBOL_TAG_CUSTOM_TYPE,
        SYMBOL_TAG_MANAGED_TYPE,
        SYMBOL_TAG_DIMENSION,
        SYMBOL_TAG_MAX
    };

    enum DataKind
    {
        DATA_UNKNOWN,
        DATA_LOCAL,
        DATA_STATIC_LOCAL,
        DATA_PARAM,
        DATA_OBJECT_PTR,                    // "this" pointer
        DATA_FILE_STATIC,
        DATA_GLOBAL,
        DATA_MEMBER,
        DATA_STATIC_MEMBER,
        DATA_CONSTANT,
        DATA_MAX
    };

    enum UdtKind
    {
        UDT_STRUCT,
        UDT_CLASS,
        UDT_UNION,
        UDT_MAX
    };


    // function types
    typedef DWORD (WINAPI *SymGetOptions_t)();
    typedef DWORD (WINAPI *SymSetOptions_t)(DWORD);
    typedef BOOL (WINAPI *SymInitialize_t)(HANDLE, LPCSTR, BOOL);
    typedef BOOL (WINAPI *SymInitializeW_t)(HANDLE, LPCWSTR, BOOL);
    typedef BOOL (WINAPI *StackWalk_t)(DWORD, HANDLE, HANDLE, LPSTACKFRAME,
                                       LPVOID, PREAD_PROCESS_MEMORY_ROUTINE,
                                       PFUNCTION_TABLE_ACCESS_ROUTINE,
                                       PGET_MODULE_BASE_ROUTINE,
                                       PTRANSLATE_ADDRESS_ROUTINE);
    typedef BOOL (WINAPI *SymFromAddr_t)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    typedef BOOL (WINAPI *SymFromAddrW_t)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFOW);
    typedef LPVOID (WINAPI *SymFunctionTableAccess_t)(HANDLE, DWORD_PTR);
    typedef DWORD_PTR (WINAPI *SymGetModuleBase_t)(HANDLE, DWORD_PTR);
    typedef BOOL (WINAPI *SymGetLineFromAddr_t)(HANDLE, DWORD,
                                                PDWORD, PIMAGEHLP_LINE);
    typedef BOOL (WINAPI *SymGetLineFromAddr64_t)(HANDLE, DWORD64,
                                                  PDWORD, PIMAGEHLP_LINE64);
    typedef BOOL (WINAPI *SymGetLineFromAddrW64_t)(HANDLE, DWORD64,
                                                   PDWORD, PIMAGEHLP_LINEW64);
    typedef BOOL (WINAPI *SymSetContext_t)(HANDLE, PIMAGEHLP_STACK_FRAME,
                                           PIMAGEHLP_CONTEXT);
    typedef BOOL (WINAPI *SymEnumSymbols_t)(HANDLE, ULONG64, PCSTR,
                                            PSYM_ENUMERATESYMBOLS_CALLBACK,
                                            const PVOID);
    typedef BOOL (WINAPI *SymEnumSymbolsW_t)(HANDLE, ULONG64, PCWSTR,
                                             PSYM_ENUMERATESYMBOLS_CALLBACKW,
                                             const PVOID);
    typedef BOOL (WINAPI *SymGetTypeInfo_t)(HANDLE, DWORD64, ULONG,
                                            IMAGEHLP_SYMBOL_TYPE_INFO, PVOID);
    typedef BOOL (WINAPI *SymCleanup_t)(HANDLE);
    typedef BOOL (WINAPI *EnumerateLoadedModules_t)(HANDLE, PENUMLOADED_MODULES_CALLBACK, PVOID);
    typedef BOOL (WINAPI *EnumerateLoadedModules64_t)(HANDLE, PENUMLOADED_MODULES_CALLBACK64, PVOID);
    typedef BOOL (WINAPI *EnumerateLoadedModulesW64_t)(HANDLE, PENUMLOADED_MODULES_CALLBACKW64, PVOID);
    typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
                                               MINIDUMP_TYPE,
                                               CONST PMINIDUMP_EXCEPTION_INFORMATION,
                                               CONST PMINIDUMP_USER_STREAM_INFORMATION,
                                               CONST PMINIDUMP_CALLBACK_INFORMATION);

    // Higher level functions selecting the right debug help library function
    // to call: for CallFoo(), it can be Foo(), Foo64(), FooW() or FooW64()
    // depending on the build options and function availability.
    //
    // They also provide more convenient to use wx-specific API, e.g. work with
    // wxString instead of char/wchar_t pointers and omit the arguments we
    // don't need.
    static BOOL CallSymInitialize(HANDLE, BOOL);
    static BOOL CallEnumerateLoadedModules(HANDLE, wxPENUMLOADED_MODULES_CALLBACK, PVOID);
    static BOOL CallSymFromAddr(HANDLE, DWORD64,
                                size_t* offset, wxString* name);
    static BOOL CallSymGetLineFromAddr(HANDLE, DWORD64,
                                       wxString* fileName, size_t* line);
    static BOOL CallSymEnumSymbols(HANDLE hProcess,
                                   ULONG64 baseOfDll,
                                   wxPSYM_ENUMERATESYMBOLS_CALLBACK callback,
                                   const PVOID callbackParam);

    // The macro called by wxDO_FOR_ALL_SYM_FUNCS() below takes 2 arguments:
    // the name of the function in the program code, which never has "64"
    // suffix, and the name of the function in the DLL which can have "64"
    // suffix in some cases. These 2 helper macros call the macro with the
    // correct arguments in both cases.
    #define wxSYM_CALL(what, name)  what(name, name)
#if defined(_M_AMD64)
    #define wxSYM_CALL_64(what, name)  what(name, name ## 64)

    // Also undo all the "helpful" definitions done by imagehlp.h that map 32
    // bit functions to 64 bit ones, we don't need this as we do it ourselves.
    #undef StackWalk
    #undef SymFunctionTableAccess
    #undef SymGetModuleBase
    #undef SymGetLineFromAddr
    #undef EnumerateLoadedModules
#else
    #define wxSYM_CALL_64(what, name)  what(name, name)
#endif

    #define wxSYM_CALL_ALWAYS_W(what, name)  what(name ## W, name ## W)

    #define wxSYM_CALL_ALTERNATIVES(what, name)                \
        what(name, name);                                      \
        what(name ## 64, name ## 64);                          \
        what(name ## W64, name ## W64)

    #define wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PUBLIC(what)       \
        wxSYM_CALL_64(what, StackWalk);                        \
        wxSYM_CALL_64(what, SymFunctionTableAccess);           \
        wxSYM_CALL_64(what, SymGetModuleBase);                 \
                                                               \
        wxSYM_CALL(what, SymGetOptions);                       \
        wxSYM_CALL(what, SymSetOptions);                       \
        wxSYM_CALL(what, SymSetContext);                       \
        wxSYM_CALL(what, SymGetTypeInfo);                      \
        wxSYM_CALL(what, SymCleanup);                          \
        wxSYM_CALL(what, MiniDumpWriteDump)

    #define wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PRIVATE(what)      \
        wxSYM_CALL(what, SymInitialize);                       \
        wxSYM_CALL(what, SymFromAddr);                         \
        wxSYM_CALL(what, SymEnumSymbols)

    #define wxDO_FOR_ALL_SYM_FUNCS_REQUIRED(what)              \
        wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PRIVATE(what);         \
        wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PUBLIC(what)

    // Alternation will work when the following functions are not found,
    // therefore they are not included in REQUIRED version.
    #define wxDO_FOR_ALL_SYM_FUNCS_OPTIONAL(what)              \
        wxSYM_CALL_ALTERNATIVES(what, SymGetLineFromAddr);     \
        wxSYM_CALL_ALTERNATIVES(what, EnumerateLoadedModules); \
        wxSYM_CALL_ALWAYS_W(what, SymInitialize);              \
        wxSYM_CALL_ALWAYS_W(what, SymFromAddr);                \
        wxSYM_CALL_ALWAYS_W(what, SymEnumSymbols)

    #define wxDO_FOR_ALL_SYM_FUNCS(what)                       \
        wxDO_FOR_ALL_SYM_FUNCS_REQUIRED(what);                 \
        wxDO_FOR_ALL_SYM_FUNCS_OPTIONAL(what)

    #define wxDECLARE_SYM_FUNCTION(func, name) static func ## _t func

    wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PUBLIC(wxDECLARE_SYM_FUNCTION);

private:
    wxDO_FOR_ALL_SYM_FUNCS_REQUIRED_PRIVATE(wxDECLARE_SYM_FUNCTION);
    wxDO_FOR_ALL_SYM_FUNCS_OPTIONAL(wxDECLARE_SYM_FUNCTION);

public:

    #undef wxDECLARE_SYM_FUNCTION

    // load all functions from DLL, return true if ok
    static bool Init();

    // return the string with the error message explaining why Init() failed
    static const wxString& GetErrorMessage();

    // log error returned by the given function to debug output
    static void LogError(const wxChar *func);

    // return textual representation of the value of given symbol
    static wxString DumpSymbol(wxPSYMBOL_INFO pSymInfo, void *pVariable);

    // return the name of the symbol with given type index
    static wxString GetSymbolName(wxPSYMBOL_INFO pSymInfo);

private:
    // dereference the given symbol, i.e. return symbol which is not a
    // pointer/reference any more
    //
    // if ppData != NULL, dereference the pointer as many times as we
    // dereferenced the symbol
    //
    // return the tag of the dereferenced symbol
    static SymbolTag DereferenceSymbol(wxPSYMBOL_INFO pSymInfo, void **ppData);

    static wxString DumpField(wxPSYMBOL_INFO pSymInfo,
                              void *pVariable,
                              unsigned level);

    static wxString DumpBaseType(BasicType bt, DWORD64 length, void *pVariable);

    static wxString DumpUDT(wxPSYMBOL_INFO pSymInfo,
                            void *pVariable,
                            unsigned level = 0);

    static bool BindDbgHelpFunctions(const wxDynamicLibrary& dllDbgHelp);
    static bool DoInit();
};

#endif // wxUSE_DBGHELP

#endif // _WX_MSW_DEBUGHLPH_H_

