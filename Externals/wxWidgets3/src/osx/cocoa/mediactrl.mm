/////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/cocoa/mediactrl.mm
// Purpose:     Built-in Media Backends for Cocoa
// Author:      Ryan Norton <wxprojects@comcast.net>, Stefan Csomor
// Modified by:
// Created:     02/03/05
// Copyright:   (c) 2004-2005 Ryan Norton, (c) 2005 David Elliot, (c) Stefan Csomor
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

#ifndef wxOSX_USE_QTKIT
    #if wxOSX_USE_IPHONE
        #define wxOSX_USE_QTKIT 0
        #define wxOSX_USE_AVFOUNDATION 1
    #else
        #define wxOSX_USE_QTKIT 1
        #define wxOSX_USE_AVFOUNDATION 0
    #endif
#else
    #if wxOSX_USE_QTKIT
        #define wxOSX_USE_AVFOUNDATION 0
    #else
        #define wxOSX_USE_AVFOUNDATION 1
    #endif
#endif

#if wxOSX_USE_AVFOUNDATION && wxOSX_USE_COCOA && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9
    #define wxOSX_USE_AVKIT 1
#else
    #define wxOSX_USE_AVKIT 0
#endif

//===========================================================================
//  BACKEND DECLARATIONS
//===========================================================================

#if wxOSX_USE_QTKIT

//---------------------------------------------------------------------------
//
//  wxQTMediaBackend
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//  QT Includes
//---------------------------------------------------------------------------
#include <QTKit/QTKit.h>

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
                                     const wxString& name) wxOVERRIDE;

    virtual bool Play() wxOVERRIDE;
    virtual bool Pause() wxOVERRIDE;
    virtual bool Stop() wxOVERRIDE;

    virtual bool Load(const wxString& fileName) wxOVERRIDE;
    virtual bool Load(const wxURI& location) wxOVERRIDE;

    virtual wxMediaState GetState() wxOVERRIDE;

    virtual bool SetPosition(wxLongLong where) wxOVERRIDE;
    virtual wxLongLong GetPosition() wxOVERRIDE;
    virtual wxLongLong GetDuration() wxOVERRIDE;

    virtual void Move(int x, int y, int w, int h) wxOVERRIDE;
    wxSize GetVideoSize() const wxOVERRIDE;

    virtual double GetPlaybackRate() wxOVERRIDE;
    virtual bool SetPlaybackRate(double dRate) wxOVERRIDE;

    virtual double GetVolume() wxOVERRIDE;
    virtual bool SetVolume(double dVolume) wxOVERRIDE;
    
    void Cleanup();
    void FinishLoad();

    virtual bool   ShowPlayerControls(wxMediaCtrlPlayerControls flags) wxOVERRIDE;
private:
    void DoShowPlayerControls(wxMediaCtrlPlayerControls flags);
    
    wxSize m_bestSize;              //Original movie size
    wxQTMovie* m_movie;               //QTMovie handle/instance
    QTMovieView* m_movieview;       //QTMovieView instance

    wxMediaCtrlPlayerControls m_interfaceflags; // Saved interface flags

    wxDECLARE_DYNAMIC_CLASS(wxQTMediaBackend);
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

-(wxQTMediaBackend*) backend
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

-(void)loadStateChanged:(NSNotification *)notification
{
    QTMovie *movie = [notification object];
    long loadState = [[movie attributeForKey:QTMovieLoadStateAttribute] longValue];
    if ( loadState == QTMovieLoadStateError )
    {
        // error occurred 
    }
    else if (loadState >= QTMovieLoadStatePlayable)
    {
        // the movie has loaded enough media data to begin playing, but we don't have an event for that yet
    }
    else if (loadState >= QTMovieLoadStateComplete) // we might use QTMovieLoadStatePlaythroughOK
    {
        m_backend->FinishLoad();
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

wxIMPLEMENT_DYNAMIC_CLASS(wxQTMediaBackend, wxMediaBackend);

wxQTMediaBackend::wxQTMediaBackend() : 
    m_movie(nil), m_movieview(nil),
    m_interfaceflags(wxMEDIACTRLPLAYERCONTROLS_NONE)
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
    NSURL *url = [NSURL URLWithString: uri.AsNSString()];
    
    if (! [wxQTMovie canInitWithURL:url])
        return false;
    
    [m_movie release];
    wxQTMovie* movie = [[wxQTMovie alloc] initWithURL:url error: nil ];

    m_movie = movie;
    if (movie != nil)
    {
        [m_movie setBackend:this];
        [m_movieview setMovie:movie];

        // If the media file is able to be loaded quickly then there may not be
        // any QTMovieLoadStateDidChangeNotification message sent, so we need to
        // also check the load state here and finish our initialization if it has
        // been loaded.
        long loadState = [[m_movie attributeForKey:QTMovieLoadStateAttribute] longValue];
        if (loadState >= QTMovieLoadStateComplete)
        {
            FinishLoad();
        }        
    }
    
    return movie != nil;
}

void wxQTMediaBackend::FinishLoad()
{
    DoShowPlayerControls(m_interfaceflags);
    
    NSSize s = [[m_movie attributeForKey:QTMovieNaturalSizeAttribute] sizeValue];
    m_bestSize = wxSize(s.width, s.height);
    
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
    // as we have a native player, no need to move the video area 
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
        [m_movieview setControllerVisible:YES];
        
        [m_movieview setStepButtonsVisible:(flags & wxMEDIACTRLPLAYERCONTROLS_STEP) ? YES:NO];
        [m_movieview setVolumeButtonVisible:(flags & wxMEDIACTRLPLAYERCONTROLS_VOLUME) ? YES:NO];
    }
}

