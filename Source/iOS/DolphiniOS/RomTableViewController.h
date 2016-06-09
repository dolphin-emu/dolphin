//
//  iNDSMasterViewController.h
//  iNDS
//
//  Created by iNDS on 6/9/13.
//  Copyright (c) 2014 iNDS. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface RomTableViewController : UITableViewController <UIAlertViewDelegate>
{
    NSArray *games;
}

- (void)reloadGames:(id)sender;

@end
