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
  
  override required init()
  {
    // integer(forKey:) returns 0 by default, so we use 0 as the "this isn't set"
    // value, and just add 1 to what Android uses
    //
    // motion_mode:
    // 1 - On with IMU IR
    // 2 - On without IMU IR
    // 3 - Off
    motion_mode = UserDefaults.standard.integer(forKey: "motion_controls_enabled")
    if (motion_mode == 0)
    {
      // On without IMU IR is the default
      UserDefaults.standard.set(2, forKey: "motion_controls_enabled")
      motion_mode = 2
    }
  }
  
  func registerMotionHandlers()
  {
    if (is_motion_enabled)
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
      
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_LEFT.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_RIGHT.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_FORWARD.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_BACKWARD.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_UP.rawValue), value: CGFloat(z))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_ACCEL_DOWN.rawValue), value: CGFloat(z))
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
      
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_PITCH_UP.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_PITCH_DOWN.rawValue), value: CGFloat(x))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_ROLL_LEFT.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_ROLL_RIGHT.rawValue), value: CGFloat(y))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_YAW_LEFT.rawValue), value: CGFloat(z))
      MainiOS.gamepadMoveEvent(forAxis: Int32(TCButtonType.WIIMOTE_GYRO_YAW_RIGHT.rawValue), value: CGFloat(z))
    }
    
    self.is_motion_enabled = true
  }
  
  func stopMotionUpdates()
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
    return self.motion_mode - 1
  }
  
  @objc func setMotionMode(_ mode: Int)
  {
    self.motion_mode = mode + 1
    UserDefaults.standard.set(motion_mode, forKey: "motion_controls_enabled")
    
    if (motion_mode != 3)
    {
      self.registerMotionHandlers()
    }
    else
    {
      self.stopMotionUpdates()
    }
  }
  
  // UIApplicationDidChangeStatusBarOrientationNotification is deprecated...
  func statusBarOrientationChanged()
  {
    self.orientation = UIApplication.shared.statusBarOrientation
  }
  
}
