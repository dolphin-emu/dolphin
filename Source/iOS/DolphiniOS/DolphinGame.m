//
//  DolphinGame.m
//  Dolphin
//
//  Created by Will Cobb on 5/31/2016.
//  Copyright (c) 2016 iNDS. All rights reserved.
//  http://problemkaputt.de/gbatek.htm

#import "DolphinGame.h"

NSString * const DolphinGameSaveStatesChangedNotification = @"DolphinGameSaveStatesChangedNotification";

@implementation DolphinGame
{
    NSArray     *saveStates;
    NSString    *rawTitle;
    NSData      *iconTitleData;
    NSString    *title;
    UIImage     *icon;
}

+ (NSArray*)gamesAtPath:(NSString*)gamesPath saveStateDirectoryPath:(NSString*)saveStatePath
{
    NSMutableArray *games = [NSMutableArray new];
    NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:gamesPath error:NULL];
    for(NSString *file in files) {
        if ([file.pathExtension isEqualToString:@"iso"] || [file.pathExtension isEqualToString:@"dol"]) {
            DolphinGame *game = [DolphinGame gameWithPath:[gamesPath stringByAppendingPathComponent:file] saveStateDirectoryPath:saveStatePath];
            if (game) [games addObject:game];
        } else { //Recurse
            BOOL isDirectory;
            if ([[NSFileManager defaultManager] fileExistsAtPath:[gamesPath stringByAppendingPathComponent:file] isDirectory:&isDirectory]) {
                if (isDirectory) {
                    [games addObjectsFromArray:[DolphinGame gamesAtPath:[gamesPath stringByAppendingPathComponent:file] saveStateDirectoryPath:saveStatePath]];
                }
            }
        }
    }
    
    return [NSArray arrayWithArray:games];
}

+ (DolphinGame*)gameWithPath:(NSString*)path saveStateDirectoryPath:(NSString*)saveStatePath
{
    return [[DolphinGame alloc] initWithPath:path saveStateDirectoryPath:saveStatePath];
}

- (DolphinGame*)initWithPath:(NSString*)path saveStateDirectoryPath:(NSString*)saveStatePath
{
    NSAssert(path.isAbsolutePath, @"DolphinGame path must be absolute");
    if (!([path.pathExtension.lowercaseString isEqualToString:@"iso"] || [path.pathExtension.lowercaseString isEqualToString:@"dol"])) return nil;
    if ([[path.lastPathComponent substringToIndex:1] isEqualToString:@"."]) return nil; // Hidden File
    
    // check file exists
    BOOL isDirectory = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDirectory] || isDirectory) return nil;
    
    if ((self = [super init])) {
        self.path = path;
        self.pathForSavedStates = saveStatePath;
        [self _loadSaveStates];
        [self _loadHeader];
    }
    return self;
}

- (void)_loadSaveStates
{
    // get save states (<ROM name without extension>.<save state name>.dsv)
    NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:self.pathForSavedStates error:NULL];
    NSString *savePrefix = [self.path.lastPathComponent.stringByDeletingPathExtension stringByAppendingString:@"."];
    
    saveStates = [files objectsAtIndexes:[files indexesOfObjectsPassingTest:^BOOL(NSString *filename, NSUInteger idx, BOOL *stop) {
        return ([filename.pathExtension isEqualToString:@"dsv"] && [filename.stringByDeletingPathExtension hasPrefix:savePrefix]);
    }]];
    
    NSFileManager *fm = [NSFileManager defaultManager];
    saveStates = [saveStates sortedArrayUsingComparator:^NSComparisonResult(NSString *s1, NSString *s2) {
        //if ([s1 hasSuffix:@".pause.dsv"]) return NSOrderedAscending;
        //if ([s2 hasSuffix:@".pause.dsv"]) return NSOrderedDescending;
        NSDate *date1 = [fm attributesOfItemAtPath:[self.pathForSavedStates stringByAppendingPathComponent:s1] error:NULL].fileModificationDate;
        NSDate *date2 = [fm attributesOfItemAtPath:[self.pathForSavedStates stringByAppendingPathComponent:s2] error:NULL].fileModificationDate;
        return [date2 compare:date1];
    }];
}

