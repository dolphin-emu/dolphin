//
//  LZMAExtractor.h
//  lzmaSDK
//
//  Created by Brian Chaikelson on 11/19/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface LZMAExtractor : NSObject {	
}

// Extract all the contents of a .7z archive directly into the indicated dir.
// Directory structure is ignored if preserveDir is false.

+ (NSArray*) extract7zArchive:(NSString*)archivePath
                      dirName:(NSString*)dirName
                  preserveDir:(BOOL)preserveDir;

// Extract all the contents of a .7z archive into the indicated temp dir
// and return an array of the fully qualified filenames. This API
// implicitly passes preserveDir as FALSE, so directory elements in the
// entry name are always ignored as files are extracted. If multiple
// entries have duplicated filenames in different directories, then
// this API would would overwrite the duplicate entry file.

+ (NSArray*) extract7zArchive:(NSString*)archivePath
                   tmpDirName:(NSString*)tmpDirName;

// Extract just one entry from an archive and save it at the
// path indicated by outPath.

+ (BOOL) extractArchiveEntry:(NSString*)archivePath
                archiveEntry:(NSString*)archiveEntry
                     outPath:(NSString*)outPath;

@end

