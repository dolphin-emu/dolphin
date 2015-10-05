/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/stackwalk.cpp
// Purpose:     wxStackWalker implementation for Unix/glibc
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-18
// Copyright:   (c) 2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STACKWALKER

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/utils.h"
#endif

#include "wx/stackwalk.h"
#include "wx/stdpaths.h"

#include <execinfo.h>

#ifdef HAVE_CXA_DEMANGLE
    #include <cxxabi.h>
#endif // HAVE_CXA_DEMANGLE

// ----------------------------------------------------------------------------
// tiny helper wrapper around popen/pclose()
// ----------------------------------------------------------------------------

class wxStdioPipe
{
public:
    // ctor parameters are passed to popen()
    wxStdioPipe(const char *command, const char *type)
    {
        m_fp = popen(command, type);
    }

    // conversion to stdio FILE
    operator FILE *() const { return m_fp; }

    // dtor closes the pipe
    ~wxStdioPipe()
    {
        if ( m_fp )
            pclose(m_fp);
    }

private:
    FILE *m_fp;

    wxDECLARE_NO_COPY_CLASS(wxStdioPipe);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxStackFrame
// ----------------------------------------------------------------------------

void wxStackFrame::OnGetName()
{
    if ( !m_name.empty() )
        return;

    // we already tried addr2line in wxStackWalker::InitFrames: it always
    // gives us demangled names (even if __cxa_demangle is not available) when
    // the function is part of the ELF (when it's in a shared object addr2line
    // will give "??") and because it seems less error-prone.
    // when it works, backtrace_symbols() sometimes returns incorrect results

    // format is: "module(funcname+offset) [address]" but the part in
    // parentheses can be not present
    wxString syminfo = wxString::FromAscii(m_syminfo);
    const size_t posOpen = syminfo.find(wxT('('));
    if ( posOpen != wxString::npos )
    {
        const size_t posPlus = syminfo.find(wxT('+'), posOpen + 1);
        if ( posPlus != wxString::npos )
        {
            const size_t posClose = syminfo.find(wxT(')'), posPlus + 1);
            if ( posClose != wxString::npos )
            {
                if ( m_name.empty() )
                {
                    m_name.assign(syminfo, posOpen + 1, posPlus - posOpen - 1);

#ifdef HAVE_CXA_DEMANGLE
                    int rc = -1;
                    char *cppfunc = __cxxabiv1::__cxa_demangle
                                    (
                                        m_name.mb_str(),
                                        NULL, // output buffer (none, alloc it)
                                        NULL, // [out] len of output buffer
                                        &rc
                                    );
                    if ( rc == 0 )
                        m_name = wxString::FromAscii(cppfunc);

                    free(cppfunc);
#endif // HAVE_CXA_DEMANGLE
                }

                unsigned long ofs;
                if ( wxString(syminfo, posPlus + 1, posClose - posPlus - 1).
                        ToULong(&ofs, 0) )
                    m_offset = ofs;
            }
        }

        m_module.assign(syminfo, posOpen);
    }
#ifndef __WXOSX__
    else // not in "module(funcname+offset)" format
    {
        m_module = syminfo;
    }
#endif // !__WXOSX__
}


// ----------------------------------------------------------------------------
// wxStackWalker
// ----------------------------------------------------------------------------

// that many frames should be enough for everyone
#define MAX_FRAMES          200

// we need a char buffer big enough to contain a call to addr2line with
// up to MAX_FRAMES addresses !
// NB: %p specifier will print the pointer in hexadecimal form
//     and thus will require 2 chars for each byte + 3 for the
//     " 0x" prefix
#define CHARS_PER_FRAME    (sizeof(void*) * 2 + 3)

// BUFSIZE will be 2250 for 32 bit machines
#define BUFSIZE            (50 + MAX_FRAMES*CHARS_PER_FRAME)

// static data
void *wxStackWalker::ms_addresses[MAX_FRAMES];
char **wxStackWalker::ms_symbols = NULL;
int wxStackWalker::m_depth = 0;
wxString wxStackWalker::ms_exepath;
static char g_buf[BUFSIZE];


void wxStackWalker::SaveStack(size_t maxDepth)
{
    // read all frames required
    maxDepth = wxMin(WXSIZEOF(ms_addresses)/sizeof(void*), maxDepth);
    m_depth = backtrace(ms_addresses, maxDepth*sizeof(void*));
    if ( !m_depth )
        return;

    ms_symbols = backtrace_symbols(ms_addresses, m_depth);
}

void wxStackWalker::ProcessFrames(size_t skip)
{
    wxStackFrame frames[MAX_FRAMES];

    if (!ms_symbols || !m_depth)
        return;

    // we are another level down from Walk(), so adjust the number of stack
    // frames to skip accordingly
    skip += 1;

    // call addr2line only once since this call may be very slow
    // (it has to load in memory the entire EXE of this app which may be quite
    //  big, especially if it contains debug info and is compiled statically!)
    int numFrames = InitFrames(frames, m_depth - skip,
                               &ms_addresses[skip], &ms_symbols[skip]);

    // now do user-defined operations on each frame
    for ( int n = 0; n < numFrames; n++ )
        OnStackFrame(frames[n]);
}

void wxStackWalker::FreeStack()
{
    // ms_symbols has been allocated by backtrace_symbols() and it's the responsibility
    // of the caller, i.e. us, to free that pointer
    if (ms_symbols)
        free( ms_symbols );
    ms_symbols = NULL;
    m_depth = 0;
}

namespace
{

// Helper function to read a line from the file and return it without the
// trailing newline. Line number is only used for error reporting.
bool ReadLine(FILE* fp, unsigned long num, wxString* line)
{
    if ( !fgets(g_buf, WXSIZEOF(g_buf), fp) )
    {
        wxUnusedVar(num); // could be unused if debug tracing is disabled

        wxLogDebug(wxS("cannot read address information for stack frame #%lu"),
                   num);
        return false;
    }

    *line = wxString::FromAscii(g_buf);
    line->RemoveLast();

    return true;
}

} // anonymous namespace

int wxStackWalker::InitFrames(wxStackFrame *arr, size_t n, void **addresses, char **syminfo)
{
    // we need to launch addr2line tool to get this information and we need to
    // have the program name for this
    wxString exepath = wxStackWalker::GetExePath();
    if ( exepath.empty() )
    {
        exepath = wxStandardPaths::Get().GetExecutablePath();
        if ( exepath.empty() )
        {
            wxLogDebug(wxT("Cannot parse stack frame because the executable ")
                       wxT("path could not be detected"));
            return 0;
        }
    }

    // build the command line for executing addr2line or atos under OS X using
    // char* directly to avoid the conversions from Unicode
#ifdef __WXOSX__
    int len = snprintf(g_buf, BUFSIZE, "atos -p %d", (int)getpid());
#else
    int len = snprintf(g_buf, BUFSIZE, "addr2line -C -f -e \"%s\"", (const char*) exepath.mb_str());
#endif
    len = (len <= 0) ? strlen(g_buf) : len;     // in case snprintf() is broken
    for (size_t i=0; i<n; i++)
    {
        snprintf(&g_buf[len], BUFSIZE - len, " %p", addresses[i]);
        len = strlen(g_buf);
    }

    //wxLogDebug(wxT("piping the command '%s'"), g_buf);  // for debug only

    wxStdioPipe fp(g_buf, "r");
    if ( !fp )
        return 0;

    // parse the output reusing the same buffer to avoid any big memory
    // allocations which could fail if our program is in a bad state
    wxString name, filename;
    unsigned long line = 0,
                  curr = 0;
    for  ( size_t i = 0; i < n; i++ )
    {
#ifdef __WXOSX__
        wxString buffer;
        if ( !ReadLine(fp, i, &buffer) )
            return false;

        line = 0;
        filename.clear();

        // We can get back either the string in the following format:
        //
        //      func(args) (in module) (file:line)
        //
        // or just the same address back if it couldn't be resolved.
        const size_t posIn = buffer.find("(in ");
        if ( posIn != wxString::npos )
        {
            name.assign(buffer, 0, posIn);

            size_t posAt = buffer.find(") (", posIn + 3);
            if ( posAt != wxString::npos )
            {
                posAt += 3; // Skip ") ("

                // Discard the two last characters which are ")\n"
                wxString location(buffer, posAt, buffer.length() - posAt - 2);

                wxString linenum;
                filename = location.BeforeFirst(':', &linenum);
                if ( !linenum.empty() )
                    linenum.ToULong(&line);
            }
        }
#else // !__WXOSX__
        // 1st line has function name
        if ( !ReadLine(fp, i, &name) )
            return false;

        name = wxString::FromAscii(g_buf);
        name.RemoveLast(); // trailing newline

        if ( name == wxT("??") )
            name.clear();

        // 2nd one -- the file/line info
        if ( !ReadLine(fp, i, &filename) )
            return false;

        const size_t posColon = filename.find(wxT(':'));
        if ( posColon != wxString::npos )
        {
            // parse line number (it's ok if it fails, this will just leave
            // line at its current, invalid, 0 value)
            wxString(filename, posColon + 1, wxString::npos).ToULong(&line);

            // remove line number from 'filename'
            filename.erase(posColon);
            if ( filename == wxT("??") )
                filename.clear();
        }
        else
        {
            wxLogDebug(wxT("Unexpected addr2line format: \"%s\" - ")
                       wxT("the semicolon is missing"),
                       filename.c_str());
        }
#endif // __WXOSX__/!__WXOSX__

        // now we've got enough info to initialize curr-th stack frame
        // (at worst, only addresses[i] and syminfo[i] have been initialized,
        //  but wxStackFrame::OnGetName may still be able to get function name):
        arr[curr++].Set(name, filename, syminfo[i], i, line, addresses[i]);
    }

    return curr;
}

void wxStackWalker::Walk(size_t skip, size_t maxDepth)
{
    // read all frames required
    SaveStack(maxDepth);

    // process them
    ProcessFrames(skip);

    // cleanup
    FreeStack();
}

#endif // wxUSE_STACKWALKER
