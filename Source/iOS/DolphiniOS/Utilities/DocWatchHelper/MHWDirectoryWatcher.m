/*
 *  MHWDirectoryWatcher.h
 *  Created by Martin Hwasser on 12/19/11.
 */

#import "MHWDirectoryWatcher.h"

#define kMHWDirectoryWatcherPollInterval 0.2
#define kMHWDirectoryWatcherPollRetryCount 5

typedef void (^MHWDirectoryWatcherCallback)(void);

@interface MHWDirectoryWatcher ()

@property (nonatomic) dispatch_source_t source;
@property (nonatomic) dispatch_queue_t queue;
@property (nonatomic) NSInteger retriesLeft;
@property (nonatomic, getter = isDirectoryChanging) BOOL directoryChanging;
@property (nonatomic, readwrite, copy) NSString *watchedPath;
@property (nonatomic, copy) MHWDirectoryWatcherCallback callback;

@end

@implementation MHWDirectoryWatcher

- (void)dealloc
{
    [self stopWatching];
    _callback = nil;
}

- (id)initWithPath:(NSString *)path
{
    self = [super init];
    if (self) {
        _watchedPath = [path copy];
        _queue = dispatch_queue_create("MHWDirectoryWatcherQueue", 0);
    }
    return self;
}

+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchedPath
                               startImmediately:(BOOL)startImmediately
                                       callback:(void(^)())cb
{
    NSAssert(watchedPath != nil, @"The directory to watch must not be nil");
    MHWDirectoryWatcher *directoryWatcher = [[MHWDirectoryWatcher alloc] initWithPath:watchedPath];
    directoryWatcher.callback = cb;
    
    if (startImmediately) {
        if (![directoryWatcher startWatching]) {
            // Something went wrong, return nil
            return nil;
        }
    }
    return directoryWatcher;
}

+ (MHWDirectoryWatcher *)directoryWatcherAtPath:(NSString *)watchPath
                                       callback:(void(^)())cb
{
    return [MHWDirectoryWatcher directoryWatcherAtPath:watchPath
                                      startImmediately:YES
                                              callback:cb];
}
#pragma mark -
#pragma mark - Public methods

- (BOOL)stopWatching
{
    if (_source != NULL) {
        dispatch_source_cancel(_source);
        _source = NULL;
        return YES;
    }
    return NO;
}

- (BOOL)startWatching
{
    // Already monitoring
    if (self.source != NULL) {
        return NO;
    }
    
    // Open an event-only file descriptor associated with the directory
    int fd = open([self.watchedPath fileSystemRepresentation], O_EVTONLY);
    if (fd < 0) {
        return NO;
    }
    
    void (^cleanup)() = ^{
        close(fd);
    };
    
    // Get a low priority queue
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    
    // Monitor the directory for writes
    self.source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, // Monitors a file descriptor
                                         fd, // our file descriptor
                                         DISPATCH_VNODE_WRITE, // The file-system object data changed.
                                         queue); // the queue to dispatch on
    
    if (!_source) {
        cleanup();
        return NO;
    }
    
    __weak __typeof__(self) _weakSelf = self;
    // Call directoryDidChange on event callback
    dispatch_source_set_event_handler(self.source, ^{
        [_weakSelf directoryDidChange];
    });
    
    // Dispatch source destructor
    dispatch_source_set_cancel_handler(self.source, cleanup);
    
    // Sources are create in suspended state, so resume it
    dispatch_resume(self.source);
    
    // Everything was OK
    return YES;
}


#pragma mark -
#pragma mark - Private methods

- (NSArray *)directoryMetadata
{
    NSFileManager *fileManager = [[NSFileManager alloc] init];
    NSArray *contents = [fileManager contentsOfDirectoryAtPath:self.watchedPath
                                                         error:NULL];
    
    NSMutableArray *directoryMetadata = [NSMutableArray array];
    
    for (NSString *fileName in contents) {
        @autoreleasepool {
            NSString *filePath = [self.watchedPath stringByAppendingPathComponent:fileName];
            NSDictionary *fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:filePath
                                                                                            error:nil];
            NSInteger fileSize = [[fileAttributes objectForKey:NSFileSize] intValue];
            // The fileName and fileSize will be our hash key
            NSString *fileHash = [NSString stringWithFormat:@"%@%ld", fileName, (long)fileSize];
            // Add it to our metadata list
            [directoryMetadata addObject:fileHash];
        }
    }
    return directoryMetadata;
}

- (void)checkChangesAfterDelay:(NSTimeInterval)timeInterval
{
    NSArray *directoryMetadata = [self directoryMetadata];

    __weak __typeof__(self) _weakSelf = self;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, timeInterval * NSEC_PER_SEC);
    dispatch_after(popTime, self.queue, ^(void){
        [_weakSelf pollDirectoryForChangesWithMetadata:directoryMetadata];
    });
}

- (void)pollDirectoryForChangesWithMetadata:(NSArray *)oldDirectoryMetadata
{
    NSArray *newDirectoryMetadata = [self directoryMetadata];
    
    // Check if metadata has changed
    self.directoryChanging = ![newDirectoryMetadata isEqualToArray:oldDirectoryMetadata];
    
    // Reset retries if it's still changing
    self.retriesLeft = self.isDirectoryChanging ? kMHWDirectoryWatcherPollRetryCount : self.retriesLeft;
    
    if (self.isDirectoryChanging || 0 < self.retriesLeft--) {
        // Either the directory is changing or
        // we should try again as more changes may occur
        [self checkChangesAfterDelay:kMHWDirectoryWatcherPollInterval];
    } else {
        // Changes appear to be completed
        // Post a notification informing that the directory did change
        dispatch_async(dispatch_get_main_queue(), ^{
            self.callback();
        });
    }
}

- (void)directoryDidChange
{
    if (!self.isDirectoryChanging) {
        // Changes just occurred
        self.directoryChanging = YES;
        self.retriesLeft = kMHWDirectoryWatcherPollRetryCount;
        
        [self checkChangesAfterDelay:kMHWDirectoryWatcherPollInterval];
    }
}

@end
