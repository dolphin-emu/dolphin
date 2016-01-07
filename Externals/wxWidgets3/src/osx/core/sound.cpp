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
    explicit wxOSXAudioToolboxSoundData(SystemSoundID soundID);

    virtual ~wxOSXAudioToolboxSoundData();

    virtual bool Play(unsigned flags) wxOVERRIDE;

    virtual void DoStop() wxOVERRIDE;
protected:
    static void CompletionCallback(SystemSoundID  mySSID, void * soundRef);
    void SoundCompleted();

    SystemSoundID m_soundID;
    bool m_playing;
};

wxOSXAudioToolboxSoundData::wxOSXAudioToolboxSoundData(SystemSoundID soundID) :
    m_soundID(soundID)
{
    m_playing = false;
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
    if ( m_playing )
    {
        m_playing = false;
        AudioServicesDisposeSystemSoundID (m_soundID);

        wxSound::SoundStopped(this);
    }
}

bool wxOSXAudioToolboxSoundData::Play(unsigned flags)
{
    Stop();

    m_flags = flags;

    AudioServicesAddSystemSoundCompletion( m_soundID, CFRunLoopGetCurrent(), NULL, wxOSXAudioToolboxSoundData::CompletionCallback, (void *) this );

    m_playing = true;

    AudioServicesPlaySystemSound(m_soundID);

    if ( !(flags & wxSOUND_ASYNC) )
    {
        while ( m_playing )
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

    wxCFRef<CFMutableStringRef> cfMutableString(CFStringCreateMutableCopy(NULL, 0, wxCFStringRef(fileName)));
    CFStringNormalize(cfMutableString,kCFStringNormalizationFormD);
    wxCFRef<CFURLRef> url(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfMutableString , kCFURLPOSIXPathStyle, false));

    SystemSoundID soundID;
    OSStatus err = AudioServicesCreateSystemSoundID(url, &soundID);
    if ( err != 0 )
    {
        wxLogError(_("Failed to load sound from \"%s\" (error %d)."), fileName, err);
        return false;
    }

    m_data = new wxOSXAudioToolboxSoundData(soundID);

    return true;
}

#endif // wxOSX_USE_AUDIOTOOLBOX

#endif //wxUSE_SOUND
