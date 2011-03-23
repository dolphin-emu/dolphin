/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/mediactrl.cpp
// Purpose:     Built-in Media Backends for Mac
// Author:      Ryan Norton <wxprojects@comcast.net>
// Modified by:
// Created:     11/07/04
// RCS-ID:      $Id: mediactrl.cpp 67280 2011-03-22 14:17:38Z DS $
// Copyright:   (c) 2004-2006 Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// OK, a casual overseer of this file may wonder why we don't use
// either CreateMovieControl or HIMovieView...
//
// CreateMovieControl
//      1) Need to dispose and create each time a new movie is loaded
//      2) Not that many real advantages
//      3) Progressively buggier in higher OSX versions
//              (see main.c of QTCarbonShell sample for details)
// HIMovieView
//      1) Crashes on destruction in ALL cases on quite a few systems!
//          (With the only real "alternative" is to simply not
//           dispose of it and let it leak...)
//      2) Massive refreshing bugs with its movie controller between
//          movies
//
// At one point we had a complete implementation for CreateMovieControl
// and on my (RN) local copy I had one for HIMovieView - but they
// were simply deemed to be too buggy/unuseful. HIMovieView could
// have been useful as well because it uses OpenGL contexts instead
// of GWorlds. Perhaps someday when someone comes out with some
// ingenious workarounds :).
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_MEDIACTRL

#include "wx/mediactrl.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/timer.h"
#endif

#if wxOSX_USE_CARBON
#define USE_QUICKTIME 1
#else
#define USE_QUICKTIME 0
#endif

#if USE_QUICKTIME

#include "wx/osx/private.h"
#include <QuickTime/QuickTimeComponents.h>

//---------------------------------------------------------------------------
// Height and Width of movie controller in the movie control (apple samples)
//---------------------------------------------------------------------------
#define wxMCWIDTH   320
#define wxMCHEIGHT  16

//===========================================================================
//  BACKEND DECLARATIONS
//===========================================================================

//---------------------------------------------------------------------------
//  wxQTMediaBackend
//---------------------------------------------------------------------------

class WXDLLIMPEXP_MEDIA wxQTMediaBackend : public wxMediaBackendCommonBase
{
public:
    wxQTMediaBackend();
    virtual ~wxQTMediaBackend();

    virtual bool CreateControl(wxControl* ctrl, wxWindow* parent,
                                     wxWindowID id,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxValidator& validator,
                                     const wxString& name);

    virtual bool Load(const wxString& fileName);
    virtual bool Load(const wxURI& location);
    virtual bool Load(const wxURI& location,
                      const wxURI& WXUNUSED(proxy))
    {
        return Load(location);
    }

    virtual bool Play();
    virtual bool Pause();
    virtual bool Stop();

    virtual wxMediaState GetState();

    virtual bool SetPosition(wxLongLong where);
    virtual wxLongLong GetPosition();
    virtual wxLongLong GetDuration();

    virtual void Move(int x, int y, int w, int h);
    wxSize GetVideoSize() const;

    virtual double GetPlaybackRate();
    virtual bool SetPlaybackRate(double dRate);

    virtual double GetVolume();
    virtual bool SetVolume(double);

    void Cleanup();
    void FinishLoad();

    virtual bool ShowPlayerControls(wxMediaCtrlPlayerControls flags);

    virtual wxLongLong GetDownloadProgress();
    virtual wxLongLong GetDownloadTotal();

    virtual void MacVisibilityChanged();

    //
    //  ------  Implementation from now on  --------
    //
    bool DoPause();
    bool DoStop();

    void DoLoadBestSize();
    void DoSetControllerVisible(wxMediaCtrlPlayerControls flags);

    wxLongLong GetDataSizeFromStart(TimeValue end);

    Boolean IsQuickTime4Installed();
    void DoNewMovieController();

    static pascal void PPRMProc(
        Movie theMovie, OSErr theErr, void* theRefCon);

    //TODO: Last param actually long - does this work on 64bit machines?
    static pascal Boolean MCFilterProc(MovieController theController,
        short action, void *params, long refCon);

    static pascal OSStatus WindowEventHandler(
        EventHandlerCallRef inHandlerCallRef,
        EventRef inEvent, void *inUserData  );

    wxSize m_bestSize;          // Original movie size
    Movie m_movie;              // Movie instance
    bool m_bPlaying;            // Whether media is playing or not
    class wxTimer* m_timer;     // Timer for streaming the movie
    MovieController m_mc;       // MovieController instance
    wxMediaCtrlPlayerControls m_interfaceflags; // Saved interface flags

    // Event handlers and UPPs/Callbacks
    EventHandlerRef             m_windowEventHandler;
    EventHandlerUPP             m_windowUPP;

