// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import UIKit

class TCWiiPad: TCView, UIGestureRecognizerDelegate
{
  var touch_ir_enabled: Bool = false
  var current_rect: CGRect? = nil
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
    pressHandler.delegate = self
    self.real_view!.addGestureRecognizer(pressHandler)
  }
  
  // Based on InputOverlayPointer.java
  @objc func recalculatePointerValues(new_rect: CGRect, game_aspect: CGFloat)
  {
    // Set rect
    self.current_rect = new_rect
    
    // Adjusting for black bars
    let device_aspect: CGFloat = new_rect.width / new_rect.height
    self.aspect_adjusted = game_aspect / device_aspect;
    
    if (game_aspect <= device_aspect) // black bars on left/right
    {
      self.x_adjusted = true
      
      let game_x = round(new_rect.height * game_aspect)
      let buffer = new_rect.width - game_x
      
      self.max_size = CGSize(width: (new_rect.width - buffer) / 2, height: new_rect.height / 2)
    }
    else // bars on top and bottom
    {
      self.x_adjusted = false
      
      let game_y = round(new_rect.width / game_aspect)
      let buffer = new_rect.height - game_y
      
      self.max_size = CGSize(width: (new_rect.width / 2), height: (new_rect.height - buffer) / 2)
    }
  }
  
  func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool
  {
    if (touch.view == self.real_view)
    {
      return true
    }
    
    return false
  }
  
  func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldRequireFailureOf otherGestureRecognizer: UIGestureRecognizer) -> Bool
  {
    if (otherGestureRecognizer is UIScreenEdgePanGestureRecognizer)
    {
      return true
    }
    
    return false
  }
  
  @objc func handleLongPress(gesture: UILongPressGestureRecognizer)
  {
    if (!self.touch_ir_enabled)
    {
      return
    }
    
    if let image_rect = self.current_rect
    {
      // Convert the point to the native screen scale
      let screen_scale = UIScreen.main.scale
      var point = gesture.location(in: self)
      point.x *= screen_scale
      point.y *= screen_scale
      var axises: [CGFloat] = [ 0, 0 ]
      
      // Check if this touch is within the rendering rectangle
      if (point.y >= image_rect.origin.y && point.y <= image_rect.origin.y + image_rect.height)
      {
        // Move the point to the origin of the rectangle
        point.x = point.x - image_rect.origin.x
        point.y = point.y - image_rect.origin.y
        
        if (self.x_adjusted)
        {
          axises[0] = (point.y - self.max_size.height) / self.max_size.height
          axises[1] = ((point.x * self.aspect_adjusted) - self.max_size.width) / self.max_size.width
        }
        else
        {
          axises[0] = ((point.y * self.aspect_adjusted) - self.max_size.height) / self.max_size.height
          axises[1] = (point.x - self.max_size.width) / self.max_size.width
        }
        
        let send_axises: [CGFloat] = [ axises[0], axises[0], axises[1], axises[1]]
        
        let axisStartIdx = TCButtonType.WIIMOTE_IR_UP
        for i in 0...3
        {
          MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(axisStartIdx.rawValue + i), value: send_axises[i])
        }
      }
    }
  }
  
  @objc func setTouchPointerEnabled(_ enabled: Bool)
  {
    self.touch_ir_enabled = enabled
  }
  
}
