///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/evtloopsrc.h
// Purpose:     wxCFEventLoopSource class
// Author:      Vadim Zeitlin
// Created:     2009-10-21
// RCS-ID:      $Id: evtloopsrc.h 64140 2010-04-25 21:33:16Z FM $
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_EVTLOOPSRC_H_
#define _WX_OSX_EVTLOOPSRC_H_

typedef struct __CFFileDescriptor *CFFileDescriptorRef;

// ----------------------------------------------------------------------------
// wxCFEventLoopSource: CoreFoundation-based wxEventLoopSource for OS X
// ----------------------------------------------------------------------------

class wxCFEventLoopSource : public wxEventLoopSource
{
public:
    wxCFEventLoopSource(wxEventLoopSourceHandler *handler, int flags)
        : wxEventLoopSource(handler, flags)
    {
        m_cffd = NULL;
    }

    // we take ownership of this CFFileDescriptorRef
    void SetFileDescriptor(CFFileDescriptorRef cffd);

    virtual ~wxCFEventLoopSource();

private:
    CFFileDescriptorRef m_cffd;

    wxDECLARE_NO_COPY_CLASS(wxCFEventLoopSource);
};

#endif // _WX_OSX_EVTLOOPSRC_H_

