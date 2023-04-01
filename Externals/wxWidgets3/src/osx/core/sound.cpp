/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/core/sound.cpp
// Purpose:     wxSound class implementation using AudioToolbox
// Author:      Stefan Csomor
// Modified by: Stefan Csomor
// Created:     2009-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SOUND

#include "wx/sound.h"

#if wxOSX_USE_AUDIOTOOLBOX

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/timer.h"
#endif

#include "wx/file.h"

#include "wx/osx/private.h"

#include <AudioToolbox/AudioToolbox.h>

class wxOSXAudioToolboxSoundData : public wxSoundData
{
public:
    wxOSXAudioToolboxSoundData(const wxString& fileName);

    ~wxOSXAudioToolboxSoundData();

    virtual bool Play(unsigned flags);

    virtual void DoStop();
protected:
    static void CompletionCallback(SystemSoundID  mySSID, void * soundRef);
    void SoundCompleted();

    SystemSoundID m_soundID;
    wxString m_sndname; //file path
};

wxOSXAudioToolboxSoundData::wxOSXAudioToolboxSoundData(const wxString& fileName) :
    m_soundID(NULL)
{
    m_sndname = fileName;
}

wxOSXAudioToolboxSoundData::~wxOSXAudioToolboxSoundData()
{
    DoStop();
}

void
wxOSXAudioToolboxSoundData::CompletionCallback(SystemSoundID WXUNUSED(mySSID),
                                               void * soundRef)
{
    wxOSXAudioToolboxSoundData* data = (wxOSXAudioToolboxSoundData*) soundRef;

    data->SoundCompleted();

    if (data->IsMarkedForDeletion())
        delete data;
}

void wxOSXAudioToolboxSoundData::SoundCompleted()
{
    if ( m_flags & wxSOUND_ASYNC )
    {
        if (m_flags & wxSOUND_LOOP)
            AudioServicesPlaySystemSound(m_soundID);
        else
            Stop();
    }
    else
    {
        Stop();
        CFRunLoopStop(CFRunLoopGetCurrent());
    }

}

void wxOSXAudioToolboxSoundData::DoStop()
{
    if (m_soundID)
    {
        AudioServicesDisposeSystemSoundID (m_soundID);
        m_soundID = NULL;

        wxSound::SoundStopped(this);
    }
}

bool wxOSXAudioToolboxSoundData::Play(unsigned flags)
{
    Stop();

    m_flags = flags;

    wxCFRef<CFMutableStringRef> cfMutableString(CFStringCreateMutableCopy(NULL, 0, wxCFStringRef(m_sndname)));
    CFStringNormalize(cfMutableString,kCFStringNormalizationFormD);
    wxCFRef<CFURLRef> url(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfMutableString , kCFURLPOSIXPathStyle, false));

    AudioServicesCreateSystemSoundID(url, &m_soundID);
    AudioServicesAddSystemSoundCompletion( m_soundID, CFRunLoopGetCurrent(), NULL, wxOSXAudioToolboxSoundData::CompletionCallback, (void *) this );

    bool sync = !(flags & wxSOUND_ASYNC);

    AudioServicesPlaySystemSound(m_soundID);

    if ( sync )
    {
        while( m_soundID )
        {
            CFRunLoopRun();
        }
    }

    return true;
}

bool wxSound::Create(size_t WXUNUSED(size), const void* WXUNUSED(data))
{
    wxFAIL_MSG( "not implemented" );

    return false;
}

bool wxSound::Create(const wxString& fileName, bool isResource)
{
    wxCHECK_MSG( !isResource, false, "not implemented" );

    m_data = new wxOSXAudioToolboxSoundData(fileName);
    return true;
}

#endif // wxOSX_USE_AUDIOTOOLBOX

#endif //wxUSE_SOUND
