// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DiscIO/VolumeVerifier.h"

#import "SoftwareVerifyResult.h"

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareVerifyComputeViewController : UIViewController

@property (weak, nonatomic) IBOutlet UIImageView* m_gear_image;
@property (weak, nonatomic) IBOutlet UIProgressView* m_progress_view;

@property(nonatomic) UICommon::GameFile* m_game_file;
@property(nonatomic) bool m_calc_crc32;
@property(nonatomic) bool m_calc_md5;
@property(nonatomic) bool m_calc_sha1;
@property(nonatomic) bool m_verify_redump;

@property(nonatomic) SoftwareVerifyResult* m_result;
@property(nonatomic) bool m_is_cancelled;

@end

NS_ASSUME_NONNULL_END
