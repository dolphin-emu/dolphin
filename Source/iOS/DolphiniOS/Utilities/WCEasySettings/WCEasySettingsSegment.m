//
//  WCEasySettingsSegment.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsSegment.h"

@implementation WCEasySettingsSegment

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title items:(NSArray *)items
{
    if (self = [super initWithIdentifier:identifier title:title]) {
        self.items = items;
        self.cellIdentifier = @"Segment";
        self.type = WCEasySettingsTypeSegment;
    }
    return self;
}

- (void)onSegment:(UISegmentedControl *)s
{
    [[NSUserDefaults standardUserDefaults] setInteger:s.selectedSegmentIndex forKey:self.identifier];
}

@end

@implementation WCEasySettingsSegmentCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
        cellSegment = [[UISegmentedControl alloc] initWithFrame:CGRectMake(20, 36, self.frame.size.width - 40, 29)];
        [self.contentView addSubview:cellSegment];
        
        cellTitle = [[UILabel alloc] initWithFrame:CGRectMake(0, 7, self.frame.size.width, 21)];
        cellTitle.textAlignment = NSTextAlignmentCenter;
        [self.contentView addSubview:cellTitle];
    }
    return self;
}

- (void)setFrame:(CGRect)frame
{
    [super setFrame:frame];
    //For some reason the segment view like to animate from a width of 0. This is a workaround
    cellSegment.frame = CGRectMake(20, 36, frame.size.width - 40, 29);
    
    cellTitle.frame = CGRectMake(0, 7, frame.size.width, 21);
}

- (void)setController:(WCEasySettingsSegment *)controller
{
    cellTitle.text = controller.title;
   
    cellSegment.hidden = YES;
    [cellSegment removeAllSegments];
    for (NSString *item in controller.items.reverseObjectEnumerator) {
        [cellSegment insertSegmentWithTitle:item atIndex:0 animated:NO];
    }
    
    [cellSegment removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
    [cellSegment addTarget:controller action:@selector(onSegment:) forControlEvents:UIControlEventValueChanged];
    cellSegment.selectedSegmentIndex = MAX([[NSUserDefaults standardUserDefaults] integerForKey:controller.identifier], 0);
    cellSegment.hidden = NO;
    
    self.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

@end

