/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/sound_osx.cpp
// Purpose:     wxSound class common osx code
// Author:      Stefan Csomor
// Modified by:
// Created:     2009-09-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SOUND

#include "wx/sound.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif

#include "wx/file.h"

#include "wx/vector.h"

#if wxOSX_USE_CARBON

#include "wx/timer.h"

class wxSoundTimer : public wxTimer
{
public:
    wxSoundTimer(wxSoundData* snd)
    : m_sound(snd)
    {
    }

    virtual ~wxSoundTimer()
    {
        Stop();
        if (m_sound)
            m_sound->DoStop();
    }

    void Notify() wxOVERRIDE
    {
        if (m_sound)
            m_sound->SoundTask();
    }

protected:
    wxSoundData* m_sound;
};

#endif // wxOSX_USE_CARBON

wxVector<wxSoundData*> s_soundsPlaying;

wxSoundData::wxSoundData()
{
#if wxOSX_USE_CARBON
    m_pTimer = NULL;
#endif // wxOSX_USE_CARBON
    m_markedForDeletion = false;
}

wxSoundData::~wxSoundData()
{
}

void wxSoundData::MarkForDeletion()
{
    m_markedForDeletion = true;
}

void wxSoundData::Stop()
{
    DoStop();
#if wxOSX_USE_CARBON
    wxDELETE(m_pTimer);
#endif // wxOSX_USE_CARBON
}

#if wxOSX_USE_CARBON

//Time between timer calls
#define MOVIE_DELAY 100

void wxSoundData::SoundTask()
{
}

void wxSoundData::CreateAndStartTimer()
{
    //Start timer and play movie asyncronously
    m_pTimer = new wxSoundTimer(this);
    m_pTimer->Start(MOVIE_DELAY, wxTIMER_CONTINUOUS);
}

#endif // wxOSX_USE_CARBON

wxSound::wxSound()
{
    Init();
}

wxSound::wxSound(const wxString& sFileName, bool isResource)
{
    Init();
    Create(sFileName, isResource);
}

wxSound::wxSound(size_t size, const void* data)
{
    Init();
    Create( size, data );
}

wxSound::~wxSound()
{
    // if the sound is in a playing state, just mark it to be deleted and
    // delete it after it plays. Otherwise, async sounds created on the stack
    // may never get the chance to play.
    bool isPlaying = false;
    for ( wxVector<wxSoundData*>::reverse_iterator s = s_soundsPlaying.rbegin();
         s != s_soundsPlaying.rend(); ++s )
    {
        if (*s == m_data)
        {
            isPlaying = true;
            break;
        }
    }

    if (isPlaying)
        m_data->MarkForDeletion();
    else
        delete m_data;
}

void wxSound::Init()
{
    m_data = NULL;
}

bool wxSound::DoPlay(unsigned flags) const
{
    if ( m_data )
    {
        s_soundsPlaying.push_back(m_data);
        if ( !m_data->Play(flags) )
            s_soundsPlaying.pop_back();
    }

    return false;
}

bool wxSound::IsPlaying()
{
    return s_soundsPlaying.size() > 0;
}

void wxSound::Stop()
{
    for ( wxVector<wxSoundData*>::reverse_iterator s = s_soundsPlaying.rbegin();
         s != s_soundsPlaying.rend(); ++s )
    {
        (*s)->Stop();
    }
}

// Notification when a sound has stopped
void wxSound::SoundStopped(const wxSoundData* data)
{
    for ( wxVector<wxSoundData*>::iterator s = s_soundsPlaying.begin();
         s != s_soundsPlaying.end(); ++s )
    {
        if ( (*s) == data )
        {
            s_soundsPlaying.erase(s);
            break;
        }
    }
}

#endif //wxUSE_SOUND
