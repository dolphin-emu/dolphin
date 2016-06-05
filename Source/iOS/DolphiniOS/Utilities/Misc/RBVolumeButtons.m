//
//  RBVolumeButtons.m
//  VolumeSnap
//
//  Created by Randall Brown on 11/17/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#import "RBVolumeButtons.h"
#import <AudioToolbox/AudioToolbox.h>
#import <MediaPlayer/MediaPlayer.h>

@interface RBVolumeButtons()

@property (nonatomic) BOOL isStealingVolumeButtons;
@property (nonatomic) BOOL hadToLowerVolume;
@property (nonatomic) BOOL hadToRaiseVolume;
@property (nonatomic) BOOL suspended;
@property (nonatomic, readwrite) float launchVolume;
@property (nonatomic, retain) UIView *volumeView;

@end

@implementation RBVolumeButtons

static void volumeListenerCallback (
                                    void                      *inClientData,
                                    AudioSessionPropertyID    inID,
                                    UInt32                    inDataSize,
                                    const void                *inData
                                    ){
    const float *volumePointer = inData;
    float volume = *volumePointer;

    if (volume > [(RBVolumeButtons*)inClientData launchVolume])
    {
        [(RBVolumeButtons*)inClientData volumeUp];
    }
    else if (volume < [(RBVolumeButtons*)inClientData launchVolume])
    {
        [(RBVolumeButtons*)inClientData volumeDown];
    }
}

- (void)volumeDown
{
    AudioSessionRemovePropertyListenerWithUserData(kAudioSessionProperty_CurrentHardwareOutputVolume, volumeListenerCallback, self);

    [[MPMusicPlayerController applicationMusicPlayer] setVolume:self.launchVolume];

    [self performSelector:@selector(initializeVolumeButtonStealer) withObject:self afterDelay:0.1];

    if (self.downBlock)
    {
        self.downBlock();
    }
}

- (void)volumeUp
{
    AudioSessionRemovePropertyListenerWithUserData(kAudioSessionProperty_CurrentHardwareOutputVolume, volumeListenerCallback, self);

    [[MPMusicPlayerController applicationMusicPlayer] setVolume:self.launchVolume];

    [self performSelector:@selector(initializeVolumeButtonStealer) withObject:self afterDelay:0.1];

    if (self.upBlock)
    {
        self.upBlock();
    }
}

- (void)startStealingVolumeButtonEvents
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAssert([[NSThread currentThread] isMainThread], @"This must be called from the main thread");

        if (self.isStealingVolumeButtons)
        {
            return;
        }

        self.isStealingVolumeButtons = YES;

        AudioSessionInitialize(NULL, NULL, NULL, NULL);

        //const UInt32 sessionCategory = kAudioSessionCategory_PlayAndRecord;
        //AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory), &sessionCategory);

        AudioSessionSetActive(YES);

        self.launchVolume = [[MPMusicPlayerController applicationMusicPlayer] volume];
        self.hadToLowerVolume = self.launchVolume == 1.0;
        self.hadToRaiseVolume = self.launchVolume == 0.0;

        // Avoid flashing the volume indicator
        if (self.hadToLowerVolume || self.hadToRaiseVolume)
        {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (self.hadToLowerVolume)
                {
                    [[MPMusicPlayerController applicationMusicPlayer] setVolume:0.95];
                    self.launchVolume = 0.95;
                }

                if (self.hadToRaiseVolume)
                {
                    [[MPMusicPlayerController applicationMusicPlayer] setVolume:0.05];
                    self.launchVolume = 0.05;
                }
            });
        }

        CGRect frame = CGRectMake(0, -100, 10, 0);
        self.volumeView = [[[MPVolumeView alloc] initWithFrame:frame] autorelease];
        [self.volumeView sizeToFit];
        [[[[UIApplication sharedApplication] windows] firstObject] insertSubview:self.volumeView atIndex:0];

        [self initializeVolumeButtonStealer];

        if (!self.suspended)
        {
            // Observe notifications that trigger suspend
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(suspendStealingVolumeButtonEvents:)
                                                         name:UIApplicationWillResignActiveNotification     // -> Inactive
                                                       object:nil];

            // Observe notifications that trigger resume
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(resumeStealingVolumeButtonEvents:)
                                                         name:UIApplicationDidBecomeActiveNotification      // <- Active
                                                       object:nil];
        }
    });
}

- (void)suspendStealingVolumeButtonEvents:(NSNotification *)notification
{
    if (self.isStealingVolumeButtons)
    {
        self.suspended = YES; // Call first!
        [self stopStealingVolumeButtonEvents];
    }
}

- (void)resumeStealingVolumeButtonEvents:(NSNotification *)notification
{
    if (self.suspended)
    {
        [self startStealingVolumeButtonEvents];
        self.suspended = NO; // Call last!
    }
}

- (void)stopStealingVolumeButtonEvents
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAssert([[NSThread currentThread] isMainThread], @"This must be called from the main thread");

        if (!self.isStealingVolumeButtons)
        {
            return;
        }

        // Stop observing all notifications
        if (!self.suspended)
        {
            [[NSNotificationCenter defaultCenter] removeObserver:self];
        }

        AudioSessionRemovePropertyListenerWithUserData(kAudioSessionProperty_CurrentHardwareOutputVolume, volumeListenerCallback, self);

        if (self.hadToLowerVolume)
        {
            [[MPMusicPlayerController applicationMusicPlayer] setVolume:1.0];
        }

        if (self.hadToRaiseVolume)
        {
            [[MPMusicPlayerController applicationMusicPlayer] setVolume:0.0];
        }

        [self.volumeView removeFromSuperview];
        self.volumeView = nil;

        //AudioSessionSetActive(NO);

        self.isStealingVolumeButtons = NO;
    });
}

- (void)dealloc
{
    self.suspended = NO;
    [self stopStealingVolumeButtonEvents];

    self.upBlock = nil;
    self.downBlock = nil;
    [super dealloc];
}

- (void)initializeVolumeButtonStealer
{
    AudioSessionAddPropertyListener(kAudioSessionProperty_CurrentHardwareOutputVolume, volumeListenerCallback, self);
}

@end
