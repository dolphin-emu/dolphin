//
//  DolphinBridge.h
//  DolphiniOS
//
//  Created by mac on 2015-03-17.
//  Copyright (c) 2015 OatmealDome. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface DolphinBridge : NSObject

- (void)createUserFolders;
- (NSString*) getUserDirectory;
- (void) setUserDirectory: (NSString*)userDir;
- (NSString*) getLibraryDirectory;
- (void) copyResources;
- (void) copyDirectoryOrFile:(NSFileManager*)fileMgr :(NSString *)directory;
- (void) loadPreferences;
- (void) saveDefaultPreferences;
- (void) openRomAtPath:(NSString *)path;
- (void) redirectConsole;

@end