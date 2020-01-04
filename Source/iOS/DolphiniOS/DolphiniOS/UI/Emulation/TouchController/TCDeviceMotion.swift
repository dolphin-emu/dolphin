// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import CoreMotion
import Foundation

@objc public class TCDeviceMotion: NSObject
{
  @objc public static let shared = TCDeviceMotion()
  
  private let motion_manager = CMMotionManager()
  
  private var is_motion_enabled = false
  private var orientation: UIInterfaceOrientation = .portrait
  private var motion_mode = 0
  private var m_port = 0
  
  override required init()
  {
    // motion_mode:
    // 0 - On
    // 1 - Off
    motion_mode = UserDefaults.standard.integer(forKey: "tscontroller_motion_mode")
  }
  
  @objc func registerMotionHandlers()
  {
    if (is_motion_enabled || motion_mode == 1)
    {
      return
    }
    
    // Set our orientation properly
    self.statusBarOrientationChanged()
    
    // Create the motion queue
    let motion_queue = OperationQueue()
    
    // Set the sensor update times
    // 200Hz is the Wiimote update interval
    let update_interval: Double = 1.0 / 200.0
    self.motion_manager.accelerometerUpdateInterval = update_interval
    self.motion_manager.gyroUpdateInterval = update_interval
    
    // Register the handlers
    self.motion_manager.startAccelerometerUpdates(to: motion_queue) { (data, error) in
      if (error != nil)
      {
        return
      }
      
      // Get the data
      let acceleration = data!.acceleration
      
      var x, y: Double
      var z = acceleration.z
      
      switch (self.orientation)
      {
      case .portrait, .unknown:
        x = -acceleration.x
        y = -acceleration.y
      case .landscapeRight:
        x = acceleration.y
        y = -acceleration.x
      case .portraitUpsideDown:
        x = acceleration.x
        y = acceleration.y
      case .landscapeLeft:
        x = -acceleration.y
        y = acceleration.x
      @unknown default:
        return
      }
      
      // CMAccelerationData's units are G's
      let gravity = -9.81
      x *= gravity
      y *= gravity
      z *= gravity
      
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_LEFT.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_RIGHT.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_FORWARD.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_BACKWARD.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_UP.rawValue), value: CGFloat(z))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_ACCEL_DOWN.rawValue), value: CGFloat(z))
    }
    
    self.motion_manager.startGyroUpdates(to: motion_queue) { (data, error) in
      if (error != nil)
      {
        return
      }
      
      // Get the data
      let rotation_rate = data!.rotationRate
      
      var x, y: Double
      let z = rotation_rate.z
      
      switch (self.orientation)
      {
      case .portrait, .unknown:
        x = -rotation_rate.x
        y = -rotation_rate.y
      case .landscapeRight:
        x = rotation_rate.y
        y = -rotation_rate.x
      case .portraitUpsideDown:
        x = rotation_rate.x
        y = rotation_rate.y
      case .landscapeLeft:
        x = -rotation_rate.y
        y = rotation_rate.x
      @unknown default:
        return
      }
      
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_PITCH_UP.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_PITCH_DOWN.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_ROLL_LEFT.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_ROLL_RIGHT.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_YAW_LEFT.rawValue), value: CGFloat(z))
      MainiOS.gamepadMoveEvent(onPad: Int32(self.m_port), axis: Int32(TCButtonType.WIIMOTE_GYRO_YAW_RIGHT.rawValue), value: CGFloat(z))
    }
    
    self.is_motion_enabled = true
  }
  
  @objc func stopMotionUpdates()
  {
    if (!is_motion_enabled)
    {
      return
    }
    
    self.motion_manager.stopAccelerometerUpdates()
    self.motion_manager.stopGyroUpdates()
    
    self.is_motion_enabled = false
  }
  
  @objc func getMotionMode() -> Int
  {
    return self.motion_mode
  }
  
  @objc func setMotionMode(_ mode: Int)
  {
    self.motion_mode = mode
    UserDefaults.standard.set(motion_mode, forKey: "tscontroller_motion_mode")
    
    if (motion_mode != 1)
    {
      self.registerMotionHandlers()
    }
    else
    {
      self.stopMotionUpdates()
    }
  }
  
  @objc func setPort(_ port: Int)
  {
    self.m_port = port
  }
  
  // UIApplicationDidChangeStatusBarOrientationNotification is deprecated...
  @objc func statusBarOrientationChanged()
  {
    self.orientation = UIApplication.shared.statusBarOrientation
  }
  
}