    MoviePrePrerollCompleteUPP  m_preprerollupp;
    MCActionFilterWithRefConUPP m_mcactionupp;

    GWorldPtr m_movieWorld;  //Offscreen movie GWorld

    friend class wxQTMediaEvtHandler;

    DECLARE_DYNAMIC_CLASS(wxQTMediaBackend)
};

// helper to hijack background erasing for the QT window
class WXDLLIMPEXP_MEDIA wxQTMediaEvtHandler : public wxEvtHandler
{
public:
    wxQTMediaEvtHandler(wxQTMediaBackend *qtb)
    {
        m_qtb = qtb;

        qtb->m_ctrl->Connect(
            qtb->m_ctrl->GetId(), wxEVT_ERASE_BACKGROUND,
            wxEraseEventHandler(wxQTMediaEvtHandler::OnEraseBackground),
            NULL, this);
    }

    void OnEraseBackground(wxEraseEvent& event);

private:
    wxQTMediaBackend *m_qtb;

    wxDECLARE_NO_COPY_CLASS(wxQTMediaEvtHandler);
};

//===========================================================================
//  IMPLEMENTATION
//===========================================================================


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// wxQTMediaBackend
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

IMPLEMENT_DYNAMIC_CLASS(wxQTMediaBackend, wxMediaBackend)

//Time between timer calls - this is the Apple recommondation to the TCL
//team I believe
#define MOVIE_DELAY 20

//---------------------------------------------------------------------------
//          wxQTMediaLoadTimer
//
//  QT, esp. QT for Windows is very picky about how you go about
//  async loading.  If you were to go through a Windows message loop
//  or a MoviesTask or both and then check the movie load state
//  it would still return 1000 (loading)... even (pre)prerolling doesn't
//  help.  However, making a load timer like this works
//---------------------------------------------------------------------------
class wxQTMediaLoadTimer : public wxTimer
{
public:
    wxQTMediaLoadTimer(wxQTMediaBackend* parent) :
      m_parent(parent) {}

    void Notify()
    {
        ::MCIdle(m_parent->m_mc);

        // kMovieLoadStatePlayable is not enough on MAC:
        // it plays, but IsMovieDone might return true (!)
        // sure we need to wait until kMovieLoadStatePlaythroughOK
        if (::GetMovieLoadState(m_parent->m_movie) >= 20000)
        {
            m_parent->FinishLoad();
            delete this;
        }
    }

protected:
    wxQTMediaBackend *m_parent;     // Backend pointer
};

// --------------------------------------------------------------------------
//          wxQTMediaPlayTimer - Handle Asyncronous Playing
//
// 1) Checks to see if the movie is done, and if not continues
//    streaming the movie
// 2) Sends the wxEVT_MEDIA_STOP event if we have reached the end of
//    the movie.
// --------------------------------------------------------------------------
class wxQTMediaPlayTimer : public wxTimer
{
public:
    wxQTMediaPlayTimer(wxQTMediaBackend* parent) :
        m_parent(parent) {}

    void Notify()
    {
        //
        //  OK, a little explaining - basically originally
        //  we only called MoviesTask if the movie was actually
        //  playing (not paused or stopped)... this was before
        //  we realized MoviesTask actually handles repainting
        //  of the current frame - so if you were to resize
        //  or something it would previously not redraw that
        //  portion of the movie.
        //
        //  So now we call MoviesTask always so that it repaints
        //  correctly.
        //
        ::MCIdle(m_parent->m_mc);

        //
        //  Handle the stop event - if the movie has reached
        //  the end, notify our handler
        //
        if (::IsMovieDone(m_parent->m_movie))
        {
            if ( m_parent->SendStopEvent() )
            {
                    m_parent->Stop();
                    wxASSERT(::GetMoviesError() == noErr);

                m_parent->QueueFinishEvent();
            }
        }
    }

protected:
    wxQTMediaBackend* m_parent;     // Backend pointer
};


//---------------------------------------------------------------------------
// wxQTMediaBackend Constructor
//
// Sets m_timer to NULL signifying we havn't loaded anything yet
//---------------------------------------------------------------------------
wxQTMediaBackend::wxQTMediaBackend()
    : m_movie(NULL), m_bPlaying(false), m_timer(NULL)
      , m_mc(NULL), m_interfaceflags(wxMEDIACTRLPLAYERCONTROLS_NONE)
      , m_preprerollupp(NULL), m_movieWorld(NULL)
{
}

