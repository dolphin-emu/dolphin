// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Cocoa/Cocoa.h>

@interface ViewController : NSViewController

@property(assign) IBOutlet NSProgressIndicator* progressCurrent;
@property(assign) IBOutlet NSProgressIndicator* progressTotal;
@property(assign) IBOutlet NSTextField* labelProgress;

- (void)SetDescription:(NSString*)string;

- (void)SetTotalMarquee:(bool)marquee;
- (void)SetCurrentMarquee:(bool)marquee;

- (void)SetTotalProgress:(double)current total:(double)total;
- (void)SetCurrentProgress:(double)current total:(double)total;

@end
