/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/mimetype.cpp
// Purpose:     classes and functions to manage MIME types
// Author:      Vadim Zeitlin
// Modified by:
// Created:     23.09.98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence (part of wxExtra library)
/////////////////////////////////////////////////////////////////////////////

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MIMETYPE && wxUSE_FILE

#include "wx/unix/mimetype.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
#endif

#include "wx/file.h"
#include "wx/confbase.h"

#include "wx/ffile.h"
#include "wx/dir.h"
#include "wx/tokenzr.h"
#include "wx/iconloc.h"
#include "wx/filename.h"
#include "wx/app.h"
#include "wx/apptrait.h"

// other standard headers
#include <ctype.h>

class wxMimeTextFile
{
public:
    wxMimeTextFile()
    {
    }

    wxMimeTextFile(const wxString& fname)
    {
       m_fname = fname;
    }

    bool Open()
    {
       wxFFile file( m_fname );
       if (!file.IsOpened())
          return false;

       size_t size = file.Length();
       wxCharBuffer buffer( size );
       file.Read( (void*) (const char*) buffer, size );

       // Check for valid UTF-8 here?
       wxString all = wxString::FromUTF8( buffer, size );

       wxStringTokenizer tok( all, "\n" );
       while (tok.HasMoreTokens())
       {
          wxString t = tok.GetNextToken();
          t.MakeLower();
          if ((!!t) && (t.Find( "comment" ) != 0) && (t.Find( "#" ) != 0) && (t.Find( "generic" ) != 0))
             m_text.Add( t );
       }
       return true;
    }

    unsigned int GetLineCount() const { return m_text.GetCount(); }
    wxString &GetLine( unsigned int line ) { return m_text[line]; }

    int pIndexOf(const wxString& sSearch,
                 bool bIncludeComments = false,
                 int iStart = 0)
    {
        wxString sTest = sSearch;
        sTest.MakeLower();
        for(size_t i = iStart; i < GetLineCount(); i++)
        {
            wxString sLine = GetLine(i);
            if(bIncludeComments || ! sLine.StartsWith(wxT("#")))
            {
                if(sLine.StartsWith(sTest))
                    return (int)i;
            }
        }
        return wxNOT_FOUND;
    }

    wxString GetVerb(size_t i)
    {
        if (i > GetLineCount() )
            return wxEmptyString;

        wxString sTmp = GetLine(i).BeforeFirst(wxT('='));
        return sTmp;
    }

    wxString GetCmd(size_t i)
    {
        if (i > GetLineCount() )
            return wxEmptyString;

        wxString sTmp = GetLine(i).AfterFirst(wxT('='));
        return sTmp;
    }

private:
    wxArrayString m_text;
    wxString m_fname;
};

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// MIME code tracing mask
#define TRACE_MIME wxT("mime")


