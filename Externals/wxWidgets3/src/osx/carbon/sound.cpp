/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/sound.cpp
// Purpose:     wxSound class implementation: optional
// Author:      Ryan Norton
// Modified by: Stefan Csomor
// Created:     1998-01-01
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_SOUND

#if wxOSX_USE_QUICKTIME

#include "wx/sound.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/timer.h"
#endif

#include "wx/file.h"

// Carbon QT Implementation Details -
//
// Memory:
// 1) OpenDefaultComponent(MovieImportType, kQTFileTypeWave);
// 2) NewMovie(0);
// 3) MovieImportDataRef() //Pass Memory Location to this
// 4) PlayMovie();
// 5) IsMovieDone(), MoviesTask() //2nd param is minimum wait time to allocate to quicktime
//
// File:
// 1) Path as CFString
// 2) Call QTNewDataReferenceFromFullPathCFString
// 3) Call NewMovieFromDataRef
// 4) Call CloseMovieFile
// 4) PlayMovie();
// 5) IsMovieDone(), MoviesTask() //2nd param is minimum wait time to allocate to quicktime
//

#ifdef __WXMAC__
  #include "wx/osx/private.h"
  #if wxOSX_USE_COCOA_OR_CARBON
    #include <QuickTime/QuickTimeComponents.h>
  #endif
#else
  #include <qtml.h>
#endif

#define MOVIE_DELAY 100

// ------------------------------------------------------------------
//          SoundManager
// ------------------------------------------------------------------

class wxOSXSoundManagerSoundData : public wxSoundData
{
public:
    wxOSXSoundManagerSoundData(const wxString& fileName);
    ~wxOSXSoundManagerSoundData();

    virtual bool Play(unsigned flags);
    virtual void SoundTask();

    void        DoStop();
protected:
    SndListHandle m_hSnd;
    SndChannelPtr m_pSndChannel;
};

wxOSXSoundManagerSoundData::wxOSXSoundManagerSoundData(const wxString& fileName) :
    m_pSndChannel(NULL)
{
    Str255 lpSnd ;

    wxMacStringToPascal( fileName , lpSnd ) ;

    m_hSnd = (SndListHandle) GetNamedResource('snd ', (const unsigned char *) lpSnd);
}

wxOSXSoundManagerSoundData::~wxOSXSoundManagerSoundData()
{
    DoStop();
    ReleaseResource((Handle)m_hSnd);
}

void wxOSXSoundManagerSoundData::DoStop()
{
    if ( m_pSndChannel )
    {
        SndDisposeChannel(m_pSndChannel, TRUE /* stop immediately, not after playing */);
        m_pSndChannel = NULL;
        wxSound::SoundStopped(this);
    }

    if (IsMarkedForDeletion())
        delete this;
}

bool wxOSXSoundManagerSoundData::Play(unsigned flags)
{
    Stop();

    m_flags = flags;

    SoundComponentData data;
    unsigned long numframes, offset;

    ParseSndHeader((SndListHandle)m_hSnd, &data, &numframes, &offset);

    SndNewChannel(&m_pSndChannel, sampledSynth,
                  initNoInterp
                  + (data.numChannels == 1 ? initMono : initStereo), NULL);

    if(SndPlay(m_pSndChannel, (SndListHandle) m_hSnd, flags & wxSOUND_ASYNC ? 1 : 0) != noErr)
        return false;

    if (flags & wxSOUND_ASYNC)
        CreateAndStartTimer();
    else
        DoStop();

    return true;
}

void wxOSXSoundManagerSoundData::SoundTask()
{
    SCStatus stat;

    if (SndChannelStatus((SndChannelPtr)m_pSndChannel, sizeof(SCStatus), &stat) != 0)
        Stop();

    //if the sound isn't playing anymore, see if it's looped,
    //and if so play it again, otherwise close things up
    if (stat.scChannelBusy == FALSE)
    {
        if (m_flags & wxSOUND_LOOP)
        {
            if(SndPlay((SndChannelPtr)m_pSndChannel, (SndListHandle) m_hSnd, true) != noErr)
                Stop();
        }
        else
            Stop();
    }
}

// ------------------------------------------------------------------
//          QuickTime
// ------------------------------------------------------------------

bool wxInitQT();
bool wxInitQT()
{
#ifndef __WXMAC__
    int nError;
    //-2093 no dll
    if ((nError = InitializeQTML(0)) != noErr)
    {
        wxLogSysError(wxString::Format(wxT("Couldn't Initialize Quicktime-%i"), nError));
        return false;
    }
#endif
    EnterMovies();
    return true;
}

void wxExitQT();
void wxExitQT()
{
    //Note that ExitMovies() is not necessary, but
    //the docs are fuzzy on whether or not TerminateQTML is
    ExitMovies();

#ifndef __WXMAC__
    TerminateQTML();
#endif
}

class wxOSXQuickTimeSoundData : public wxSoundData
{
public:
    wxOSXQuickTimeSoundData(const wxString& fileName);
    wxOSXQuickTimeSoundData(size_t size, const void* data);
    ~wxOSXQuickTimeSoundData();

    virtual bool Play(unsigned flags);
    virtual void SoundTask();
    virtual void DoStop();
protected:
    Movie m_movie;

