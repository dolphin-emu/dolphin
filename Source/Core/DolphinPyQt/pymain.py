import sys

from PyQt5.QtCore import Qt, QAbstractEventDispatcher
from PyQt5.QtWidgets import QApplication, \
    QDialog

from in_development_warning import InDevelopmentWarning
from main_window import MainWindow
from wrap_dolphin import *
from sip_dolphin import Settings

app = None

def main():
    global app
    # Note: sys.argv is not set; need to set it from Main.cpp
    app = QApplication([])

    UICommon.SetUserDirectory(b"")
    UICommon.CreateDirectories()
    UICommon.Init()
    Resources.Init()

    # Whenever the event loop is about to go to sleep, dispatch the jobs
    # queued in the Core first.
    QAbstractEventDispatcher.instance().aboutToBlock.connect(Core.HostDispatchJobs)

    retval = 0
    if Settings().IsInDevelopmentWarningEnabled():
        retval = InDevelopmentWarning().exec_() == QDialog.Rejected
    if not retval:
        win = MainWindow()
        win.show()
        app.exec_()

    BootManager.Stop()
    Core.Shutdown()
    UICommon.Shutdown()

    # Currently ignored by Main.cpp
    return retval
    
