//
//  ViewController.h
//  DolphiniOS
//
//  Created by Will Cobb on 5/20/16.
//
//

#import <UIKit/UIKit.h>
@class GLKView;
@class DolphinGame;
@interface EmulatorViewController : UIViewController

@property IBOutlet GLKView *glkView;

- (void)launchGame:(DolphinGame *)game;

@end