// Read a XDG *.desktop file of type 'Application'
void wxMimeTypesManagerImpl::LoadXDGApp(const wxString& filename)
{
    wxLogTrace(TRACE_MIME, wxT("loading XDG file %s"), filename.c_str());

    wxMimeTextFile file(filename);
    if ( !file.Open() )
        return;

    // Here, only type 'Application' should be considered.
    int nIndex = file.pIndexOf( "Type=" );
    if (nIndex != wxNOT_FOUND && file.GetCmd(nIndex) != "application")
        return;

    // The hidden entry specifies a file to be ignored.
    nIndex = file.pIndexOf( "Hidden=" );
    if (nIndex != wxNOT_FOUND && file.GetCmd(nIndex) == "true")
        return;

    // Semicolon separated list of mime types handled by the application.
    nIndex = file.pIndexOf( wxT("MimeType=") );
    if (nIndex == wxNOT_FOUND)
        return;
    wxString mimetypes = file.GetCmd (nIndex);

    // Name of the application
    wxString nameapp;
    nIndex = wxNOT_FOUND;
#if wxUSE_INTL // try "Name[locale name]" first
    wxLocale *locale = wxGetLocale();
    if ( locale )
        nIndex = file.pIndexOf(wxT("Name[")+locale->GetName()+wxT("]="));
#endif // wxUSE_INTL
    if(nIndex == wxNOT_FOUND)
        nIndex = file.pIndexOf( wxT("Name=") );
    if(nIndex != wxNOT_FOUND)
        nameapp = file.GetCmd(nIndex);

    // Icon of the application.
    wxString nameicon, namemini;
    nIndex = wxNOT_FOUND;
#if wxUSE_INTL // try "Icon[locale name]" first
    if ( locale )
        nIndex = file.pIndexOf(wxT("Icon[")+locale->GetName()+wxT("]="));
#endif // wxUSE_INTL
    if(nIndex == wxNOT_FOUND)
        nIndex = file.pIndexOf( wxT("Icon=") );
    if(nIndex != wxNOT_FOUND) {
        nameicon = wxString(wxT("--icon ")) + file.GetCmd(nIndex);
        namemini = wxString(wxT("--miniicon ")) + file.GetCmd(nIndex);
    }

    // Replace some of the field code in the 'Exec' entry.
    // TODO: deal with %d, %D, %n, %N, %k and %v (but last one is deprecated)
    nIndex = file.pIndexOf( wxT("Exec=") );
    if (nIndex == wxNOT_FOUND)
        return;
    wxString sCmd = file.GetCmd(nIndex);
    // we expect %f; others including  %F and %U and %u are possible
    sCmd.Replace(wxT("%F"), wxT("%f"));
    sCmd.Replace(wxT("%U"), wxT("%f"));
    sCmd.Replace(wxT("%u"), wxT("%f"));
    if (0 == sCmd.Replace ( wxT("%f"), wxT("%s") ))
        sCmd = sCmd + wxT(" %s");
    sCmd.Replace(wxT("%c"), nameapp);
    sCmd.Replace(wxT("%i"), nameicon);
    sCmd.Replace(wxT("%m"), namemini);

    wxStringTokenizer tokenizer(mimetypes, wxT(";"));
    while(tokenizer.HasMoreTokens()) {
        wxString mimetype = tokenizer.GetNextToken().Lower();
        nIndex = m_aTypes.Index(mimetype);
        if(nIndex != wxNOT_FOUND) { // is this a known MIME type?
            wxMimeTypeCommands* entry = m_aEntries[nIndex];
            entry->AddOrReplaceVerb(wxT("open"), sCmd);
        }
    }
}

void wxMimeTypesManagerImpl::LoadXDGAppsFilesFromDir(const wxString& dirname)
{
    // Don't complain if we don't have permissions to read - it confuses users
    wxLogNull logNull;

    if(! wxDir::Exists(dirname))
        return;
    wxDir dir(dirname);
    if ( !dir.IsOpened() )
        return;

    wxString filename;
    // Look into .desktop files
    bool cont = dir.GetFirst(&filename, wxT("*.desktop"), wxDIR_FILES);
    while (cont)
    {
        wxFileName p(dirname, filename);
        LoadXDGApp( p.GetFullPath() );
        cont = dir.GetNext(&filename);
    }

    // Recurse into subdirs, which on KDE may hold most of the .desktop files
    cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
    while (cont)
    {
        wxFileName p(dirname, wxEmptyString);
        p.AppendDir(filename);
        LoadXDGAppsFilesFromDir( p.GetPath() );
        cont = dir.GetNext(&filename);
    }
}


void wxMimeTypesManagerImpl::LoadXDGGlobs(const wxString& filename)
{
    if ( !wxFileName::FileExists(filename) )
        return;

    wxLogTrace(TRACE_MIME, wxT("loading XDG globs file from %s"), filename.c_str());

    wxMimeTextFile file(filename);
    if ( !file.Open() )
        return;

    size_t i;
    for (i = 0; i < file.GetLineCount(); i++)
    {
       wxStringTokenizer tok( file.GetLine(i), ":" );
       wxString mime = tok.GetNextToken();
       wxString ext = tok.GetNextToken();
       ext.Remove( 0, 2 );
       wxArrayString exts;
       exts.Add( ext );

       AddToMimeData(mime, wxEmptyString, NULL, exts, wxEmptyString, true );
    }
}