#endif

#if wxOSX_USE_AVFOUNDATION

//---------------------------------------------------------------------------
//
//  wxAVMediaBackend
//
//---------------------------------------------------------------------------

#import <AVFoundation/AVFoundation.h>

#if wxOSX_USE_AVKIT
#import <AVKit/AVKit.h>
#endif

class WXDLLIMPEXP_FWD_MEDIA wxAVMediaBackend;

static void *AVSPPlayerItemStatusContext = &AVSPPlayerItemStatusContext;
static void *AVSPPlayerRateContext = &AVSPPlayerRateContext;

@interface wxAVPlayer : AVPlayer {

    AVPlayerLayer *playerLayer;

    wxAVMediaBackend* m_backend;
}

-(BOOL)isPlaying;

@property (retain) AVPlayerLayer *playerLayer;

@end

class WXDLLIMPEXP_MEDIA wxAVMediaBackend : public wxMediaBackendCommonBase
{
public:

    wxAVMediaBackend();
    ~wxAVMediaBackend();

    virtual bool CreateControl(wxControl* ctrl, wxWindow* parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxValidator& validator,
                               const wxString& name) wxOVERRIDE;

    virtual bool Play() wxOVERRIDE;
    virtual bool Pause() wxOVERRIDE;
    virtual bool Stop() wxOVERRIDE;

    virtual bool Load(const wxString& fileName) wxOVERRIDE;
    virtual bool Load(const wxURI& location) wxOVERRIDE;

    virtual wxMediaState GetState() wxOVERRIDE;

    virtual bool SetPosition(wxLongLong where) wxOVERRIDE;
    virtual wxLongLong GetPosition() wxOVERRIDE;
    virtual wxLongLong GetDuration() wxOVERRIDE;

    virtual void Move(int x, int y, int w, int h) wxOVERRIDE;
    wxSize GetVideoSize() const wxOVERRIDE;

    virtual double GetPlaybackRate() wxOVERRIDE;
    virtual bool SetPlaybackRate(double dRate) wxOVERRIDE;

    virtual double GetVolume() wxOVERRIDE;
    virtual bool SetVolume(double dVolume) wxOVERRIDE;

    void Cleanup();
    void FinishLoad();

    virtual bool   ShowPlayerControls(wxMediaCtrlPlayerControls flags) wxOVERRIDE;
private:
    void DoShowPlayerControls(wxMediaCtrlPlayerControls flags);

    wxSize m_bestSize;              //Original movie size
    wxAVPlayer* m_player;               //AVPlayer handle/instance

    wxMediaCtrlPlayerControls m_interfaceflags; // Saved interface flags

    wxDECLARE_DYNAMIC_CLASS(wxAVMediaBackend);
};

// --------------------------------------------------------------------------
// wxAVMediaBackend
// --------------------------------------------------------------------------

@implementation wxAVPlayer

@synthesize playerLayer;

- (id) init
{
    self = [super init];

    [self addObserver:self forKeyPath:@"currentItem.status"
                  options:NSKeyValueObservingOptionNew context:AVSPPlayerItemStatusContext];
    [self addObserver:self forKeyPath:@"rate"
              options:NSKeyValueObservingOptionNew context:AVSPPlayerRateContext];

    return self;
}

