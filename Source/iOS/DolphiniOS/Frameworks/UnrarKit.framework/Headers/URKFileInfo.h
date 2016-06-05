//
//  URKFileInfo.h
//  UnrarKit
//

#import <Foundation/Foundation.h>
#import "raros.hpp"
#import "dll.hpp"

/* See http://www.forensicswiki.org/wiki/RAR and
   http://www.rarlab.com/technote.htm#filehead for
   more information about the RAR File Header spec */

/**
 *  Defines the packing methods that can be used on a file in an archive
 */
typedef NS_ENUM(NSUInteger, URKCompressionMethod) {
    
    /**
     *  No compression is used
     */
    URKCompressionMethodStorage = 0x30,
    
    /**
     *  Fastest compression
     */
    URKCompressionMethodFastest = 0x31,
    
    /**
     *  Fast compression
     */
    URKCompressionMethodFast = 0x32,
    
    /**
     *  Normal compression
     */
    URKCompressionMethodNormal = 0x33,
    
    /**
     *  Good compression
     */
    URKCompressionMethodGood = 0x34,
    
    /**
     *  Best compression
     */
    URKCompressionMethodBest = 0x35,
};

/**
 *  Defines the various operating systems that can be used when archiving
 */
typedef NS_ENUM(NSUInteger, URKHostOS) {
    
    /**
     *  MS-DOS
     */
    URKHostOSMSDOS = 0,
    
    /**
     *  OS/2
     */
    URKHostOSOS2 = 1,
    
    /**
     *  Windows
     */
    URKHostOSWindows = 2,
    
    /**
     *  Unix
     */
    URKHostOSUnix = 3,
    
    /**
     *  Mac OS
     */
    URKHostOSMacOS = 4,
    
    /**
     *  BeOS
     */
    URKHostOSBeOS = 5,
};

/**
 *  A wrapper around a RAR archive's file header, defining the various fields
 *  it contains
 */
@interface URKFileInfo : NSObject

/**
 *  The name of the file's archive
 */
@property (readonly, strong) NSString *archiveName;

/**
 *  The name of the file
 */
@property (readonly, strong) NSString *filename;

/**
 *  The timestamp of the file
 */
@property (readonly, strong) NSDate *timestamp;

/**
 *  The CRC checksum of the file
 */
@property (readonly, assign) NSUInteger CRC;

/**
 *  Size of the uncompressed file
 */
@property (readonly, assign) long long uncompressedSize;

/**
 *  Size of the compressed file
 */
@property (readonly, assign) long long compressedSize;

/**
 *  YES if the file will be continued of the next volume
 */
@property (readonly) BOOL isEncryptedWithPassword;

/**
 *  YES if the file is a directory
 */
@property (readonly) BOOL isDirectory;

/**
 *  The type of compression
 */
@property (readonly, assign) URKCompressionMethod compressionMethod;

/**
 *  The OS of the file
 */
@property (readonly, assign) URKHostOS hostOS;

/**
 *  Returns a URKFileInfo instance for the given extended header data
 *
 *  @param fileHeader The header data for a RAR file
 *
 *  @return an instance of URKFileInfo
 */
+ (instancetype) fileInfo:(struct RARHeaderDataEx *)fileHeader;

@end
