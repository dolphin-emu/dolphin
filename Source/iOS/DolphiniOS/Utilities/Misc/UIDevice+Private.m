//
//  UITapticEngine.m
//  ViewControllerPreview
//
//  Created by Dal Rupnik on 28/09/15.
//  Copyright © 2015 Apple Inc. All rights reserved.
//

#import "UIDevice+Private.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincomplete-implementation"

@implementation UIDevice (Private)

- (UITapticEngine *)tapticEngine
{
    return [self _tapticEngine];
}

@end

#pragma clang diagnostic pop
