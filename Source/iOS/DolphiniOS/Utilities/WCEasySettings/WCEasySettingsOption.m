//
//  WCEazySettingsOption.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsOption.h"
#import "WCEasySettingsOptionViewController.h"
@implementation WCEasySettingsOption

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title options:(NSArray *)options optionSubtitles:(NSArray *)optionSubtitles subtitle:(NSString *)subtitle
{
    if (self = [super initWithIdentifier:identifier title:title]) {
        self.options = options;
        self.optionSubtitles = optionSubtitles;
        self.cellIdentifier = @"Option";
        self.type = WCEasySettingsTypeOption;
        self.subtitle = subtitle;
    }
    return self;
}

- (void)itemSelected
{
    WCEasySettingsOptionViewController *optionView = [[WCEasySettingsOptionViewController alloc] initWithStyle:UITableViewStyleGrouped];
    optionView.title = self.title;
    optionView.option = self;
    [self.delegate pushViewController:optionView];
}

@end

@implementation WCEasySettingsOptionCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:reuseIdentifier]) {
        
    }
    return self;
}

- (void)setController:(WCEasySettingsOption *)controller
{
    self.textLabel.text = controller.title;
    
    NSInteger selectedOption = [[NSUserDefaults standardUserDefaults] integerForKey:controller.identifier];
    if (selectedOption >= controller.options.count) {
        [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:controller.identifier];
        selectedOption = 0;
    }
    self.detailTextLabel.text = controller.options[selectedOption];
    
    self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

@end
