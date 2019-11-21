// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface EmulationViewController : UIViewController

@property(nonatomic) NSString* videoBackend;
@property(nonatomic) NSString* targetFile;
@property(nonatomic) UIView* padView;

- (id)initWithFile:(NSString*)file videoBackend:(NSString*)videoBackend;
- (void)loadView;

@end

NS_ASSUME_NONNULL_END