- (void)dealloc
{
	[playerLayer release];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [self removeObserver:self forKeyPath:@"rate" context:AVSPPlayerRateContext];
	[self removeObserver:self forKeyPath:@"currentItem.status" context:AVSPPlayerItemStatusContext];

	[super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
	if (context == AVSPPlayerItemStatusContext)
	{
        id val = [change objectForKey:NSKeyValueChangeNewKey];
        if ( val != [NSNull null ] )
        {
            AVPlayerStatus status = [ val integerValue];

            switch (status)
            {
                case AVPlayerItemStatusUnknown:
                    break;
                case AVPlayerItemStatusReadyToPlay:
                    [[NSNotificationCenter defaultCenter]
                     addObserver:self selector:@selector(playerItemDidReachEnd:)
                     name:AVPlayerItemDidPlayToEndTimeNotification object:self.currentItem];
                    m_backend->FinishLoad();
                    break;
                case AVPlayerItemStatusFailed:
                    break;
                default:
                    break;
            }
        }
	}
	else if (context == AVSPPlayerRateContext)
	{
		NSNumber* newRate = [change objectForKey:NSKeyValueChangeNewKey];
        if ([newRate intValue] == 0)
        {
            m_backend->QueuePauseEvent();
        }
        else if ( [self isPlaying] == NO )
        {
            m_backend->QueuePlayEvent();
        }
	}
	else
	{
		[super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
	}
}

-(wxAVMediaBackend*) backend
{
    return m_backend;
}

-(void) setBackend:(wxAVMediaBackend*) backend
{
    m_backend = backend;
}

- (void)playerItemDidReachEnd:(NSNotification *)notification
{
    if ( m_backend )
    {
        if ( m_backend->SendStopEvent() )
            m_backend->QueueFinishEvent();
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

#if wxOSX_USE_IPHONE

@interface wxAVView : UIView
{
}

@end

@implementation wxAVView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXIPhoneClassAddWXMethods( self );
    }
}

+ (Class)layerClass
{
	return [AVPlayerLayer class];
}

- (id) initWithFrame:(CGRect)rect player:(wxAVPlayer*) player
{
    if ( !(self=[super initWithFrame:rect]) )
        return nil;

    AVPlayerLayer* playerLayer = (AVPlayerLayer*) [self layer];
    [playerLayer setPlayer: player];
    [player setPlayerLayer:playerLayer];

    return self;
}

- (AVPlayerLayer*) playerLayer
{
    return (AVPlayerLayer*) [self layer];
}
@end

#else

#if wxOSX_USE_AVKIT

@interface wxAVPlayerView : AVPlayerView
{
}

@end

@implementation wxAVPlayerView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id) initWithFrame:(NSRect)rect player:(wxAVPlayer*) player
{
    if ( !(self=[super initWithFrame:rect]) )
        return nil;

    self.player = player;

    return self;
}

- (AVPlayerLayer*) playerLayer
{
    return (AVPlayerLayer*) [[[self layer] sublayers] firstObject];
}

@end

#endif // wxOSX_USE_AVKIT

@interface wxAVView : NSView
{
}

@end

@implementation wxAVView

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id) initWithFrame:(NSRect)rect player:(AVPlayer*) player
{
    if ( !(self=[super initWithFrame:rect]) )
        return nil;

    [self setWantsLayer:YES];
    AVPlayerLayer* playerlayer = [[AVPlayerLayer playerLayerWithPlayer: player] retain];
    [player setPlayerLayer:playerlayer];

    [playerlayer setFrame:[[self layer] bounds]];
    [playerlayer setAutoresizingMask:kCALayerWidthSizable | kCALayerHeightSizable];
    [[self layer] addSublayer:playerlayer];

    return self;
}

- (AVPlayerLayer*) playerLayer
{
    return (AVPlayerLayer*) [[[self layer] sublayers] firstObject];
}

@end

#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxAVMediaBackend, wxMediaBackend);

wxAVMediaBackend::wxAVMediaBackend() :
m_player(nil),
m_interfaceflags(wxMEDIACTRLPLAYERCONTROLS_NONE)
{
}

wxAVMediaBackend::~wxAVMediaBackend()
{
    Cleanup();
}

bool wxAVMediaBackend::CreateControl(wxControl* inctrl, wxWindow* parent,
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

    m_player = [[wxAVPlayer alloc] init];
    [m_player setBackend:this];

    WXRect r = wxOSXGetFrameForControl( mediactrl, pos , size ) ;

    WXWidget view = NULL;
#if wxOSX_USE_AVKIT
    if ( NSClassFromString(@"AVPlayerView") )
    {
        view = [[wxAVPlayerView alloc] initWithFrame: r player:m_player];
        [(wxAVPlayerView*) view setControlsStyle:AVPlayerViewControlsStyleNone];
    }
#endif

    if ( view == NULL )
    {
        view = [[wxAVView alloc] initWithFrame: r player:m_player];
    }

#if wxOSX_USE_IPHONE
    wxWidgetIPhoneImpl* impl = new wxWidgetIPhoneImpl(mediactrl,view);
#else
    wxWidgetCocoaImpl* impl = new wxWidgetCocoaImpl(mediactrl,view);
#endif
    mediactrl->SetPeer(impl);

    m_ctrl = mediactrl;
    return true;
}

