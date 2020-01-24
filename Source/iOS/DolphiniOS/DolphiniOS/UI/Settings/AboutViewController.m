// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AboutViewController.h"

@interface AboutViewController ()

@end

@implementation AboutViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  NSString* thanks_path = [[NSBundle mainBundle] pathForResource:@"SpecialThanks" ofType:@"txt"];
  NSString* thanks_str = [NSString stringWithContentsOfFile:thanks_path encoding:NSUTF8StringEncoding error:nil];
  NSArray* thanks_array = [thanks_str componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
  
  // Clear the placeholder text
  [self.m_patrons_one setText:@""];
  [self.m_patrons_two setText:@""];
  
  for (NSUInteger i = 0; i < [thanks_array count]; i++)
  {
    UILabel* target_label;
    if (i % 2 == 0)
    {
      target_label = self.m_patrons_one;
    }
    else
    {
      target_label = self.m_patrons_two;
    }
    
    NSString* new_text = [target_label.text stringByAppendingString:[NSString stringWithFormat:@"%@\n", [thanks_array objectAtIndex:i]]];
    [target_label setText:new_text];
  }
  
  // Flash the scroll indicators
  [self.m_scroll_view flashScrollIndicators];
}

- (IBAction)SourceCodePressed:(id)sender
{
  NSURL* url = [NSURL URLWithString:@"https://github.com/oatmealdome/dolphin/tree/ios-jb/"];
  [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
}

@end
