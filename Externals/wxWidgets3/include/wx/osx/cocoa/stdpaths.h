/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/stdpaths.h
// Purpose:     wxStandardPaths for Cocoa
// Author:      Tobias Taschner
// Created:     2015-09-09
// Copyright:   (c) 2015 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_STDPATHS_H_
#define _WX_COCOA_STDPATHS_H_

// ----------------------------------------------------------------------------
// wxStandardPaths
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxStandardPaths : public wxStandardPathsBase
{
public:
    virtual ~wxStandardPaths();

    // implement base class pure virtuals
    virtual wxString GetExecutablePath() const wxOVERRIDE;
    virtual wxString GetConfigDir() const wxOVERRIDE;
    virtual wxString GetUserConfigDir() const wxOVERRIDE;
    virtual wxString GetDataDir() const wxOVERRIDE;
    virtual wxString GetLocalDataDir() const wxOVERRIDE;
    virtual wxString GetUserDataDir() const wxOVERRIDE;
    virtual wxString GetPluginsDir() const wxOVERRIDE;
    virtual wxString GetResourcesDir() const wxOVERRIDE;
    virtual wxString
    GetLocalizedResourcesDir(const wxString& lang,
                             ResourceCat category = ResourceCat_None) const wxOVERRIDE;
    virtual wxString GetUserDir(Dir userDir) const wxOVERRIDE;

protected:
    // Ctor is protected, use wxStandardPaths::Get() instead of instantiating
    // objects of this class directly.
    wxStandardPaths();
};


#endif // _WX_COCOA_STDPATHS_H_