// ----------------------------------------------------------------------------
// wxFileTypeImpl (Unix)
// ----------------------------------------------------------------------------

wxString wxFileTypeImpl::GetExpandedCommand(const wxString & verb, const wxFileType::MessageParameters& params) const
{
    wxString sTmp;
    size_t i = 0;
    while ( (i < m_index.GetCount() ) && sTmp.empty() )
    {
        sTmp = m_manager->GetCommand( verb, m_index[i] );
        i++;
    }

    return wxFileType::ExpandCommand(sTmp, params);
}

bool wxFileTypeImpl::GetIcon(wxIconLocation *iconLoc) const
{
    wxString sTmp;
    size_t i = 0;
    while ( (i < m_index.GetCount() ) && sTmp.empty() )
    {
        sTmp = m_manager->m_aIcons[m_index[i]];
        i++;
    }

    if ( sTmp.empty() )
        return false;

    if ( iconLoc )
    {
        iconLoc->SetFileName(sTmp);
    }

    return true;
}

bool wxFileTypeImpl::GetMimeTypes(wxArrayString& mimeTypes) const
{
    mimeTypes.Clear();
    size_t nCount = m_index.GetCount();
    for (size_t i = 0; i < nCount; i++)
        mimeTypes.Add(m_manager->m_aTypes[m_index[i]]);

    return true;
}

size_t wxFileTypeImpl::GetAllCommands(wxArrayString *verbs,
                                  wxArrayString *commands,
                                  const wxFileType::MessageParameters& params) const
{
    wxString vrb, cmd, sTmp;
    size_t count = 0;
    wxMimeTypeCommands * sPairs;

    // verbs and commands have been cleared already in mimecmn.cpp...
    // if we find no entries in the exact match, try the inexact match
    for (size_t n = 0; ((count == 0) && (n < m_index.GetCount())); n++)
    {
        // list of verb = command pairs for this mimetype
        sPairs = m_manager->m_aEntries [m_index[n]];
        size_t i;
        for ( i = 0; i < sPairs->GetCount(); i++ )
        {
            vrb = sPairs->GetVerb(i);
            // some gnome entries have "." inside
            vrb = vrb.AfterLast(wxT('.'));
            cmd = sPairs->GetCmd(i);
            if (! cmd.empty() )
            {
                 cmd = wxFileType::ExpandCommand(cmd, params);
                 count++;
                 if ( vrb.IsSameAs(wxT("open")))
                 {
                     if ( verbs )
                        verbs->Insert(vrb, 0u);
                     if ( commands )
                        commands ->Insert(cmd, 0u);
                 }
                 else
                 {
                     if ( verbs )
                        verbs->Add(vrb);
                     if ( commands )
                        commands->Add(cmd);
                 }
             }
        }
    }

    return count;
}

bool wxFileTypeImpl::GetExtensions(wxArrayString& extensions)
{
    const wxString strExtensions = m_manager->GetExtension(m_index[0]);
    extensions.Empty();

    // one extension in the space or comma-delimited list
    wxString strExt;
    wxString::const_iterator end = strExtensions.end();
    for ( wxString::const_iterator p = strExtensions.begin(); /* nothing */; ++p )
    {
        if ( p == end || *p == wxT(' ') || *p == wxT(',') )
        {
            if ( !strExt.empty() )
            {
                extensions.Add(strExt);
                strExt.Empty();
            }
            //else: repeated spaces
            // (shouldn't happen, but it's not that important if it does happen)

            if ( p == end )
                break;
        }
        else if ( *p == wxT('.') )
        {
            // remove the dot from extension (but only if it's the first char)
            if ( !strExt.empty() )
            {
                strExt += wxT('.');
            }
            //else: no, don't append it
        }
        else
        {
            strExt += *p;
        }
    }

    return true;
}

