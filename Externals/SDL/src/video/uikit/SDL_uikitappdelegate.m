/*
 SDL - Simple DirectMedia Layer
 Copyright (C) 1997-2009 Sam Lantinga
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
 Sam Lantinga
 slouken@libsdl.org
*/

#import "SDL_uikitappdelegate.h"
#import "SDL_uikitopenglview.h"
#import "SDL_events_c.h"
#import "jumphack.h"

#ifdef main
#undef main
#endif

extern int SDL_main(int argc, char *argv[]);
static int forward_argc;
static char **forward_argv;

int main(int argc, char **argv) {

	int i;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	/* store arguments */
	forward_argc = argc;
	forward_argv = (char **)malloc(argc * sizeof(char *));
	for (i=0; i<argc; i++) {
		forward_argv[i] = malloc( (strlen(argv[i])+1) * sizeof(char));
		strcpy(forward_argv[i], argv[i]);
	}

	/* Give over control to run loop, SDLUIKitDelegate will handle most things from here */
	UIApplicationMain(argc, argv, NULL, @"SDLUIKitDelegate");
	
	[pool release];
	
}

@implementation SDLUIKitDelegate

@synthesize window;
@synthesize windowID;

/* convenience method */
+(SDLUIKitDelegate *)sharedAppDelegate {
	/* the delegate is set in UIApplicationMain(), which is garaunteed to be called before this method */
	return (SDLUIKitDelegate *)[[UIApplication sharedApplication] delegate];
}

- (id)init {
	self = [super init];
	window = nil;
	windowID = 0;
	return self;
}

- (void)postFinishLaunch {

	/* run the user's application, passing argc and argv */
	int exit_status = SDL_main(forward_argc, forward_argv);
	
	/* free the memory we used to hold copies of argc and argv */
	int i;
	for (i=0; i<forward_argc; i++) {
		free(forward_argv[i]);
	}
	free(forward_argv);	
		
	/* exit, passing the return status from the user's application */
	exit(exit_status);
}

- (void)applicationDidFinishLaunching:(UIApplication *)application {
			
	/* Set working directory to resource path */
	[[NSFileManager defaultManager] changeCurrentDirectoryPath: [[NSBundle mainBundle] resourcePath]];
	
	[self performSelector:@selector(postFinishLaunch) withObject:nil
afterDelay:0.0];
}

- (void)applicationWillTerminate:(UIApplication *)application {
	
	SDL_SendQuit();
	 /* hack to prevent automatic termination.  See SDL_uikitevents.m for details */
	longjmp(*(jump_env()), 1);
	
}

- (void) applicationWillResignActive:(UIApplication*)application
{
//	NSLog(@"%@", NSStringFromSelector(_cmd));
	SDL_SendWindowEvent(self.windowID, SDL_WINDOWEVENT_MINIMIZED, 0, 0);
}

- (void) applicationDidBecomeActive:(UIApplication*)application
{
//	NSLog(@"%@", NSStringFromSelector(_cmd));
	SDL_SendWindowEvent(self.windowID, SDL_WINDOWEVENT_RESTORED, 0, 0);
}



-(void)dealloc {
	[window release];
	[super dealloc];
}

@end
