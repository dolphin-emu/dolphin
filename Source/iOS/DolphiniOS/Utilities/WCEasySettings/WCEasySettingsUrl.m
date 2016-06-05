//
//  WCEazySettingsOption.m
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsUrl.h"
@implementation WCEasySettingsUrl

- (id)initWithTitle:(NSString *)title subtitle:(NSString *)subtitle  url:(NSString *)url
{
    if (self = [super initWithIdentifier:nil title:title]) {
        self.url = url;
        self.subtitle = subtitle;
        self.type = WCEasySettingsTypeUrl;
        self.cellIdentifier = @"Url";
    }
    return self;
}


- (void)itemSelected
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:self.url]];
}

@end

@implementation WCEasySettingsUrlCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:reuseIdentifier]) {
        
    }
    return self;
}

- (void)setController:(WCEasySettingsUrl *)controller
{
    self.textLabel.text = controller.title;
    self.detailTextLabel.text = controller.subtitle;
    if (controller.url.length > 0) {
        self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        self.selectionStyle = UITableViewCellSelectionStyleDefault;
    } else {
        self.accessoryType = UITableViewCellAccessoryNone;
        self.selectionStyle = UITableViewCellSelectionStyleNone;
    }
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];
}

@end
