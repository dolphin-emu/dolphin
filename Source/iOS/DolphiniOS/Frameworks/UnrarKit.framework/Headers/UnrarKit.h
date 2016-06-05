//
//  UnrarKit.h
//  UnrarKit
//
//  Created by Dov Frankel on 1/9/2015.
//  Copyright (c) 2015 Abbey Code. All rights reserved.
//

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

//! Project version number for UnrarKit.
FOUNDATION_EXPORT double UnrarKitVersionNumber;

//! Project version string for UnrarKit.
FOUNDATION_EXPORT const unsigned char UnrarKitVersionString[];


#import "URKArchive.h"
#import "URKFileInfo.h"