// set an arbitrary command:
// could adjust the code to ask confirmation if it already exists and
// overwriteprompt is true, but this is currently ignored as *Associate* has
// no overwrite prompt
bool
wxFileTypeImpl::SetCommand(const wxString& cmd,
                           const wxString& verb,
                           bool WXUNUSED(overwriteprompt))
{
    wxArrayString strExtensions;
    wxString strDesc, strIcon;

    wxArrayString strTypes;
    GetMimeTypes(strTypes);
    if ( strTypes.IsEmpty() )
        return false;

    wxMimeTypeCommands *entry = new wxMimeTypeCommands();
    entry->Add(verb + wxT("=")  + cmd + wxT(" %s "));

    bool ok = false;
    size_t nCount = strTypes.GetCount();
    for ( size_t i = 0; i < nCount; i++ )
    {
        if ( m_manager->DoAssociation
                        (
                            strTypes[i],
                            strIcon,
                            entry,
                            strExtensions,
                            strDesc
                        ) )
        {
            // DoAssociation() took ownership of entry, don't delete it below
            ok = true;
        }
    }

    if ( !ok )
        delete entry;

    return ok;
}

// ignore index on the grounds that we only have one icon in a Unix file
bool wxFileTypeImpl::SetDefaultIcon(const wxString& strIcon, int WXUNUSED(index))
{
    if (strIcon.empty())
        return false;

    wxArrayString strExtensions;
    wxString strDesc;

    wxArrayString strTypes;
    GetMimeTypes(strTypes);
    if ( strTypes.IsEmpty() )
        return false;

    wxMimeTypeCommands *entry = new wxMimeTypeCommands();
    bool ok = false;
    size_t nCount = strTypes.GetCount();
    for ( size_t i = 0; i < nCount; i++ )
    {
        if ( m_manager->DoAssociation
                        (
                            strTypes[i],
                            strIcon,
                            entry,
                            strExtensions,
                            strDesc
                        ) )
        {
            // we don't need to free entry now, DoAssociation() took ownership
            // of it
            ok = true;
        }
    }

    if ( !ok )
        delete entry;

    return ok;
}

// ----------------------------------------------------------------------------
// wxMimeTypesManagerImpl (Unix)
// ----------------------------------------------------------------------------

wxMimeTypesManagerImpl::wxMimeTypesManagerImpl()
{
    m_initialized = false;
}

void wxMimeTypesManagerImpl::InitIfNeeded()
{
    if ( !m_initialized )
    {
        // set the flag first to prevent recursion
        m_initialized = true;

        int mailcapStyles = wxMAILCAP_ALL;
        if ( wxAppTraits * const traits = wxApp::GetTraitsIfExists() )
        {
            wxString wm = traits->GetDesktopEnvironment();

            if ( wm == "KDE" )
                mailcapStyles = wxMAILCAP_KDE;
            else if ( wm == "GNOME" )
                mailcapStyles = wxMAILCAP_GNOME;
            //else: unknown, use the default
        }

        Initialize(mailcapStyles);
    }
}