//---------------------------------------------------------------------------
// wxQTMediaBackend Destructor
//
// 1) Cleans up the QuickTime movie instance
// 2) Decrements the QuickTime reference counter - if this reaches
//    0, QuickTime shuts down
// 3) Decrements the QuickTime Windows Media Layer reference counter -
//    if this reaches 0, QuickTime shuts down the Windows Media Layer
//---------------------------------------------------------------------------
wxQTMediaBackend::~wxQTMediaBackend()
{
    if (m_movie)
        Cleanup();

    // Cleanup for moviecontroller
    if (m_mc)
    {
        // destroy wxQTMediaEvtHandler we pushed on it
        m_ctrl->PopEventHandler(true);
        RemoveEventHandler(m_windowEventHandler);
        DisposeEventHandlerUPP(m_windowUPP);

        // Dispose of the movie controller
        ::DisposeMovieController(m_mc);
        m_mc = NULL;

        // Dispose of offscreen GWorld
        ::DisposeGWorld(m_movieWorld);
    }

    // Note that ExitMovies() is not necessary...
    ExitMovies();
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::CreateControl
//
// 1) Intializes QuickTime
// 2) Creates the control window
//---------------------------------------------------------------------------
bool wxQTMediaBackend::CreateControl(
    wxControl* ctrl,
    wxWindow* parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& validator,
    const wxString& name)
{
    if (!IsQuickTime4Installed())
        return false;

    EnterMovies();

    wxMediaCtrl* mediactrl = (wxMediaCtrl*)ctrl;

    //
    // Create window
    // By default wxWindow(s) is created with a border -
    // so we need to get rid of those
    //
    // Since we don't have a child window like most other
    // backends, we don't need wxCLIP_CHILDREN
    //
    if ( !mediactrl->wxControl::Create(
        parent, id, pos, size,
        wxWindow::MacRemoveBordersFromStyle(style),
        validator, name))
    {
        return false;
    }

#if wxUSE_VALIDATORS
    mediactrl->SetValidator(validator);
#endif

    m_ctrl = mediactrl;
    return true;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::IsQuickTime4Installed
//
// Determines whether version 4 of QT is installed
// (Pretty much for Classic only)
//---------------------------------------------------------------------------
Boolean wxQTMediaBackend::IsQuickTime4Installed()
{
    OSErr error;
    long result;

    error = Gestalt(gestaltQuickTime, &result);
    return (error == noErr) && (((result >> 16) & 0xffff) >= 0x0400);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Load (file version)
//
// 1) Get an FSSpec from the Windows path name
// 2) Open the movie
// 3) Obtain the movie instance from the movie resource
// 4) Close the movie resource
// 5) Finish loading
//---------------------------------------------------------------------------
bool wxQTMediaBackend::Load(const wxString& fileName)
{
    if (m_movie)
        Cleanup();

    ::ClearMoviesStickyError(); // clear previous errors so
                                // GetMoviesStickyError is useful

    OSErr err = noErr;
    short movieResFile;
    FSSpec sfFile;

    wxMacFilename2FSSpec( fileName, &sfFile );
    if (OpenMovieFile( &sfFile, &movieResFile, fsRdPerm ) != noErr)
        return false;

    short movieResID = 0;
    Str255 movieName;

    err = NewMovieFromFile(
        &m_movie,
        movieResFile,
        &movieResID,
        movieName,
        newMovieActive,
        NULL); // wasChanged

    // Do not use ::GetMoviesStickyError() here because it returns -2009
    // a.k.a. invalid track on valid mpegs
    if (err == noErr && ::GetMoviesError() == noErr)
    {
        ::CloseMovieFile(movieResFile);

        // Create movie controller/control
        DoNewMovieController();

        FinishLoad();
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Load (URL Version)
//
// 1) Build an escaped URI from location
// 2) Create a handle to store the URI string
// 3) Put the URI string inside the handle
// 4) Make a QuickTime URL data ref from the handle with the URI in it
// 5) Clean up the URI string handle
// 6) Do some prerolling
// 7) Finish Loading
//---------------------------------------------------------------------------
bool wxQTMediaBackend::Load(const wxURI& location)
{
    if (m_movie)
        Cleanup();

    ::ClearMoviesStickyError(); // clear previous errors so
                                // GetMoviesStickyError is useful

    wxString theURI = location.BuildURI();
    OSErr err;

    size_t len;
    const char* theURIString;

#if wxUSE_UNICODE
    wxCharBuffer buf = wxConvLocal.cWC2MB(theURI.wc_str(), theURI.length(), &len);
    theURIString = buf;
#else
    theURIString = theURI;
    len = theURI.length();
#endif

    Handle theHandle = ::NewHandleClear(len + 1);
    wxASSERT(theHandle);

    ::BlockMoveData(theURIString, *theHandle, len + 1);

    // create the movie from the handle that refers to the URI
    err = ::NewMovieFromDataRef(
        &m_movie,
        newMovieActive | newMovieAsyncOK /* | newMovieIdleImportOK*/,
        NULL, theHandle,
        URLDataHandlerSubType);

    ::DisposeHandle(theHandle);

    if (err == noErr && ::GetMoviesStickyError() == noErr)
    {
        // Movie controller resets prerolling, so we must create first
        DoNewMovieController();

        long timeNow;
        Fixed playRate;

        timeNow = ::GetMovieTime(m_movie, NULL);
        wxASSERT(::GetMoviesError() == noErr);

        playRate = ::GetMoviePreferredRate(m_movie);
        wxASSERT(::GetMoviesError() == noErr);

        //
        //  Note that the callback here is optional,
        //  but without it PrePrerollMovie can be buggy
        //  (see Apple ml).  Also, some may wonder
        //  why we need this at all - this is because
        //  Apple docs say QuickTime streamed movies
        //  require it if you don't use a Movie Controller,
        //  which we don't by default.
        //
        m_preprerollupp = wxQTMediaBackend::PPRMProc;
        ::PrePrerollMovie( m_movie, timeNow, playRate,
                           m_preprerollupp, (void*)this);

        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::DoNewMovieController
//
// Attaches movie to moviecontroller or creates moviecontroller
// if not created yet
//---------------------------------------------------------------------------
void wxQTMediaBackend::DoNewMovieController()
{
    if (!m_mc)
    {
        // Get top level window ref for some mac functions
        WindowRef wrTLW = (WindowRef) m_ctrl->MacGetTopLevelWindowRef();

        // MovieController not set up yet, so we need to create a new one.
        // You have to pass a valid movie to NewMovieController, evidently
        ::SetMovieGWorld(m_movie,
                       (CGrafPtr) GetWindowPort(wrTLW),
                       NULL);
        wxASSERT(::GetMoviesError() == noErr);

        Rect bounds = wxMacGetBoundsForControl(
            m_ctrl,
            m_ctrl->GetPosition(),
            m_ctrl->GetSize());

        m_mc = ::NewMovieController(
            m_movie, &bounds,
            mcTopLeftMovie | mcNotVisible /* | mcWithFrame */ );
        wxASSERT(::GetMoviesError() == noErr);

        ::MCDoAction(m_mc, 32, (void*)true); // mcActionSetKeysEnabled
        wxASSERT(::GetMoviesError() == noErr);

        // Setup a callback so we can tell when the user presses
        // play on the player controls
        m_mcactionupp = wxQTMediaBackend::MCFilterProc;
        ::MCSetActionFilterWithRefCon( m_mc, m_mcactionupp, (long)this );
        wxASSERT(::GetMoviesError() == noErr);

        // Part of a suggestion from Greg Hazel to repaint movie when idle
        m_ctrl->PushEventHandler(new wxQTMediaEvtHandler(this));

        // Create offscreen GWorld for where to "show" when window is hidden
        Rect worldRect;
        worldRect.left = worldRect.top = 0;
        worldRect.right = worldRect.bottom = 1;
        ::NewGWorld(&m_movieWorld, 0, &worldRect, NULL, NULL, 0);

        // Catch window messages:
        // if we do not do this and if the user clicks the play
        // button on the controller, for instance, nothing will happen...
        EventTypeSpec theWindowEventTypes[] =
        {
            { kEventClassMouse,     kEventMouseDown },
            { kEventClassMouse,     kEventMouseUp },
            { kEventClassMouse,     kEventMouseDragged },
            { kEventClassKeyboard,  kEventRawKeyDown },
            { kEventClassKeyboard,  kEventRawKeyRepeat },
            { kEventClassKeyboard,  kEventRawKeyUp },
            { kEventClassWindow,    kEventWindowUpdate },
            { kEventClassWindow,    kEventWindowActivated },
            { kEventClassWindow,    kEventWindowDeactivated }
        };
        m_windowUPP =
            NewEventHandlerUPP( wxQTMediaBackend::WindowEventHandler );
        InstallWindowEventHandler(
            wrTLW,
            m_windowUPP,
            GetEventTypeCount( theWindowEventTypes ), theWindowEventTypes,
            this,
            &m_windowEventHandler );
    }
    else
    {
        // MovieController already created:
        // Just change the movie in it and we're good to go
        Point thePoint;
        thePoint.h = thePoint.v = 0;
        ::MCSetMovie(m_mc, m_movie,
              (WindowRef)m_ctrl->MacGetTopLevelWindowRef(),
              thePoint);
        wxASSERT(::GetMoviesError() == noErr);
    }
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::FinishLoad
//
// Performs operations after a movie ready to play/loaded.
//---------------------------------------------------------------------------
void wxQTMediaBackend::FinishLoad()
{
    // get the real size of the movie
    DoLoadBestSize();

    // show the player controls if the user wants to
    if (m_interfaceflags)
        DoSetControllerVisible(m_interfaceflags);

    // we want millisecond precision
    ::SetMovieTimeScale(m_movie, 1000);
    wxASSERT(::GetMoviesError() == noErr);

    // start movie progress timer
    m_timer = new wxQTMediaPlayTimer(this);
    wxASSERT(m_timer);
    m_timer->Start(MOVIE_DELAY, wxTIMER_CONTINUOUS);

    // send loaded event and refresh size
    NotifyMovieLoaded();
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::DoLoadBestSize
//
// Sets the best size of the control from the real size of the movie
//---------------------------------------------------------------------------
void wxQTMediaBackend::DoLoadBestSize()
{
    // get the real size of the movie
    Rect outRect;
    ::GetMovieNaturalBoundsRect(m_movie, &outRect);
    wxASSERT(::GetMoviesError() == noErr);

    // determine best size
    m_bestSize.x = outRect.right - outRect.left;
    m_bestSize.y = outRect.bottom - outRect.top;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Play
//
// Start the QT movie
// (Apple recommends mcActionPrerollAndPlay but that's QT 4.1+)
//---------------------------------------------------------------------------
bool wxQTMediaBackend::Play()
{
    Fixed fixRate = (Fixed) (wxQTMediaBackend::GetPlaybackRate() * 0x10000);
    if (!fixRate)
        fixRate = ::GetMoviePreferredRate(m_movie);

    wxASSERT(fixRate != 0);

    if (!m_bPlaying)
        ::MCDoAction( m_mc, 8 /* mcActionPlay */, (void*) fixRate);

    bool result = (::GetMoviesError() == noErr);
    if (result)
    {
        m_bPlaying = true;
        QueuePlayEvent();
    }

    return result;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Pause
//
// Stop the movie
//---------------------------------------------------------------------------
bool wxQTMediaBackend::DoPause()
{
    // Stop the movie A.K.A. ::StopMovie(m_movie);
    if (m_bPlaying)
    {
        ::MCDoAction( m_mc, 8 /*mcActionPlay*/,  (void *) 0);
        m_bPlaying = false;
        return ::GetMoviesError() == noErr;
    }

    // already paused
    return true;
}

bool wxQTMediaBackend::Pause()
{
    bool bSuccess = DoPause();
    if (bSuccess)
        this->QueuePauseEvent();

    return bSuccess;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Stop
//
// 1) Stop the movie
// 2) Seek to the beginning of the movie
//---------------------------------------------------------------------------
bool wxQTMediaBackend::DoStop()
{
    if (!wxQTMediaBackend::DoPause())
        return false;

    ::GoToBeginningOfMovie(m_movie);
    return ::GetMoviesError() == noErr;
}

bool wxQTMediaBackend::Stop()
{
    bool bSuccess = DoStop();
    if (bSuccess)
        QueueStopEvent();

    return bSuccess;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetPlaybackRate
//
// 1) Get the movie playback rate from ::GetMovieRate
//---------------------------------------------------------------------------
double wxQTMediaBackend::GetPlaybackRate()
{
    return ( ((double)::GetMovieRate(m_movie)) / 0x10000);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::SetPlaybackRate
//
// 1) Convert dRate to Fixed and Set the movie rate through SetMovieRate
//---------------------------------------------------------------------------
bool wxQTMediaBackend::SetPlaybackRate(double dRate)
{
    ::SetMovieRate(m_movie, (Fixed) (dRate * 0x10000));
    return ::GetMoviesError() == noErr;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::SetPosition
//
// 1) Create a time record struct (TimeRecord) with appropriate values
// 2) Pass struct to SetMovieTime
//---------------------------------------------------------------------------
bool wxQTMediaBackend::SetPosition(wxLongLong where)
{
    TimeRecord theTimeRecord;
    memset(&theTimeRecord, 0, sizeof(TimeRecord));
    theTimeRecord.value.lo = where.GetLo();
    theTimeRecord.value.hi = where.GetHi();
    theTimeRecord.scale = ::GetMovieTimeScale(m_movie);
    theTimeRecord.base = ::GetMovieTimeBase(m_movie);
    ::SetMovieTime(m_movie, &theTimeRecord);

    if (::GetMoviesError() != noErr)
        return false;

    return true;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetPosition
//
// Calls GetMovieTime
//---------------------------------------------------------------------------
wxLongLong wxQTMediaBackend::GetPosition()
{
    return ::GetMovieTime(m_movie, NULL);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetVolume
//
// Gets the volume through GetMovieVolume - which returns a 16 bit short -
//
// +--------+--------+
// +   (1)  +   (2)  +
// +--------+--------+
//
// (1) first 8 bits are value before decimal
// (2) second 8 bits are value after decimal
//
// Volume ranges from -1.0 (gain but no sound), 0 (no sound and no gain) to
// 1 (full gain and sound)
//---------------------------------------------------------------------------
double wxQTMediaBackend::GetVolume()
{
    short sVolume = ::GetMovieVolume(m_movie);

    if (sVolume & (128 << 8)) //negative - no sound
        return 0.0;

    return sVolume / 256.0;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::SetVolume
//
// Sets the volume through SetMovieVolume - which takes a 16 bit short -
//
// +--------+--------+
// +   (1)  +   (2)  +
// +--------+--------+
//
// (1) first 8 bits are value before decimal
// (2) second 8 bits are value after decimal
//
// Volume ranges from -1.0 (gain but no sound), 0 (no sound and no gain) to
// 1 (full gain and sound)
//---------------------------------------------------------------------------
bool wxQTMediaBackend::SetVolume(double dVolume)
{
    ::SetMovieVolume(m_movie, (short) (dVolume * 256));
    return true;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetDuration
//
// Calls GetMovieDuration
//---------------------------------------------------------------------------
wxLongLong wxQTMediaBackend::GetDuration()
{
    return ::GetMovieDuration(m_movie);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetState
//
// Determines the current state - the timer keeps track of whether or not
// we are paused or stopped (if the timer is running we are playing)
//---------------------------------------------------------------------------
wxMediaState wxQTMediaBackend::GetState()
{
    // Could use
    // GetMovieActive/IsMovieDone/SetMovieActive
    // combo if implemented that way
    if (m_bPlaying)
        return wxMEDIASTATE_PLAYING;
    else if (!m_movie || wxQTMediaBackend::GetPosition() == 0)
        return wxMEDIASTATE_STOPPED;
    else
        return wxMEDIASTATE_PAUSED;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Cleanup
//
// Diposes of the movie timer, Control if native, and stops and disposes
// of the QT movie
//---------------------------------------------------------------------------
void wxQTMediaBackend::Cleanup()
{
    m_bPlaying = false;
    wxDELETE(m_timer);

    // Stop the movie:
    // Apple samples with CreateMovieControl typically
    // install a event handler and do this on the dispose
    // event, but we do it here for simplicity
    // (It might keep playing for several seconds after
    // control destruction if not)
    wxQTMediaBackend::Pause();

    // Dispose of control or remove movie from MovieController
    Point thePoint;
    thePoint.h = thePoint.v = 0;
    ::MCSetVisible(m_mc, false);
    ::MCSetMovie(m_mc, NULL, NULL, thePoint);

    ::DisposeMovie(m_movie);
    m_movie = NULL;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetVideoSize
//
// Returns the actual size of the QT movie
//---------------------------------------------------------------------------
wxSize wxQTMediaBackend::GetVideoSize() const
{
    return m_bestSize;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::Move
//
// Move the movie controller or movie control
// (we need to actually move the movie control manually...)
// Top 10 things to do with quicktime in March 93's issue
// of DEVELOP - very useful
// http:// www.mactech.com/articles/develop/issue_13/031-033_QuickTime_column.html
// OLD NOTE: Calling MCSetControllerBoundsRect without detaching
//          supposively resulted in a crash back then. Current code even
//          with CFM classic runs fine. If there is ever a problem,
//          take out the if 0 lines below
//---------------------------------------------------------------------------
void wxQTMediaBackend::Move(int x, int y, int w, int h)
{
    if (m_timer)
    {
        m_ctrl->GetParent()->MacWindowToRootWindow(&x, &y);
        Rect theRect = {y, x, y + h, x + w};

#if 0 // see note above
        ::MCSetControllerAttached(m_mc, false);
         wxASSERT(::GetMoviesError() == noErr);
#endif

        ::MCSetControllerBoundsRect(m_mc, &theRect);
        wxASSERT(::GetMoviesError() == noErr);

#if 0 // see note above
        if (m_interfaceflags)
        {
            ::MCSetVisible(m_mc, true);
            wxASSERT(::GetMoviesError() == noErr);
        }
#endif
    }
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::DoSetControllerVisible
//
// Utility function that takes care of showing the moviecontroller
// and showing/hiding the particular controls on it
//---------------------------------------------------------------------------
void wxQTMediaBackend::DoSetControllerVisible(
                        wxMediaCtrlPlayerControls flags)
{
    ::MCSetVisible(m_mc, true);

    // Take care of subcontrols
    if (::GetMoviesError() == noErr)
    {
        long mcFlags = 0;
        ::MCDoAction(m_mc, 39/*mcActionGetFlags*/, (void*)&mcFlags);

        if (::GetMoviesError() == noErr)
        {
             mcFlags |= (  //(1<<0)/*mcFlagSuppressMovieFrame*/ |
                     (1 << 3)/*mcFlagsUseWindowPalette*/
                       | ((flags & wxMEDIACTRLPLAYERCONTROLS_STEP)
                          ? 0 : (1 << 1)/*mcFlagSuppressStepButtons*/)
                       | ((flags & wxMEDIACTRLPLAYERCONTROLS_VOLUME)
                          ? 0 : (1 << 2)/*mcFlagSuppressSpeakerButton*/)
                          //if we take care of repainting ourselves
         //              | (1 << 4) /*mcFlagDontInvalidate*/
                          );

            ::MCDoAction(m_mc, 38/*mcActionSetFlags*/, (void*)mcFlags);
        }
    }

    // Adjust height and width of best size for movie controller
    // if the user wants it shown
    m_bestSize.x = m_bestSize.x > wxMCWIDTH ? m_bestSize.x : wxMCWIDTH;
    m_bestSize.y += wxMCHEIGHT;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::ShowPlayerControls
//
// Shows/Hides subcontrols on the media control
//---------------------------------------------------------------------------
bool wxQTMediaBackend::ShowPlayerControls(wxMediaCtrlPlayerControls flags)
{
    if (!m_mc)
        return false; // no movie controller...

    bool bSizeChanged = false;

    // if the controller is visible and we want to hide it do so
    if (m_interfaceflags && !flags)
    {
        bSizeChanged = true;
        DoLoadBestSize();
        ::MCSetVisible(m_mc, false);
    }
    else if (!m_interfaceflags && flags) // show controller if hidden
    {
        bSizeChanged = true;
        DoSetControllerVisible(flags);
    }

    // readjust parent sizers
    if (bSizeChanged)
    {
        NotifyMovieSizeChanged();

        // remember state in case of loading new media
        m_interfaceflags = flags;
    }

    return ::GetMoviesError() == noErr;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetDataSizeFromStart
//
// Calls either GetMovieDataSize or GetMovieDataSize64 with a value
// of 0 for the starting value
//---------------------------------------------------------------------------
wxLongLong wxQTMediaBackend::GetDataSizeFromStart(TimeValue end)
{
#if 0 // old pre-qt4 way
    return ::GetMovieDataSize(m_movie, 0, end)
#else // qt4 way
    wide llDataSize;
    ::GetMovieDataSize64(m_movie, 0, end, &llDataSize);
    return wxLongLong(llDataSize.hi, llDataSize.lo);
#endif
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetDownloadProgress
//---------------------------------------------------------------------------
wxLongLong wxQTMediaBackend::GetDownloadProgress()
{
#if 0 // hackish and slow
    Handle hMovie = NewHandle(0);
    PutMovieIntoHandle(m_movie, hMovie);
    long lSize = GetHandleSize(hMovie);
    DisposeHandle(hMovie);

    return lSize;
#else
    TimeValue tv;
    if (::GetMaxLoadedTimeInMovie(m_movie, &tv) != noErr)
    {
        wxLogDebug(wxT("GetMaxLoadedTimeInMovie failed"));
        return 0;
    }

    return wxQTMediaBackend::GetDataSizeFromStart(tv);
#endif
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::GetDownloadTotal
//---------------------------------------------------------------------------
wxLongLong wxQTMediaBackend::GetDownloadTotal()
{
    return wxQTMediaBackend::GetDataSizeFromStart(
                    ::GetMovieDuration(m_movie)
                                                 );
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::MacVisibilityChanged
//
// The main problem here is that Windows quicktime, for example,
// renders more directly to a HWND. Mac quicktime does not do this
// and instead renders to the port of the WindowRef/WindowPtr on top
// of everything else/all other windows.
//
// So, for example, if you were to have a CreateTabsControl/wxNotebook
// and change pages, even if you called HIViewSetVisible/SetControlVisibility
// directly the movie will still continue playing on top of everything else
// if you went to a different tab.
//
// Note that another issue, and why we call MCSetControllerPort instead
// of SetMovieGWorld directly, is that in addition to rendering on
// top of everything else the last created controller steals mouse and
// other input from everything else in the window, including other
// controllers. Setting the port of it releases this behaviour.
//---------------------------------------------------------------------------
void wxQTMediaBackend::MacVisibilityChanged()
{
    if(!m_mc || !m_ctrl->m_bLoaded)
        return; //not initialized yet

    if(m_ctrl->IsShownOnScreen())
    {
        //The window is being shown again, so set the GWorld of the
        //controller back to the port of the parent WindowRef
        WindowRef wrTLW =
            (WindowRef) m_ctrl->MacGetTopLevelWindowRef();

        ::MCSetControllerPort(m_mc, (CGrafPtr) GetWindowPort(wrTLW));
        wxASSERT(::GetMoviesError() == noErr);
    }
    else
    {
        //We are being hidden - set the GWorld of the controller
        //to the offscreen GWorld
        ::MCSetControllerPort(m_mc, m_movieWorld);
        wxASSERT(::GetMoviesError() == noErr);
    }
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::OnEraseBackground
//
// Suggestion from Greg Hazel to repaint the movie when idle
// (on pause also)
//---------------------------------------------------------------------------
void wxQTMediaEvtHandler::OnEraseBackground(wxEraseEvent& WXUNUSED(evt))
{
    // Work around Nasty OSX drawing bug:
    // http://lists.apple.com/archives/QuickTime-API/2002/Feb/msg00311.html
    WindowRef wrTLW = (WindowRef) m_qtb->m_ctrl->MacGetTopLevelWindowRef();

    RgnHandle region = ::MCGetControllerBoundsRgn(m_qtb->m_mc);
    ::MCInvalidate(m_qtb->m_mc, wrTLW, region);
    ::MCIdle(m_qtb->m_mc);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::PPRMProc (static)
//
// Called when done PrePrerolling the movie.
// Note that in 99% of the cases this does nothing...
// Anyway we set up the loading timer here to tell us when the movie is done
//---------------------------------------------------------------------------
pascal void wxQTMediaBackend::PPRMProc(
    Movie theMovie,
    OSErr WXUNUSED_UNLESS_DEBUG(theErr),
    void* theRefCon)
{
    wxASSERT( theMovie );
    wxASSERT( theRefCon );
    wxASSERT( theErr == noErr );

    wxQTMediaBackend* pBE = (wxQTMediaBackend*) theRefCon;

    long lTime = ::GetMovieTime(theMovie,NULL);
    Fixed rate = ::GetMoviePreferredRate(theMovie);
    ::PrerollMovie(theMovie,lTime,rate);
    pBE->m_timer = new wxQTMediaLoadTimer(pBE);
    pBE->m_timer->Start(MOVIE_DELAY);
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::MCFilterProc (static)
//
// Callback for when the movie controller receives a message
//---------------------------------------------------------------------------
pascal Boolean wxQTMediaBackend::MCFilterProc(
    MovieController WXUNUSED(theController),
    short action,
    void * WXUNUSED(params),
    long refCon)
{
    wxQTMediaBackend* pThis = (wxQTMediaBackend*)refCon;

    switch (action)
    {
    case 1:
        // don't process idle events
        break;

    case 8:
        // play button triggered - MC will set movie to opposite state
        // of current - playing ? paused : playing
        pThis->m_bPlaying = !(pThis->m_bPlaying);
        break;

    default:
        break;
    }

    return 0;
}

//---------------------------------------------------------------------------
// wxQTMediaBackend::WindowEventHandler [static]
//
// Event callback for the top level window of our control that passes
// messages to our moviecontroller so it can receive mouse clicks etc.
//---------------------------------------------------------------------------
pascal OSStatus wxQTMediaBackend::WindowEventHandler(
    EventHandlerCallRef WXUNUSED(inHandlerCallRef),
    EventRef inEvent,
    void *inUserData)
{
    wxQTMediaBackend* be = (wxQTMediaBackend*) inUserData;

    // Only process keyboard messages on this window if it actually
    // has focus, otherwise it will steal keystrokes from other windows!
    // As well as when it is not loaded properly as it
    // will crash in MCIsPlayerEvent
    if((GetEventClass(inEvent) == kEventClassKeyboard &&
        wxWindow::FindFocus() != be->m_ctrl)
        || !be->m_ctrl->m_bLoaded)
            return eventNotHandledErr;

    // Pass the event onto the movie controller
    EventRecord theEvent;
    ConvertEventRefToEventRecord( inEvent, &theEvent );
    OSStatus err;

    // TODO: Apple says MCIsPlayerEvent is depreciated and
    // MCClick, MCKey, MCIdle etc. should be used
    // (RN: Of course that's what they say about
    //  CreateMovieControl and HIMovieView as well, LOL!)
    err = ::MCIsPlayerEvent( be->m_mc, &theEvent );

    // Pass on to other event handlers if not handled- i.e. wx
    if (err != noErr)
        return noErr;
    else
        return eventNotHandledErr;
}

#endif

// in source file that contains stuff you don't directly use
#include "wx/html/forcelnk.h"
FORCE_LINK_ME(basewxmediabackends)

#endif // wxUSE_MEDIACTRL
