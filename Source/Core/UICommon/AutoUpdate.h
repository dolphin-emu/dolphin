// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

// This class defines all the logic for Dolphin auto-update checking. UI-specific elements have to
// be defined in a backend specific subclass.
class AutoUpdateChecker
{
public:
  // Initiates a check for updates in the background. Calls the OnUpdateAvailable callback if an
  // update is available, does "nothing" otherwise.
  void CheckForUpdate();

  static bool SystemSupportsAutoUpdates();

  struct NewVersionInformation
  {
    // Name (5.0-1234) and revision hash of the new version.
    std::string new_shortrev;
    std::string new_hash;

    // The full changelog in HTML format.
    std::string changelog_html;

    // Internals, to be passed to the updater binary.
    std::string this_manifest_url;
    std::string next_manifest_url;
    std::string content_store_url;
  };

  // Starts the updater process, which will wait in the background until the current process exits.
  enum class RestartMode
  {
    NO_RESTART_AFTER_UPDATE = 0,
    RESTART_AFTER_UPDATE,
  };
  void TriggerUpdate(const NewVersionInformation& info, RestartMode restart_mode);

protected:
  virtual void OnUpdateAvailable(const NewVersionInformation& info) = 0;
};
