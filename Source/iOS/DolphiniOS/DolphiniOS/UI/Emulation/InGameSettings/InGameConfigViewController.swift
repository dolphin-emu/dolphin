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
  
  override func tableView(_ tableView: UITableView, willSelectRowAt indexPath: IndexPath) -> IndexPath?
  {
    if (indexPath.section == 0 && indexPath.row == 1 && !m_emulation_controller!.isWii)
    {
      let alert = UIAlertController(title: "GameCube Game", message: "You cannot do this in a GameCube game.", preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
      self.present(alert, animated: true, completion: nil)
      
      return nil
    }
    
    return indexPath
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
