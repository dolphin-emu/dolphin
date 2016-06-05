//
//  WCEasySettingsSlider.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import "WCEasySettingsItem.h"

@interface WCEasySettingsSlider : WCEasySettingsItem

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title;
- (void)onSlide:(UISlider *)s;
@end

@interface WCEasySettingsSliderCell : WCEasySettingsItemCell {
    UILabel     *cellTitle;
    UISlider    *cellSlider;
    UILabel     *sliderPercentage;
}


@end
