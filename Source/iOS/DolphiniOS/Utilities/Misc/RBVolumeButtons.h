//
//  RBVolumeButtons.h
//  VolumeSnap
//
//  Created by Randall Brown on 11/17/11.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface RBVolumeButtons : NSObject

@property (nonatomic, copy) dispatch_block_t upBlock;
@property (nonatomic, copy) dispatch_block_t downBlock;
@property (nonatomic, readonly) float launchVolume;

- (void)startStealingVolumeButtonEvents;
- (void)stopStealingVolumeButtonEvents;

@end
