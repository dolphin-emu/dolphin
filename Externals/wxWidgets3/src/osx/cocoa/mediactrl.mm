/////////////////////////////////////////////////////////////////////////////
// Name:        src/cocoa/mediactrl.cpp
// Purpose:     Built-in Media Backends for Cocoa
// Author:      Ryan Norton <wxprojects@comcast.net>
// Modified by:
// Created:     02/03/05
// RCS-ID:      $Id: mediactrl.mm 39285 2006-05-23 11:04:37Z ABX $
// Copyright:   (c) 2004-2005 Ryan Norton, (c) 2005 David Elliot
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//===========================================================================
//  DECLARATIONS
//===========================================================================

//---------------------------------------------------------------------------
// Pre-compiled header stuff
//---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

//---------------------------------------------------------------------------
// Compilation guard
//---------------------------------------------------------------------------
#if wxUSE_MEDIACTRL

#include "wx/mediactrl.h"

#ifndef WX_PRECOMP
    #include "wx/timer.h"
#endif

#include "wx/osx/private.h"

//===========================================================================
//  BACKEND DECLARATIONS
//===========================================================================

//---------------------------------------------------------------------------
//
//  wxQTMediaBackend
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//  QT Includes
//---------------------------------------------------------------------------
#include <QTKit/QTKit.h>

#include "wx/cocoa/autorelease.h"
#include "wx/cocoa/string.h"

#import <AppKit/NSMovie.h>
#import <AppKit/NSMovieView.h>

class WXDLLIMPEXP_FWD_MEDIA wxQTMediaBackend;

@interface wxQTMovie : QTMovie {
    
    wxQTMediaBackend* m_backend;
}

-(BOOL)isPlaying;

@end

class WXDLLIMPEXP_MEDIA wxQTMediaBackend : public wxMediaBackendCommonBase
{
public:

    wxQTMediaBackend();
    ~wxQTMediaBackend();

    virtual bool CreateControl(wxControl* ctrl, wxWindow* parent,
                                     wxWindowID id,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxValidator& validator,
                                     const wxString& name);

    virtual bool Play();
    virtual bool Pause();
    virtual bool Stop();

    virtual bool Load(const wxString& fileName);
    virtual bool Load(const wxURI& location);

    virtual wxMediaState GetState();

    virtual bool SetPosition(wxLongLong where);
    virtual wxLongLong GetPosition();
    virtual wxLongLong GetDuration();

    virtual void Move(int x, int y, int w, int h);
    wxSize GetVideoSize() const;

    virtual double GetPlaybackRate();
    virtual bool SetPlaybackRate(double dRate);

    virtual double GetVolume();
    virtual bool SetVolume(double dVolume);
    
    void Cleanup();
    void FinishLoad();

    virtual bool   ShowPlayerControls(wxMediaCtrlPlayerControls flags);
private:
    void DoShowPlayerControls(wxMediaCtrlPlayerControls flags);
    
    wxSize m_bestSize;              //Original movie size
    wxQTMovie* m_movie;               //QTMovie handle/instance
    QTMovieView* m_movieview;       //QTMovieView instance

    wxMediaCtrlPlayerControls m_interfaceflags; // Saved interface flags

    DECLARE_DYNAMIC_CLASS(wxQTMediaBackend);
};

// --------------------------------------------------------------------------
// wxQTMovie
// --------------------------------------------------------------------------

@implementation wxQTMovie 

- (id)initWithURL:(NSURL *)url error:(NSError **)errorPtr
{
    if ( [super initWithURL:url error:errorPtr] != nil )
    {
        m_backend = NULL;
        
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self selector:@selector(movieDidEnd:) 
                   name:QTMovieDidEndNotification object:nil];
        [nc addObserver:self selector:@selector(movieRateChanged:) 
                   name:QTMovieRateDidChangeNotification object:nil];
        [nc addObserver:self selector:@selector(loadStateChanged:) 
                   name:QTMovieLoadStateDidChangeNotification object:nil];
        
        return self;
    }
    else 
        return nil;
}

-(void)dealloc
{
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
	[nc removeObserver:self];
    
	[super dealloc];    
}

-(wxQTMediaBackend*) backend;
{
    return m_backend;
}

-(void) setBackend:(wxQTMediaBackend*) backend
{
    m_backend = backend;
}

- (void)movieDidEnd:(NSNotification *)notification
{
    if ( m_backend )
    {
        if ( m_backend->SendStopEvent() )
            m_backend->QueueFinishEvent();
    }
}

- (void)movieRateChanged:(NSNotification *)notification
{
	NSDictionary *userInfo = [notification userInfo];
	
	NSNumber *newRate = [userInfo objectForKey:QTMovieRateDidChangeNotificationParameter];
    
	if ([newRate intValue] == 0)
	{
		m_backend->QueuePauseEvent();
	}	
    else if ( [self isPlaying] == NO )
    {
		m_backend->QueuePlayEvent();
    }
}

-(void)loadStateChanged:(QTMovie *)movie
{
    long loadState = [[movie attributeForKey:QTMovieLoadStateAttribute] longValue];
    if (loadState >= QTMovieLoadStatePlayable)
    {
        // the movie has loaded enough media data to begin playing
    }
    else if (loadState >= QTMovieLoadStateLoaded)
    {
        m_backend->FinishLoad();
    }
    else if (loadState == -1)
    {
        // error occurred 
    }
}

-(BOOL)isPlaying
{
	if ([self rate] == 0)
	{
		return NO;
	}
	
	return YES;
}

