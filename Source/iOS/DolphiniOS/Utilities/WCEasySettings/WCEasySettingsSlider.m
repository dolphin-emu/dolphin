//
//  WCEasySettingsSlider.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsSlider.h"

@implementation WCEasySettingsSlider

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title
{
    if (self = [super initWithIdentifier:identifier title:title]) {
        self.cellIdentifier = @"Slider";
        self.type = WCEasySettingsTypeSlider;
    }
    return self;
}

- (void)configureCell:(UITableViewCell *)cell
{
    UILabel *cellTitle = [cell viewWithTag:1];
    if (!cellTitle) {
        cellTitle = [[UILabel alloc] initWithFrame:CGRectMake(0, 7, cell.frame.size.width, 21)];
        cellTitle.textAlignment = NSTextAlignmentCenter;
        [cell addSubview:cellTitle];
    }
    cellTitle.text = self.title;
    
    UISlider *cellSlider = [cell viewWithTag:2];
    if (!cellSlider) {
        cellSlider = [[UISlider alloc] initWithFrame:CGRectMake(20, 36, cell.frame.size.width - 40, 29)];
        cellSlider.tag = 2;
        [cell addSubview:cellSlider];
    }
    [cellSlider removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
    [cellSlider addTarget:self action:@selector(onSlide:) forControlEvents:UIControlEventValueChanged];
    cellSlider.value = [[NSUserDefaults standardUserDefaults] floatForKey:self.identifier];
    
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (void)onSlide:(UISlider *)s
{
    [[NSUserDefaults standardUserDefaults] setFloat:s.value forKey:self.identifier];
}

@end


@implementation WCEasySettingsSliderCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
        cellSlider = [[UISlider alloc] initWithFrame:CGRectMake(60, 36, self.frame.size.width - 80, 29)];
        [self.contentView addSubview:cellSlider];
        
        sliderPercentage = [[UILabel alloc] initWithFrame:CGRectMake(15, 36, 50, 29)];
        [self.contentView addSubview:sliderPercentage];
        
        cellTitle = [[UILabel alloc] initWithFrame:CGRectMake(0, 7, self.frame.size.width, 21)];
        cellTitle.textAlignment = NSTextAlignmentCenter;
        [self.contentView addSubview:cellTitle];
    }
    return self;
}

- (void)setFrame:(CGRect)frame
{
    [super setFrame:frame];
    cellSlider.frame = CGRectMake(60, 36, frame.size.width - 80, 29);
    cellTitle.frame = CGRectMake(0, 7, frame.size.width, 21);
}

- (void)setController:(WCEasySettingsSlider *)controller
{
    [super setController:controller];
    cellTitle.text = controller.title;
    
    [cellSlider removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
    [cellSlider addTarget:self action:@selector(onSlide:) forControlEvents:UIControlEventValueChanged];
    cellSlider.value = [[NSUserDefaults standardUserDefaults] floatForKey:controller.identifier];
    
    sliderPercentage.text = [NSString stringWithFormat:@"%d%%", (int)(cellSlider.value * 100)];
    
    self.selectionStyle = UITableViewCellSelectionStyleNone;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

- (void)onSlide:(UISlider *)s
{
    WCEasySettingsSlider *c = (WCEasySettingsSlider *)self.controller;
    sliderPercentage.text = [NSString stringWithFormat:@"%d%%", (int)(cellSlider.value * 100)];
    [c onSlide:s];
}

@end
