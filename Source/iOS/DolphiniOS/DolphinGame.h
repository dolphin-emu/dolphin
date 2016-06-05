//
//  DolphinGame.h
//  DolphiniOS
//
//  Created by Will Cobb on 5/31/16.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

FOUNDATION_EXPORT NSString * const DolphinGameSaveStatesChangedNotification;

@interface DolphinGame : NSObject

@property (strong, nonatomic) NSString *path;
@property (nonatomic, readonly) NSString *title;
@property (nonatomic, readonly) NSString *rawTitle;
@property (nonatomic, readonly) NSString *gameTitle;
@property (nonatomic, readonly) UIImage *icon;
@property (nonatomic, readonly) NSInteger numberOfSaveStates;
@property (strong, nonatomic) NSString *pathForSavedStates;
@property (nonatomic, readonly) BOOL hasPauseState;

+ (int)preferredLanguage; // returns a NDS_FW_LANG_ constant
+ (NSArray*)gamesAtPath:(NSString*)path saveStateDirectoryPath:(NSString*)saveStatePath;
+ (DolphinGame*)gameWithPath:(NSString*)path saveStateDirectoryPath:(NSString*)saveStatePath;
- (DolphinGame*)initWithPath:(NSString*)path saveStateDirectoryPath:(NSString*)saveStatePath;
- (NSString*)pathForSaveStateWithName:(NSString*)name;
- (NSString*)pathForSaveStateAtIndex:(NSInteger)idx;
- (NSString*)nameOfSaveStateAtIndex:(NSInteger)idx;
- (NSString*)nameOfSaveStateAtPath:(NSString*)path;
- (NSDate*)dateOfSaveStateAtIndex:(NSInteger)idx;
- (BOOL)deleteSaveStateAtIndex:(NSInteger)idx;
- (BOOL)deleteSaveStateWithName:(NSString *)name;
- (void)reloadSaveStates;
- (NSArray*)saveStates;

@end
