/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dir.cpp
// Purpose:     wxDir implementation for Win32
// Author:      Vadim Zeitlin
// Modified by:
// Created:     08.12.99
// RCS-ID:      $Id: dir.cpp 67280 2011-03-22 14:17:38Z DS $
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif // PCH

#include "wx/dir.h"

#ifdef __WINDOWS__
    #include "wx/msw/private.h"
#endif

// ----------------------------------------------------------------------------
// define the types and functions used for file searching
// ----------------------------------------------------------------------------

namespace
{

typedef WIN32_FIND_DATA FIND_STRUCT;
typedef HANDLE FIND_DATA;
typedef DWORD FIND_ATTR;

inline FIND_DATA InitFindData()
{
    return INVALID_HANDLE_VALUE;
}

inline bool IsFindDataOk(FIND_DATA fd)
{
        return fd != INVALID_HANDLE_VALUE;
}

inline void FreeFindData(FIND_DATA fd)
{
    if ( !::FindClose(fd) )
    {
        wxLogLastError(wxT("FindClose"));
    }
}

inline FIND_DATA FindFirst(const wxString& spec,
                           FIND_STRUCT *finddata)
{
    return ::FindFirstFile(spec.t_str(), finddata);
}

inline bool FindNext(FIND_DATA fd, FIND_STRUCT *finddata)
{
    return ::FindNextFile(fd, finddata) != 0;
}

const wxChar *GetNameFromFindData(FIND_STRUCT *finddata)
{
    return finddata->cFileName;
}

inline FIND_ATTR GetAttrFromFindData(FIND_STRUCT *finddata)
{
    return finddata->dwFileAttributes;
}

inline bool IsDir(FIND_ATTR attr)
{
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline bool IsHidden(FIND_ATTR attr)
{
    return (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef MAX_PATH
    #define MAX_PATH 260        // from VC++ headers
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

#define M_DIR       ((wxDirData *)m_data)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// this class stores everything we need to enumerate the files
class wxDirData
{
public:
    wxDirData(const wxString& dirname);
    ~wxDirData();

    void SetFileSpec(const wxString& filespec) { m_filespec = filespec; }
    void SetFlags(int flags) { m_flags = flags; }

    void Close();
    void Rewind();
    bool Read(wxString *filename);

    const wxString& GetName() const { return m_dirname; }

private:
    FIND_DATA m_finddata;

    wxString m_dirname;
    wxString m_filespec;

    int      m_flags;

    wxDECLARE_NO_COPY_CLASS(wxDirData);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirData
// ----------------------------------------------------------------------------

wxDirData::wxDirData(const wxString& dirname)
         : m_dirname(dirname)
{
    m_finddata = InitFindData();
}

wxDirData::~wxDirData()
{
    Close();
}

void wxDirData::Close()
{
    if ( IsFindDataOk(m_finddata) )
    {
        FreeFindData(m_finddata);

        m_finddata = InitFindData();
    }
}

void wxDirData::Rewind()
{
    Close();
}

bool wxDirData::Read(wxString *filename)
{
    bool first = false;

    WIN32_FIND_DATA finddata;
    #define PTR_TO_FINDDATA (&finddata)

    if ( !IsFindDataOk(m_finddata) )
    {
        // open first
        wxString filespec = m_dirname;
        if ( !wxEndsWithPathSeparator(filespec) )
        {
            filespec += wxT('\\');
        }
        if ( !m_filespec )
            filespec += wxT("*.*");
        else
            filespec += m_filespec;

        m_finddata = FindFirst(filespec, PTR_TO_FINDDATA);

        first = true;
    }

    if ( !IsFindDataOk(m_finddata) )
    {
#ifdef __WIN32__
        DWORD err = ::GetLastError();

        if ( err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES )
        {
            wxLogSysError(err, _("Cannot enumerate files in directory '%s'"),
                          m_dirname.c_str());
        }
#endif // __WIN32__
        //else: not an error, just no (such) files

        return false;
    }

    const wxChar *name;
    FIND_ATTR attr;

    for ( ;; )
    {
        if ( first )
        {
            first = false;
        }
        else
        {
            if ( !FindNext(m_finddata, PTR_TO_FINDDATA) )
            {
#ifdef __WIN32__
                DWORD err = ::GetLastError();

                if ( err != ERROR_NO_MORE_FILES )
                {
                    wxLogLastError(wxT("FindNext"));
                }
#endif // __WIN32__
                //else: not an error, just no more (such) files

                return false;
            }
        }

        name = GetNameFromFindData(PTR_TO_FINDDATA);
        attr = GetAttrFromFindData(PTR_TO_FINDDATA);

        // don't return "." and ".." unless asked for
        if ( name[0] == wxT('.') &&
             ((name[1] == wxT('.') && name[2] == wxT('\0')) ||
              (name[1] == wxT('\0'))) )
        {
            if ( !(m_flags & wxDIR_DOTDOT) )
                continue;
        }

        // check the type now
        if ( !(m_flags & wxDIR_FILES) && !IsDir(attr) )
        {
            // it's a file, but we don't want them
            continue;
        }
        else if ( !(m_flags & wxDIR_DIRS) && IsDir(attr) )
        {
            // it's a dir, and we don't want it
            continue;
        }

        // finally, check whether it's a hidden file
        if ( !(m_flags & wxDIR_HIDDEN) )
        {
            if ( IsHidden(attr) )
            {
                // it's a hidden file, skip it
                continue;
            }
        }

        *filename = name;

        break;
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxDir construction/destruction
// ----------------------------------------------------------------------------

wxDir::wxDir(const wxString& dirname)
{
    m_data = NULL;

    (void)Open(dirname);
}

bool wxDir::Open(const wxString& dirname)
{
    delete M_DIR;

    // The Unix code does a similar test
    if (wxDirExists(dirname))
    {
        m_data = new wxDirData(dirname);

        return true;
    }
    else
    {
        m_data = NULL;

        return false;
    }
}

bool wxDir::IsOpened() const
{
    return m_data != NULL;
}

wxString wxDir::GetName() const
{
    wxString name;
    if ( m_data )
    {
        name = M_DIR->GetName();
        if ( !name.empty() )
        {
            // bring to canonical Windows form
            name.Replace(wxT("/"), wxT("\\"));

            if ( name.Last() == wxT('\\') )
            {
                // chop off the last (back)slash
                name.Truncate(name.length() - 1);
            }
        }
    }

    return name;
}

wxDir::~wxDir()
{
    delete M_DIR;
}

// ----------------------------------------------------------------------------
// wxDir enumerating
// ----------------------------------------------------------------------------

bool wxDir::GetFirst(wxString *filename,
                     const wxString& filespec,
                     int flags) const
{
    wxCHECK_MSG( IsOpened(), false, wxT("must wxDir::Open() first") );

    M_DIR->Rewind();

    M_DIR->SetFileSpec(filespec);
    M_DIR->SetFlags(flags);

    return GetNext(filename);
}

bool wxDir::GetNext(wxString *filename) const
{
    wxCHECK_MSG( IsOpened(), false, wxT("must wxDir::Open() first") );

    wxCHECK_MSG( filename, false, wxT("bad pointer in wxDir::GetNext()") );

    return M_DIR->Read(filename);
}

// ----------------------------------------------------------------------------
// wxGetDirectoryTimes: used by wxFileName::GetTimes()
// ----------------------------------------------------------------------------

#ifdef __WIN32__

extern bool
wxGetDirectoryTimes(const wxString& dirname,
                    FILETIME *ftAccess, FILETIME *ftCreate, FILETIME *ftMod)
{
#ifdef __WXWINCE__
    // FindFirst() is going to fail
    wxASSERT_MSG( !dirname.empty(),
                  wxT("incorrect directory name format in wxGetDirectoryTimes") );
#else
    // FindFirst() is going to fail
    wxASSERT_MSG( !dirname.empty() && dirname.Last() != wxT('\\'),
                  wxT("incorrect directory name format in wxGetDirectoryTimes") );
#endif

    FIND_STRUCT fs;
    FIND_DATA fd = FindFirst(dirname, &fs);
    if ( !IsFindDataOk(fd) )
    {
        return false;
    }

    *ftAccess = fs.ftLastAccessTime;
    *ftCreate = fs.ftCreationTime;
    *ftMod = fs.ftLastWriteTime;

    FindClose(fd);

    return true;
}

#endif // __WIN32__

