/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/appprogress.h
// Purpose:     wxAppProgressIndicator OS X implementation
// Author:      Tobias Taschner
// Created:     2014-10-22
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_APPPROG_H_
#define _WX_OSX_APPPROG_H_

#include "wx/window.h"

class WXDLLIMPEXP_CORE wxAppProgressIndicator
    : public wxAppProgressIndicatorBase
{
public:
    wxAppProgressIndicator(wxWindow* parent = NULL, int maxValue = 100);
    virtual ~wxAppProgressIndicator();

    virtual bool IsAvailable() const wxOVERRIDE;

    virtual void SetValue(int value) wxOVERRIDE;
    virtual void SetRange(int range) wxOVERRIDE;
    virtual void Pulse() wxOVERRIDE;
    virtual void Reset() wxOVERRIDE;

private:
    int m_maxValue;
    void *m_dockIcon;

    wxDECLARE_NO_COPY_CLASS(wxAppProgressIndicator);
};

#endif  // _WX_OSX_APPPROG_H_
