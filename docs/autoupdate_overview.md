# AutoUpdate Overview
Dolphin's autoupdate procedure is spread through a number of files; this overview describes the
update flow.

## General notes:
* The updater is only supported on Windows and MacOS.
* There are four update frequency tracks: Dev (updated every commit), Beta (a few times a year),
  Stable (very rarely), and Disabled.
* Applications can't overwrite themselves so a separate application is responsible for actually
  updating the Dolphin executable and other files.
* The updater application needs to be able to also update itself, so it creates a copy which
  updates both Dolphin and the original updater. Every time Dolphin launches it deletes the
  updater copy (if it exists).

## Class and file responsibilities:
* AutoUpdateChecker (UICommon/AutoUpdate.h): Checks if an update is available.
    * Verifies the updater is supported on the user's platform and hasn't been disabled.
    * Retrieves new version information from the update server.
    * Copies the updater application and launches the copy.
* Updater (DolphinQt/Updater.h): Serves as the interface between AutoUpdateChecker and Qt.
    * Spawns a background thread when Dolphin launches that calls AutoUpdateChecker.
    * Creates the update prompt window when an update is available.
    * If the user wants to update now, closes Dolphin.
* MacUpdater/main.m and MacUpdater/AppDelegate.mm: Entry point to MacOS updater.
    * Converts command line arguments to vector\<string\> and passes them to
      UpdaterCommon::RunUpdate().
* WinUpdater/main.cpp: Entry point to Windows updater.
    * Converts command line arguments to vector\<string\>.
    * Checks if updater has write access to the Dolphin executable directory. If not, attempts
      to relaunch itself with admin privileges (creating a User Account Control prompt).
    * Passes argument vector to UpdaterCommon::RunUpdate().
* UpdaterCommon/UpdaterCommon.cpp: Performs the actual update process.
    * Manages updater UI.
    * Fetches file manifests from update server for current and new versions.
    * Calculates file diff between versions.
    * Downloads and adds/replaces changed files and deletes removed files.
    * Verifies file hashes.
    * If the user updated immediately (rather than waiting for Dolphin to close before starting
       the update), starts Dolphin again when the update is complete.

## Update flow:
* An update check is started in one of two ways:
    * When Dolphin is launched (unless in NoGUI or batch mode):
         * In main.cpp an instance of Updater is created and invokes start(), which is inherited
           from QThread and creates a new thread which performs the check off the main thread.
         * QThread::start() calls run() which is overridden in Updater and calls
           AutoUpdateChecker::CheckForUpdate().
    * When the user selects Help -> "Check for Updates..." in the main menu:
         * The menu option runs a callback to MenuBar::InstallUpdateManually().
         * The Config value for the update track is backed up to a temporary variable, then
           overwritten with "dev" to force a check for the latest version. After the check
           completes the original Config value is restored.
         * Updater::CheckForUpdate() is called, which calls AutoUpdateChecker::CheckForUpdate().
* AutoUpdateChecker::CheckForUpdate() checks if the selected track has a new version available.
    * If not the check ends. If the user started it, returns to Updater::CheckForUpdate() which
      tells the user they're up to date.
* Information about the update is passed to OnUpdateAvailable(), which is overridden by Updater.
* OnUpdateAvailable() creates a window displaying the update changelog and asks the user if they
  want to update now, update after Dolphin closes, not update, or never auto-update.
* If the user wants to update AutoUpdateChecker::TriggerUpdate() is called.
* TriggerUpdate() builds the command line arguments for the updater process, creates a copy of
  the updater executable, then runs the copy in a new process.
* TriggerUpdate() returns to OnUpdateAvailable().  If the user chose to update now, Dolphin's
  main window is closed which results in the Dolphin process ending.
* The updater process begins.
    * On MacOS (starts main() in MacUpdater/main.m):
         * Checks that the process received command line arguments. If not it tells the user the
           updater can't be launched directly and quits.
         * Calls NSApplicationMain(), which passes control to the AppDelegate defined in
           MacUpdater/AppDelegate.mm.
         * The command line arguments are converted to a vector\<string\>
           and passed to RunUpdater() in UpdaterCommon.h.
    * On Windows (starts wWinMain() in WinUpdater/Main.cpp):
         * Checks that the process received command line arguments. If not it tells the user the
           updater can't be launched directly and quits.
         * Attempts to open Updater.log in the same directory as Dolphin.exe. If this fails,
           checks to see if the process has admin privileges.
             * If not, attempts to relaunch the updater as admin. This will spawn a User Account
               Control prompt.
             * If the user declines the UAC prompt, or if the updater already has admin status,
               the update aborts.
         * Converts the command line arguments to a vector\<string\> and passes them to RunUpdater()
           in UpdaterCommon.h.
* RunUpdater() parses and validates the command line arguments, hides the updater UI, then waits
  for the Dolphin process to quit.
* RunUpdater() begins the actual update.
    * Fetches file manifests from update server for current and new versions.
    * Decompresses manifests and verifies their signatures.
    * Downloads and adds/replaces changed files and deletes removed files.
    * Verifies downloaded files match manifest hashes.
* If the user updated immediately (rather than waiting for Dolphin to close before starting
  the update), Dolphin restarts.
* As part of Dolphin's normal startup process, the Updater copy is deleted.
