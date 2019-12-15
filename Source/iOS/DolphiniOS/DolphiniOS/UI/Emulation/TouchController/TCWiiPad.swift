// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

class TCWiiPad: TCView
{
  var max_size: CGSize = CGSize.zero
  var aspect_adjusted: CGFloat = 0
  var x_adjusted: Bool = false
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
    
    // Register our "long press" gesture recognizer
    let pressHandler = UILongPressGestureRecognizer(target: self, action: #selector(handleLongPress))
    pressHandler.minimumPressDuration = 0
    pressHandler.numberOfTouchesRequired = 1
    pressHandler.cancelsTouchesInView = false
    self.real_view!.addGestureRecognizer(pressHandler)
  }
  
  // Based on InputOverlayPointer.java
  func recalculatePointerValues()
  {
    let screen_scale = UIScreen.main.scale
    let scaled_size = CGSize(width: self.bounds.size.width * screen_scale, height: self.bounds.size.height * screen_scale)
    
    // Adjusting for black bars
    let device_aspect: CGFloat = scaled_size.width / scaled_size.height
    let game_aspect: CGFloat = MainiOS.getGameAspectRatio()
    self.aspect_adjusted = game_aspect / device_aspect;
    
    if (game_aspect <= device_aspect) // black bars on left/right
    {
      self.x_adjusted = true
      
      let game_x = round(scaled_size.height * game_aspect)
      let buffer = scaled_size.width - game_x
      
      self.max_size = CGSize(width: (scaled_size.width - buffer) / 2, height: scaled_size.height / 2)
    }
    else // bars on top and bottom
    {
      self.x_adjusted = false
      
      let game_y = round(scaled_size.width / game_aspect)
      let buffer = scaled_size.height - game_y
      
      self.max_size = CGSize(width: (scaled_size.width / 2), height: (scaled_size.height - buffer) / 2)
    }
  }
  
  @objc func handleLongPress(gesture: UILongPressGestureRecognizer)
  {
    // Convert the point to the native screen scale
    let screen_scale = UIScreen.main.scale
    var point = gesture.location(in: self)
    point.x *= screen_scale
    point.y *= screen_scale

    let image_rect = MainiOS.getRenderTargetRectangle()
    var axises: [CGFloat] = [ 0, 0 ]
    
    // Check if this touch is within the rendering rectangle
    if (point.y >= image_rect.origin.y && point.y <= image_rect.origin.y + image_rect.height)
    {
      // Move the point to the origin of the rectangle
      point.x = point.x - image_rect.origin.x
      point.y = point.y - image_rect.origin.y
      
      // TODO: why doesn't this work properly in portrait?
      /*if (self.x_adjusted)
      {
        axises[0] = (point.y - self.max_size.height) / self.max_size.height
        axises[1] = ((point.x * self.aspect_adjusted) - self.max_size.width) / self.max_size.width
      }
      else
      {
        axises[0] = ((point.y * self.aspect_adjusted) - self.max_size.height) / self.max_size.height
        axises[1] = (point.x - self.max_size.width) / self.max_size.width
      }*/
      
      axises[0] = (point.y - self.max_size.height) / self.max_size.height
      axises[1] = (point.x - self.max_size.width) / self.max_size.width
      
      let send_axises: [CGFloat] = [ axises[0], axises[0], axises[1], axises[1]]
      
      let axisStartIdx = TCButtonType.WIIMOTE_IR_UP
      for i in 0...3
      {
        MainiOS.gamepadMoveEvent(forAxis: Int32(axisStartIdx.rawValue + i), value: send_axises[i])
      }
      
    }
  }
  
}
