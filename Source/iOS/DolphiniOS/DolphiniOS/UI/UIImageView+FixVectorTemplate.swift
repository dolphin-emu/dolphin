// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// https://gist.github.com/buechner/3b97000a6570a2bfbc99c005cb010bac
// Fixing a bug in XCode, where an image set as template can not be tinted in Interface Builder: http://openradar.appspot.com/18448072
import UIKit

extension UIImageView {
    override public func awakeFromNib() {
        super.awakeFromNib()
        self.tintColorDidChange()
    }
}