@end

// --------------------------------------------------------------------------
// wxQTMediaBackend
// --------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxQTMediaBackend, wxMediaBackend);

wxQTMediaBackend::wxQTMediaBackend() : 
    m_interfaceflags(wxMEDIACTRLPLAYERCONTROLS_NONE),
    m_movie(nil), m_movieview(nil)
{
}

wxQTMediaBackend::~wxQTMediaBackend()
{
    Cleanup();
}

bool wxQTMediaBackend::CreateControl(wxControl* inctrl, wxWindow* parent,
                                     wxWindowID wid,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxValidator& validator,
                                     const wxString& name)
{
    wxMediaCtrl* mediactrl = (wxMediaCtrl*) inctrl;

    mediactrl->DontCreatePeer();
    
    if ( !mediactrl->wxControl::Create(
                                       parent, wid, pos, size,
                                       wxWindow::MacRemoveBordersFromStyle(style),
                                       validator, name))
    {
        return false;
    }
    
    NSRect r = wxOSXGetFrameForControl( mediactrl, pos , size ) ;
    QTMovieView* theView = [[QTMovieView alloc] initWithFrame: r];

    wxWidgetCocoaImpl* impl = new wxWidgetCocoaImpl(mediactrl,theView);
    mediactrl->SetPeer(impl);
    
    m_movieview = theView;
    // will be set up after load
    [theView setControllerVisible:NO];

    m_ctrl = mediactrl;
    return true;
}

bool wxQTMediaBackend::Load(const wxString& fileName)
{
    return Load(
                wxURI(
                    wxString( wxT("file://") ) + fileName
                     )
               );
}

bool wxQTMediaBackend::Load(const wxURI& location)
{
    wxCFStringRef uri(location.BuildURI());

    [m_movie release];
    wxQTMovie* movie = [[wxQTMovie alloc] initWithURL: [NSURL URLWithString: uri.AsNSString()] error: nil ];
    
    m_movie = movie;
    [m_movie setBackend:this];
    [m_movieview setMovie:movie];
    
    return movie != nil;
}

void wxQTMediaBackend::FinishLoad()
{
    DoShowPlayerControls(m_interfaceflags);
    
    NSRect r =[m_movieview movieBounds];
    m_bestSize.x = r.size.width;
    m_bestSize.y = r.size.height;
    
    NotifyMovieLoaded();

}

bool wxQTMediaBackend::Play()
{
    [m_movieview play:nil];
    return true;
}

bool wxQTMediaBackend::Pause()
{
    [m_movieview pause:nil];
    return true;
}

bool wxQTMediaBackend::Stop()
{
    [m_movieview pause:nil];
    [m_movieview gotoBeginning:nil];
    return true;
}

double wxQTMediaBackend::GetVolume()
{
    return [m_movie volume];
}

bool wxQTMediaBackend::SetVolume(double dVolume)
{
    [m_movie setVolume:dVolume];
    return true;
}
double wxQTMediaBackend::GetPlaybackRate()
{
    return [m_movie rate];
}

bool wxQTMediaBackend::SetPlaybackRate(double dRate)
{
    [m_movie setRate:dRate];
    return true;
}

bool wxQTMediaBackend::SetPosition(wxLongLong where)
{
    QTTime position;
    position = [m_movie currentTime];
    position.timeValue = (where.GetValue() / 1000.0) * position.timeScale;
    [m_movie setCurrentTime:position];
    return true;
}

wxLongLong wxQTMediaBackend::GetPosition()
{
    QTTime position = [m_movie currentTime];
    return ((double) position.timeValue) / position.timeScale * 1000;
}

wxLongLong wxQTMediaBackend::GetDuration()
{
    QTTime duration = [m_movie duration];
    return ((double) duration.timeValue) / duration.timeScale * 1000;
}

wxMediaState wxQTMediaBackend::GetState()
{
    if ( [m_movie isPlaying] )
        return wxMEDIASTATE_PLAYING;
    else
    {
        if ( GetPosition() == 0 )
            return wxMEDIASTATE_STOPPED;
        else
            return wxMEDIASTATE_PAUSED;
    }
}

void wxQTMediaBackend::Cleanup()
{
    [m_movieview setMovie:NULL];
    [m_movie release];
    m_movie = nil;
}

wxSize wxQTMediaBackend::GetVideoSize() const
{
    return m_bestSize;
}

void wxQTMediaBackend::Move(int x, int y, int w, int h)
{
}

bool wxQTMediaBackend::ShowPlayerControls(wxMediaCtrlPlayerControls flags)
{
    if ( m_interfaceflags != flags )
        DoShowPlayerControls(flags);
    
    m_interfaceflags = flags;    
    return true;
}

void wxQTMediaBackend::DoShowPlayerControls(wxMediaCtrlPlayerControls flags)
{
    if (flags == wxMEDIACTRLPLAYERCONTROLS_NONE )
    {
        [m_movieview setControllerVisible:NO];
    }
    else 
    {
        [m_movieview setStepButtonsVisible:(flags & wxMEDIACTRLPLAYERCONTROLS_STEP) ? YES:NO];
        [m_movieview setVolumeButtonVisible:(flags & wxMEDIACTRLPLAYERCONTROLS_VOLUME) ? YES:NO];
    }
}

//in source file that contains stuff you don't directly use
#include "wx/html/forcelnk.h"
FORCE_LINK_ME(basewxmediabackends);

#endif //wxUSE_MEDIACTRL