    wxString m_sndname; //file path
    Handle m_soundHandle;
};


wxOSXQuickTimeSoundData::wxOSXQuickTimeSoundData(const wxString& fileName) :
    m_movie(NULL), m_soundHandle(NULL)
{
    m_sndname = fileName;
}

wxOSXQuickTimeSoundData::wxOSXQuickTimeSoundData(size_t size, const void* data) :
    m_movie(NULL)
{
    m_soundHandle = NewHandleClear((Size)size);
    BlockMove(data, *m_soundHandle, size);
}

wxOSXQuickTimeSoundData::~wxOSXQuickTimeSoundData()
{
    if ( m_soundHandle )
        DisposeHandle(m_soundHandle);
}

bool wxOSXQuickTimeSoundData::Play(unsigned flags)
{
    if ( m_movie )
        Stop();

    m_flags = flags;

    if (!wxInitQT())
        return false;

    if( m_soundHandle )
    {
        Handle dataRef = nil;
        MovieImportComponent miComponent;
        Track targetTrack = nil;
        TimeValue addedDuration = 0;
        long outFlags = 0;
        OSErr err;
        ComponentResult result;

        err = PtrToHand(&m_soundHandle, &dataRef, sizeof(Handle));

        HLock(m_soundHandle);
        if (memcmp(&(*m_soundHandle)[8], "WAVE", 4) == 0)
            miComponent = OpenDefaultComponent(MovieImportType, kQTFileTypeWave);
        else if (memcmp(&(*m_soundHandle)[8], "AIFF", 4) == 0)
            miComponent = OpenDefaultComponent(MovieImportType, kQTFileTypeAIFF);
        else if (memcmp(&(*m_soundHandle)[8], "AIFC", 4) == 0)
            miComponent = OpenDefaultComponent(MovieImportType, kQTFileTypeAIFC);
        else
        {
            HUnlock(m_soundHandle);
            wxLogSysError(wxT("wxSound - Location in memory does not contain valid data"));
            return false;
        }

        HUnlock(m_soundHandle);
        m_movie = NewMovie(0);

        result = MovieImportDataRef(miComponent,                dataRef,
                                    HandleDataHandlerSubType,   m_movie,
                                    nil,                        &targetTrack,
                                    nil,                        &addedDuration,
                                    movieImportCreateTrack,     &outFlags);

        if (result != noErr)
        {
            wxLogSysError(wxString::Format(wxT("Couldn't import movie data\nError:%i"), (int)result));
        }

        SetMovieVolume(m_movie, kFullVolume);
        GoToBeginningOfMovie(m_movie);
    }
    else
    {
        OSErr err = noErr ;

        Handle dataRef = NULL;
        OSType dataRefType;

        err = QTNewDataReferenceFromFullPathCFString(wxCFStringRef(m_sndname,wxLocale::GetSystemEncoding()),
                                                     (UInt32)kQTNativeDefaultPathStyle, 0, &dataRef, &dataRefType);

        wxASSERT(err == noErr);

        if (NULL != dataRef || err != noErr)
        {
            err = NewMovieFromDataRef( &m_movie, newMovieDontAskUnresolvedDataRefs , NULL, dataRef, dataRefType );
            wxASSERT(err == noErr);
            DisposeHandle(dataRef);
        }

        if (err != noErr)
        {
            wxLogSysError(
                          wxString::Format(wxT("wxSound - Could not open file: %s\nError:%i"), m_sndname.c_str(), err )
                          );
            return false;
        }
    }

    //Start the m_movie!
    StartMovie(m_movie);

    if (flags & wxSOUND_ASYNC)
    {
        CreateAndStartTimer();
    }
    else
    {
        wxASSERT_MSG(!(flags & wxSOUND_LOOP), wxT("Can't loop and play syncronously at the same time"));

        //Play movie until it ends, then exit
        //Note that due to quicktime caching this may not always
        //work 100% correctly
        while (!IsMovieDone(m_movie))
            MoviesTask(m_movie, 1);

        DoStop();
    }

    return true;
}

void wxOSXQuickTimeSoundData::DoStop()
{
    if( m_movie )
    {
        StopMovie(m_movie);
        DisposeMovie(m_movie);
        m_movie = NULL;
        wxSound::SoundStopped(this);
        wxExitQT();
    }
}

void wxOSXQuickTimeSoundData::SoundTask()
{
    if(IsMovieDone(m_movie))
    {
        if (m_flags & wxSOUND_LOOP)
        {
            StopMovie(m_movie);
            GoToBeginningOfMovie(m_movie);
            StartMovie(m_movie);
        }
        else
            Stop();
    }
    else
        MoviesTask(m_movie, MOVIE_DELAY); //Give QT time to play movie
}

bool wxSound::Create(size_t size, const void* data)
{
    m_data = new wxOSXQuickTimeSoundData(size,data);
    return true;
}

bool wxSound::Create(const wxString& fileName, bool isResource)
{
    if ( isResource )
        m_data = new wxOSXSoundManagerSoundData(fileName);
    else
        m_data = new wxOSXQuickTimeSoundData(fileName);
    return true;
}

#endif // wxOSX_USE_QUICKTIME

#endif //wxUSE_SOUND