// read system and user mailcaps and other files
void wxMimeTypesManagerImpl::Initialize(int mailcapStyles,
                                        const wxString& sExtraDir)
{
#ifdef __VMS
    // XDG tables are never installed on OpenVMS
    return;
#else

    // Read MIME type - extension associations
    LoadXDGGlobs( "/usr/share/mime/globs" );
    LoadXDGGlobs( "/usr/local/share/mime/globs" );

    // Load desktop files for XDG, and then override them with the defaults.
    // We will override them one desktop file at a time, rather
    // than one mime type at a time, but it should be a reasonable
    // heuristic.
    {
        wxString xdgDataHome = wxGetenv("XDG_DATA_HOME");
        if ( xdgDataHome.empty() )
            xdgDataHome = wxGetHomeDir() + "/.local/share";
        wxString xdgDataDirs = wxGetenv("XDG_DATA_DIRS");
        if ( xdgDataDirs.empty() )
        {
            xdgDataDirs = "/usr/local/share:/usr/share";
            if (mailcapStyles & wxMAILCAP_GNOME)
                xdgDataDirs += ":/usr/share/gnome:/opt/gnome/share";
            if (mailcapStyles & wxMAILCAP_KDE)
                xdgDataDirs += ":/usr/share/kde3:/opt/kde3/share";
        }
        if ( !sExtraDir.empty() )
        {
           xdgDataDirs += ':';
           xdgDataDirs += sExtraDir;
        }

        wxArrayString dirs;
        wxStringTokenizer tokenizer(xdgDataDirs, ":");
        while ( tokenizer.HasMoreTokens() )
        {
            wxString p = tokenizer.GetNextToken();
            dirs.Add(p);
        }
        dirs.insert(dirs.begin(), xdgDataHome);

        wxString defaultsList;
        size_t i;
        for (i = 0; i < dirs.GetCount(); i++)
        {
            wxString f = dirs[i];
            if (f.Last() != '/') f += '/';
            f += "applications/defaults.list";
            if (wxFileExists(f))
            {
                defaultsList = f;
                break;
            }
        }

        // Load application files and associate them to corresponding mime types.
        size_t nDirs = dirs.GetCount();
        for (size_t nDir = 0; nDir < nDirs; nDir++)
        {
            wxString dirStr = dirs[nDir];
            if (dirStr.Last() != '/') dirStr += '/';
            dirStr += "applications";
            LoadXDGAppsFilesFromDir(dirStr);
        }

        if (!defaultsList.IsEmpty())
        {
            wxArrayString deskTopFilesSeen;

            wxMimeTextFile textfile(defaultsList);
            if ( textfile.Open() )
            {
                int nIndex = textfile.pIndexOf( wxT("[Default Applications]") );
                if (nIndex != wxNOT_FOUND)
                {
                    for (i = nIndex+1; i < textfile.GetLineCount(); i++)
                    {
                        if (textfile.GetLine(i).Find(wxT("=")) != wxNOT_FOUND)
                        {
                            wxString desktopFile = textfile.GetCmd(i);

                            if (deskTopFilesSeen.Index(desktopFile) == wxNOT_FOUND)
                            {
                                deskTopFilesSeen.Add(desktopFile);
                                size_t j;
                                for (j = 0; j < dirs.GetCount(); j++)
                                {
                                    wxString desktopPath = dirs[j];
                                    if (desktopPath.Last() != '/') desktopPath += '/';
                                    desktopPath += "applications/";
                                    desktopPath += desktopFile;

                                    if (wxFileExists(desktopPath))
                                        LoadXDGApp(desktopPath);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#endif
}

// clear data so you can read another group of WM files
void wxMimeTypesManagerImpl::ClearData()
{
    m_aTypes.Clear();
    m_aIcons.Clear();
    m_aExtensions.Clear();
    m_aDescriptions.Clear();

    WX_CLEAR_ARRAY(m_aEntries);
    m_aEntries.Empty();
}

wxMimeTypesManagerImpl::~wxMimeTypesManagerImpl()
{
    ClearData();
}

wxFileType * wxMimeTypesManagerImpl::Associate(const wxFileTypeInfo& ftInfo)
{
    InitIfNeeded();

    wxString strType = ftInfo.GetMimeType();
    wxString strDesc = ftInfo.GetDescription();
    wxString strIcon = ftInfo.GetIconFile();

    wxMimeTypeCommands *entry = new wxMimeTypeCommands();

    if ( ! ftInfo.GetOpenCommand().empty())
        entry->Add(wxT("open=")  + ftInfo.GetOpenCommand() + wxT(" %s "));
    if ( ! ftInfo.GetPrintCommand().empty())
        entry->Add(wxT("print=") + ftInfo.GetPrintCommand() + wxT(" %s "));

    // now find where these extensions are in the data store and remove them
    wxArrayString sA_Exts = ftInfo.GetExtensions();
    wxString sExt, sExtStore;
    size_t i, nIndex;
    size_t nExtCount = sA_Exts.GetCount();
    for (i=0; i < nExtCount; i++)
    {
        sExt = sA_Exts.Item(i);

        // clean up to just a space before and after
        sExt.Trim().Trim(false);
        sExt = wxT(' ') + sExt + wxT(' ');
        size_t nCount = m_aExtensions.GetCount();
        for (nIndex = 0; nIndex < nCount; nIndex++)
        {
            sExtStore = m_aExtensions.Item(nIndex);
            if (sExtStore.Replace(sExt, wxT(" ") ) > 0)
                m_aExtensions.Item(nIndex) = sExtStore;
        }
    }

    if ( !DoAssociation(strType, strIcon, entry, sA_Exts, strDesc) )
        return NULL;

    return GetFileTypeFromMimeType(strType);
}

bool wxMimeTypesManagerImpl::DoAssociation(const wxString& strType,
                                           const wxString& strIcon,
                                           wxMimeTypeCommands *entry,
                                           const wxArrayString& strExtensions,
                                           const wxString& strDesc)
{
    int nIndex = AddToMimeData(strType, strIcon, entry, strExtensions, strDesc, true);

    if ( nIndex == wxNOT_FOUND )
        return false;

    return true;
}

int wxMimeTypesManagerImpl::AddToMimeData(const wxString& strType,
                                          const wxString& strIcon,
                                          wxMimeTypeCommands *entry,
                                          const wxArrayString& strExtensions,
                                          const wxString& strDesc,
                                          bool replaceExisting)
{
    InitIfNeeded();

    // ensure mimetype is always lower case
    wxString mimeType = strType.Lower();

    // is this a known MIME type?
    int nIndex = m_aTypes.Index(mimeType);
    if ( nIndex == wxNOT_FOUND )
    {
        // We put MIME types containing  "application" at the end, so that
        // if the MIME type for the extension "htm" is searched for, it will
        // rather find "text/html" than "application/x-mozilla-bookmarks".
        if (mimeType.Find( "application" ) == 0)
        {
           // new file type
           m_aTypes.Add(mimeType);
           m_aIcons.Add(strIcon);
           m_aEntries.Add(entry ? entry : new wxMimeTypeCommands);

           // change nIndex so we can use it below to add the extensions
           m_aExtensions.Add(wxEmptyString);
           nIndex = m_aExtensions.size() - 1;

           m_aDescriptions.Add(strDesc);
        }
        else
        {
           // new file type
           m_aTypes.Insert(mimeType,0);
           m_aIcons.Insert(strIcon,0);
           m_aEntries.Insert(entry ? entry : new wxMimeTypeCommands,0);

           // change nIndex so we can use it below to add the extensions
           m_aExtensions.Insert(wxEmptyString,0);
           nIndex = 0;

           m_aDescriptions.Insert(strDesc,0);
        }
    }
    else // yes, we already have it
    {
        if ( replaceExisting )
        {
            // if new description change it
            if ( !strDesc.empty())
                m_aDescriptions[nIndex] = strDesc;

            // if new icon change it
            if ( !strIcon.empty())
                m_aIcons[nIndex] = strIcon;

            if ( entry )
            {
                delete m_aEntries[nIndex];
                m_aEntries[nIndex] = entry;
            }
        }
        else // add data we don't already have ...
        {
            // if new description add only if none
            if ( m_aDescriptions[nIndex].empty() )
                m_aDescriptions[nIndex] = strDesc;

            // if new icon and no existing icon
            if ( m_aIcons[nIndex].empty() )
                m_aIcons[nIndex] = strIcon;

            // add any new entries...
            if ( entry )
            {
                wxMimeTypeCommands *entryOld = m_aEntries[nIndex];

                size_t count = entry->GetCount();
                for ( size_t i = 0; i < count; i++ )
                {
                    const wxString& verb = entry->GetVerb(i);
                    if ( !entryOld->HasVerb(verb) )
                    {
                        entryOld->AddOrReplaceVerb(verb, entry->GetCmd(i));
                    }
                }

                // as we don't store it anywhere, it won't be deleted later as
                // usual -- do it immediately instead
                delete entry;
            }
        }
    }

    // always add the extensions to this mimetype
    wxString& exts = m_aExtensions[nIndex];

    // add all extensions we don't have yet
    wxString ext;
    size_t count = strExtensions.GetCount();
    for ( size_t i = 0; i < count; i++ )
    {
        ext = strExtensions[i];
        ext += wxT(' ');

        if ( exts.Find(ext) == wxNOT_FOUND )
        {
            exts += ext;
        }
    }

    // check data integrity
    wxASSERT( m_aTypes.GetCount() == m_aEntries.GetCount() &&
              m_aTypes.GetCount() == m_aExtensions.GetCount() &&
              m_aTypes.GetCount() == m_aIcons.GetCount() &&
              m_aTypes.GetCount() == m_aDescriptions.GetCount() );

    return nIndex;
}

wxFileType * wxMimeTypesManagerImpl::GetFileTypeFromExtension(const wxString& ext)
{
    if (ext.empty() )
        return NULL;

    InitIfNeeded();

    wxFileType* fileTypeFallback = NULL;
    size_t count = m_aExtensions.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        wxStringTokenizer tk(m_aExtensions[n], wxT(' '));

        while ( tk.HasMoreTokens() )
        {
            // consider extensions as not being case-sensitive
            if ( tk.GetNextToken().IsSameAs(ext, false /* no case */) )
            {
                // found
                wxFileType *fileType = new wxFileType;
                fileType->m_impl->Init(this, n);

                // See if this one has a known open-command. If not, keep
                // looking for another one that does, as a file that can't be
                // opened is not very useful, but store this one as a fallback.
                wxString type, desc, open;
                fileType->GetMimeType(&type);
                fileType->GetDescription(&desc);
                wxFileType::MessageParameters params("filename."+ext, type);
                if ( fileType->GetOpenCommand(&open, params) )
                {
                    delete fileTypeFallback;
                    return fileType;
                }
                else
                {
                    // Override the previous fallback, if any, with the new
                    // one: we consider that later entries have priority.
                    delete fileTypeFallback;
                    fileTypeFallback = fileType;
                }
            }
        }
    }

    // If we couldn't find a filetype with a known open-command, return any
    // without one
    return fileTypeFallback;
}

wxFileType * wxMimeTypesManagerImpl::GetFileTypeFromMimeType(const wxString& mimeType)
{
    InitIfNeeded();

    wxFileType * fileType = NULL;
    // mime types are not case-sensitive
    wxString mimetype(mimeType);
    mimetype.MakeLower();

    // first look for an exact match
    int index = m_aTypes.Index(mimetype);

    if ( index != wxNOT_FOUND )
    {
        fileType = new wxFileType;
        fileType->m_impl->Init(this, index);
    }

    // then try to find "text/*" as match for "text/plain" (for example)
    // NB: if mimeType doesn't contain '/' at all, BeforeFirst() will return
    //     the whole string - ok.

    index = wxNOT_FOUND;
    wxString strCategory = mimetype.BeforeFirst(wxT('/'));

    size_t nCount = m_aTypes.GetCount();
    for ( size_t n = 0; n < nCount; n++ )
    {
        if ( (m_aTypes[n].BeforeFirst(wxT('/')) == strCategory ) &&
                m_aTypes[n].AfterFirst(wxT('/')) == wxT("*") )
        {
            index = n;
            break;
        }
    }

    if ( index != wxNOT_FOUND )
    {
       // don't throw away fileType that was already found
        if (!fileType)
            fileType = new wxFileType;
        fileType->m_impl->Init(this, index);
    }

    return fileType;
}

wxString wxMimeTypesManagerImpl::GetCommand(const wxString & verb, size_t nIndex) const
{
    wxString command, testcmd, sV, sTmp;
    sV = verb + wxT("=");

    // list of verb = command pairs for this mimetype
    wxMimeTypeCommands * sPairs = m_aEntries [nIndex];

    size_t i;
    size_t nCount = sPairs->GetCount();
    for ( i = 0; i < nCount; i++ )
    {
        sTmp = sPairs->GetVerbCmd (i);
        if ( sTmp.Contains(sV) )
            command = sTmp.AfterFirst(wxT('='));
    }

    return command;
}

void wxMimeTypesManagerImpl::AddFallback(const wxFileTypeInfo& filetype)
{
    InitIfNeeded();

    wxString extensions;
    const wxArrayString& exts = filetype.GetExtensions();
    size_t nExts = exts.GetCount();
    for ( size_t nExt = 0; nExt < nExts; nExt++ )
    {
        if ( nExt > 0 )
            extensions += wxT(' ');

        extensions += exts[nExt];
    }

    AddMimeTypeInfo(filetype.GetMimeType(),
                    extensions,
                    filetype.GetDescription());
}

void wxMimeTypesManagerImpl::AddMimeTypeInfo(const wxString& strMimeType,
                                             const wxString& strExtensions,
                                             const wxString& strDesc)
{
    // reading mailcap may find image/* , while
    // reading mime.types finds image/gif and no match is made
    // this means all the get functions don't work  fix this
    wxString strIcon;
    wxString sTmp = strExtensions;

    wxArrayString sExts;
    sTmp.Trim().Trim(false);

    while (!sTmp.empty())
    {
        sExts.Add(sTmp.AfterLast(wxT(' ')));
        sTmp = sTmp.BeforeLast(wxT(' '));
    }

    AddToMimeData(strMimeType, strIcon, NULL, sExts, strDesc, true);
}

size_t wxMimeTypesManagerImpl::EnumAllFileTypes(wxArrayString& mimetypes)
{
    InitIfNeeded();

    mimetypes.Empty();

    size_t count = m_aTypes.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        // don't return template types from here (i.e. anything containg '*')
        const wxString &type = m_aTypes[n];
        if ( type.Find(wxT('*')) == wxNOT_FOUND )
        {
            mimetypes.Add(type);
        }
    }

    return mimetypes.GetCount();
}

// ----------------------------------------------------------------------------
// writing to MIME type files
// ----------------------------------------------------------------------------

bool wxMimeTypesManagerImpl::Unassociate(wxFileType *ft)
{
    InitIfNeeded();

    wxArrayString sMimeTypes;
    ft->GetMimeTypes(sMimeTypes);

    size_t i;
    size_t nCount = sMimeTypes.GetCount();
    for (i = 0; i < nCount; i ++)
    {
        const wxString &sMime = sMimeTypes.Item(i);
        int nIndex = m_aTypes.Index(sMime);
        if ( nIndex == wxNOT_FOUND)
        {
            // error if we get here ??
            return false;
        }
        else
        {
            m_aTypes.RemoveAt(nIndex);
            m_aEntries.RemoveAt(nIndex);
            m_aExtensions.RemoveAt(nIndex);
            m_aDescriptions.RemoveAt(nIndex);
            m_aIcons.RemoveAt(nIndex);
        }
    }
    // check data integrity
    wxASSERT( m_aTypes.GetCount() == m_aEntries.GetCount() &&
            m_aTypes.GetCount() == m_aExtensions.GetCount() &&
            m_aTypes.GetCount() == m_aIcons.GetCount() &&
            m_aTypes.GetCount() == m_aDescriptions.GetCount() );

    return true;
}

#endif
  // wxUSE_MIMETYPE && wxUSE_FILE