- (void)reloadSaveStates
{
    [self _loadSaveStates];
    [[NSNotificationCenter defaultCenter] postNotificationName:DolphinGameSaveStatesChangedNotification object:self userInfo:nil];
}

- (NSArray*)saveStates
{
    return saveStates;
}

- (NSString*)title
{
    return self.path.lastPathComponent.stringByDeletingPathExtension;
}
- (NSString*)rawTitle
{
    return rawTitle;
}

+ (int)preferredLanguage
{
    NSString *preferredLanguage = [[NSLocale preferredLanguages][0] substringToIndex:2];
    NSUInteger pos = [@[@"ja",@"en",@"fr",@"de",@"it",@"es",@"zh"] indexOfObject:preferredLanguage];
    if (pos == NSNotFound) return 1; // NDS_FW_LANG_ENG
    return (int)pos;
}

- (NSString*)gameTitle
{
    if (iconTitleData == nil) return nil;
    if (title == nil) {
        uint16_t version = OSReadLittleInt16(iconTitleData.bytes, 0x000);
        
        // find preferred language
        uint32_t titleOffset = 0x240 + 0x100 * [DolphinGame preferredLanguage];
        // version 1 doesn't have chinese, use english
        if ((titleOffset == 0x840 && version < 2)) titleOffset = 0x340;
        
        NSData *titleData = [iconTitleData subdataWithRange:NSMakeRange(titleOffset, 0x100)];
        title = [[NSString alloc] initWithData:titleData encoding:NSUTF16LittleEndianStringEncoding];
    }
    return title;
    return @"Not implemented";
}

- (UIImage*)icon
{
    if (iconTitleData == nil) return nil;
    if (icon == nil) {
        NSMutableData *bitmapData = [NSMutableData dataWithCapacity:32*32*4];
        
        // Setup the 4-bit CLUT.
        //
        // The actual color values are stored with the ROM icon data in RGB555 format.
        // We convert these color values and store them in the CLUT as RGBA8888 values.
        //
        // The first entry always represents the alpha, so we can just ignore it.
        uint32_t clut[16] = {0x00000000};
        for (int i = 1; i < 16; i++) {
            uint16_t color16 = OSReadLittleInt16(iconTitleData.bytes + 0x220, 2*i);
            clut[i] =   ((color16 & 0x001F) << 3) |
            ((color16 & 0x03E0) << 6) |
            ((color16 & 0x7C00) << 9) |
            0xFF000000;
        }
        
        // Load the image from the icon pixel data.
        //
        // ROM icons are stored in 4-bit indexed color and have dimensions of 32x32 pixels.
        // Also, ROM icons are split into 16 separate 8x8 pixel squares arranged in a 4x4
        // array. Here, we sequentially read from the ROM data, and adjust our write
        // location appropriately within the bitmap memory block.
        const uint32_t *iconPixPtr = iconTitleData.bytes + 0x20;
        uint32_t *bitmapPixPtr;
        uint32_t pixRowColors;
        unsigned int pixRowIndex;
        for(int y = 0; y < 4; y++) {
            for(int x = 0; x < 4; x++) {
                for(pixRowIndex = 0; pixRowIndex < 8; pixRowIndex++, iconPixPtr++) {
                    // Load the entire row of pixels as a single 32-bit chunk.
                    pixRowColors = OSReadLittleInt32(iconPixPtr, 0);
                    
                    // Set the write location.
                    bitmapPixPtr = bitmapData.mutableBytes + 4 * (( ((y * 8) + pixRowIndex) * 32 ) + (x * 8));
                    
                    // Set the RGBA8888 bitmap pixels using our CLUT from earlier.
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x0000000F)];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x000000F0) >> 4];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x00000F00) >> 8];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x0000F000) >> 12];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x000F0000) >> 16];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x00F00000) >> 20];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0x0F000000) >> 24];
                    *bitmapPixPtr++ = clut[(pixRowColors & 0xF0000000) >> 28];
                }
            }
        }
        
        // create image
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)(bitmapData));
        CGImageRef cgImage = CGImageCreate(32, 32, 8, 32, 32*4, space, kCGImageAlphaLast, provider, NULL, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
        CGColorSpaceRelease(space);
        
        icon = [UIImage imageWithCGImage:cgImage];
        CGImageRelease(cgImage);
    }
    return icon;
}

