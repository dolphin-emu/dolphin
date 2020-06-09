// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#import "SoftwareVerifyProblem.h"

NS_ASSUME_NONNULL_BEGIN

// Wrapper object for DiscIO::VolumeVerifier::Result data
@interface SoftwareVerifyResult : NSObject

@property(nonatomic) NSString* m_crc32;
@property(nonatomic) NSString* m_md5;
@property(nonatomic) NSString* m_sha1;
@property(nonatomic) NSString* m_redump_status;
@property(nonatomic) NSArray<SoftwareVerifyProblem*>* m_problems;
@property(nonatomic) NSString* m_summary;

@end

NS_ASSUME_NONNULL_END
