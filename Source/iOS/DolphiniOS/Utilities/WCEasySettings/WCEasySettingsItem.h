//
//  WCEasySettingsItem.h
//  iNDS
//
//  Created by Will Cobb on 4/20/16.
//  Copyright © 2016 iNDS. All rights reserved.
//

#import <Foundation/Foundation.h>



typedef NS_ENUM(NSInteger, WCEazySettingsType) {
    WCEasySettingsTypeSwitch,
    WCEasySettingsTypeSegment,
    WCEasySettingsTypeOption,
    WCEasySettingsTypeSlider,
    WCEasySettingsTypeUrl,
    WCEasySettingsTypeCustom
};

@protocol WCEasySettingsItemDelegate <NSObject>

- (void)pushViewController:(UIViewController *)controller;

@end

@interface WCEasySettingsItem : NSObject

@property NSString          *title;
@property NSString          *identifier;
@property WCEazySettingsType type;
@property NSString          *cellIdentifier;
@property id <WCEasySettingsItemDelegate> delegate;

- (id)initWithIdentifier:(NSString *)identifier title:(NSString *)title;
- (void)itemSelected;

@end

@interface WCEasySettingsItemCell : UITableViewCell

@property (nonatomic) WCEasySettingsItem *controller;

@end