- (NSInteger)numberOfSaveStates
{
    return saveStates.count;
}

- (BOOL)deleteSaveStateWithName:(NSString *)name
{
    if (![[NSFileManager defaultManager] removeItemAtPath:[self pathForSaveStateWithName:name] error:NULL]) return NO;
    [self reloadSaveStates];
    return YES;
}

- (BOOL)deleteSaveStateAtIndex:(NSInteger)idx
{
    if (idx < 0 || idx >= saveStates.count) return NO;
    NSLog(@"Deleting: %@", [self pathForSaveStateAtIndex:idx]);
    if (![[NSFileManager defaultManager] removeItemAtPath:[self pathForSaveStateAtIndex:idx] error:NULL]) return NO;
    [self reloadSaveStates];
    return YES;
}

- (NSString*)pathForSaveStateWithName:(NSString*)name
{
    name = [NSString stringWithFormat:@"%@.%@.dsv", self.path.lastPathComponent.stringByDeletingPathExtension, name];
    return [self.pathForSavedStates stringByAppendingPathComponent:name];
}

- (NSString*)pathForSaveStateAtIndex:(NSInteger)idx
{
    if (idx < 0 || idx >= saveStates.count) return nil;
    return [self.pathForSavedStates stringByAppendingPathComponent:saveStates[idx]];
}

- (NSString*)nameOfSaveStateAtIndex:(NSInteger)idx
{
    if (idx < 0 || idx >= saveStates.count) return nil;
    return [[saveStates[idx] substringFromIndex:self.path.lastPathComponent.stringByDeletingPathExtension.length+1] stringByDeletingPathExtension];
}

- (NSString*)nameOfSaveStateAtPath:(NSString*)path
{
    return [path componentsSeparatedByString:@"."][1];
}

- (NSDate*)dateOfSaveStateAtIndex:(NSInteger)idx
{
    if (idx < 0 || idx >= saveStates.count) return nil;
    return [[NSFileManager defaultManager] attributesOfItemAtPath:[self pathForSaveStateAtIndex:idx] error:NULL].fileModificationDate;
}

- (BOOL)hasPauseState
{
    return [[self nameOfSaveStateAtIndex:0] isEqualToString:@"pause"];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<DolphinGame 0x%p: %@>", self, self.path];
}

- (void)_loadHeader
{
    NSFileHandle *fh = [NSFileHandle fileHandleForReadingAtPath:self.path];
    if (fh == nil) return;
    
    NSData *data = nil;
    @try {
        // read rawTitle
        [fh seekToFileOffset:0xC];
        data = [fh readDataOfLength:4];
        if (data.length != 4) return;
        rawTitle = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
        
        // read location of icon+title
        [fh seekToFileOffset:0x68];
        data = [fh readDataOfLength:4];
        if (data.length != 4) return;
        uint32_t iconOffset = OSReadLittleInt32(data.bytes, 0);
        if (iconOffset == 0) return; //No header
        
        // read icon+title data
        [fh seekToFileOffset:iconOffset];
        data = [fh readDataOfLength:0xA00];
        if (data.length != 0xA00) return;
        
        iconTitleData = data;
    }
    @catch (NSException *exception) {
        return;
    }
    @finally {
        [fh closeFile];
    }
}

@end