bool wxAVMediaBackend::Load(const wxString& fileName)
{
    return Load(
                wxURI(
                      wxString( wxT("file://") ) + fileName
                      )
                );
}

bool wxAVMediaBackend::Load(const wxURI& location)
{
    wxCFStringRef uri(location.BuildURI());
    NSURL *url = [NSURL URLWithString: uri.AsNSString()];

    AVAsset* asset = [AVAsset assetWithURL:url];
    if (! asset )
        return false;

    if ( [asset isPlayable] )
    {
        AVPlayerItem *playerItem = [AVPlayerItem playerItemWithAsset:asset];
        [m_player replaceCurrentItemWithPlayerItem:playerItem];

        return playerItem != nil;
    }
    return false;
}

void wxAVMediaBackend::FinishLoad()
{
    DoShowPlayerControls(m_interfaceflags);

    AVPlayerItem *playerItem = [m_player currentItem];

    CGSize s = [playerItem presentationSize];
    m_bestSize = wxSize(s.width, s.height);

    NotifyMovieLoaded();
}

bool wxAVMediaBackend::Play()
{
    [m_player play];
    return true;
}

bool wxAVMediaBackend::Pause()
{
    [m_player pause];
    return true;
}

bool wxAVMediaBackend::Stop()
{
    [m_player pause];
    [m_player seekToTime:kCMTimeZero];
    return true;
}

double wxAVMediaBackend::GetVolume()
{
    return [m_player volume];
}

bool wxAVMediaBackend::SetVolume(double dVolume)
{
    [m_player setVolume:dVolume];
    return true;
}
double wxAVMediaBackend::GetPlaybackRate()
{
    return [m_player rate];
}

bool wxAVMediaBackend::SetPlaybackRate(double dRate)
{
    [m_player setRate:dRate];
    return true;
}

bool wxAVMediaBackend::SetPosition(wxLongLong where)
{
	[m_player seekToTime:CMTimeMakeWithSeconds(where.GetValue() / 1000.0, 1)
              toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];

    return true;
}

wxLongLong wxAVMediaBackend::GetPosition()
{
    return CMTimeGetSeconds([m_player currentTime])*1000.0;
}

wxLongLong wxAVMediaBackend::GetDuration()
{
    AVPlayerItem *playerItem = [m_player currentItem];

	if ([playerItem status] == AVPlayerItemStatusReadyToPlay)
		return CMTimeGetSeconds([[playerItem asset] duration])*1000.0;
	else
		return 0.f;
}

wxMediaState wxAVMediaBackend::GetState()
{
    if ( [m_player isPlaying] )
        return wxMEDIASTATE_PLAYING;
    else
    {
        if ( GetPosition() == 0 )
            return wxMEDIASTATE_STOPPED;
        else
            return wxMEDIASTATE_PAUSED;
    }
}

void wxAVMediaBackend::Cleanup()
{
    [m_player pause];
    [m_player release];
    m_player = nil;
}

wxSize wxAVMediaBackend::GetVideoSize() const
{
    return m_bestSize;
}

void wxAVMediaBackend::Move(int x, int y, int w, int h)
{
    // as we have a native player, no need to move the video area
}

bool wxAVMediaBackend::ShowPlayerControls(wxMediaCtrlPlayerControls flags)
{
    if ( m_interfaceflags != flags )
        DoShowPlayerControls(flags);

    m_interfaceflags = flags;
    return true;
}

void wxAVMediaBackend::DoShowPlayerControls(wxMediaCtrlPlayerControls flags)
{
#if wxOSX_USE_AVKIT
    NSView* view = m_ctrl->GetHandle();
    if ( [view isKindOfClass:[wxAVPlayerView class]] )
    {
        wxAVPlayerView* playerView = (wxAVPlayerView*) view;
        if (flags == wxMEDIACTRLPLAYERCONTROLS_NONE )
            playerView.controlsStyle = AVPlayerViewControlsStyleNone;
        else
            playerView.controlsStyle = AVPlayerViewControlsStyleDefault;
    }
#endif
}

#endif

//in source file that contains stuff you don't directly use
#include "wx/html/forcelnk.h"
FORCE_LINK_ME(basewxmediabackends);

#endif //wxUSE_MEDIACTRL
