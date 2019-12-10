// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation
import MetalKit
import UIKit

class EmulationViewController: UIViewController
{
  var softwareFile: String = ""
  var isWii: Bool = false
  
  @objc init(file: String, wii: Bool)
  {
    super.init(nibName: nil, bundle: nil)
    
    self.softwareFile = file
    self.isWii = wii
    
    self.modalPresentationStyle = .fullScreen
  }
  
  required init?(coder: NSCoder)
  {
    super.init(coder: coder)
  }
  
  override func loadView()
  {
    let videoBackend = MainiOS.getGfxBackend()
    if (videoBackend == "Vulkan")
    {
      self.view = MTKView(frame: UIScreen.main.bounds)
    }
    else
    {
      // Everything else (OpenGL ES, Software, Null) should use the EAGLView
      self.view = EAGLView(frame: UIScreen.main.bounds)
    }
    
    let controllerNibName: String = (self.isWii) ? "TCWiiPad" : "TCGameCubePad"
    
    let padView = Bundle(for: type(of: self)).loadNibNamed(controllerNibName, owner: self, options: nil)![0] as! UIView
    padView.frame = UIScreen.main.bounds
    self.view.addSubview(padView)
  }
  
  override func viewDidLoad()
  {
    let queue = DispatchQueue(label: "org.dolphin-emu.ios.emulation-queue")
    let view = self.view
    
    queue.async
    {
      MainiOS.startEmulation(withFile: self.softwareFile, view: view)
    }
  }
  
  override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator)
  {
    MainiOS.windowResized()
  }
  
}
