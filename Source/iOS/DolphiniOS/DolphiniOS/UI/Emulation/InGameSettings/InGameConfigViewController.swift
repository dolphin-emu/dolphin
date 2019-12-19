// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import UIKit

class InGameConfigViewController: UITableViewController
{
  var m_emulation_controller: EmulationViewController? = nil
  
  override func viewDidLoad()
  {
    super.viewDidLoad()
    
    self.m_emulation_controller = self.parent?.presentingViewController?.children[0] as? EmulationViewController
  }
  
  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath)
  {
    if (indexPath.section == 0 && indexPath.row == 0)
    {
      if (m_emulation_controller!.isWii)
      {
        self.performSegue(withIdentifier: "controller_toggle_wii", sender: nil)
      }
      else
      {
        self.performSegue(withIdentifier: "controller_toggle_gc", sender: nil)
      }
    }
  }
  
  override func prepare(for segue: UIStoryboardSegue, sender: Any?)
  {
    if (segue.identifier?.starts(with: "controller_toggle") ?? false)
    {
      let destination = segue.destination as! ControllerToggleViewController
      destination.m_emulation_controller = self.m_emulation_controller
    }
  }

}
