import sys

from PyQt5.QtCore import pyqtSignal, QFile
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import QFileDialog, QMainWindow, \
    QMessageBox, QStackedWidget

from wrap_dolphin import *
from sip_dolphin import *

from render_widget import RenderWidget

about = None

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__(None)

        self.setWindowTitle(self.tr("Dolphin"))
        self.setWindowIcon(QIcon(Resources.GetMisc(Resources.LOGO_SMALL)))
        self.setUnifiedTitleAndToolBarOnMac(True)

        self.CreateComponents()

        self.ConnectGameList()
        self.ConnectPathsDialog()
        self.ConnectToolBar()
        self.ConnectRenderWidget()
        self.ConnectStack()
        self.ConnectMenuBar()

    def CreateComponents(self):
        self._menu_bar = MenuBar(self)
        self._tool_bar = ToolBar(self)
        self._game_list = GameList(self)
        self._render_widget = RenderWidget()
        self._stack = QStackedWidget(self)
        self._paths_dialog = PathDialog(self)
        self._settings_window = SettingsWindow(self)

    def ConnectGameList(self):
        self._game_list.GameSelected.connect(self.Play)

    def ConnectPathsDialog(self):
        self._paths_dialog.PathAdded.connect(self._game_list.DirectoryAdded)
        self._paths_dialog.PathRemoved.connect(self._game_list.DirectoryRemoved)

    def ConnectToolBar(self):
        self.addToolBar(self._tool_bar)
        self._tool_bar.OpenPressed.connect(self.Open)
        self._tool_bar.PlayPressed.connect(self.Play)
        self._tool_bar.PausePressed.connect(self.Pause)
        self._tool_bar.StopPressed.connect(self.Stop)
        self._tool_bar.FullScreenPressed.connect(self.FullScreen)
        self._tool_bar.ScreenShotPressed.connect(self.ScreenShot)
        self._tool_bar.PathsPressed.connect(self.ShowPathsDialog)
        self._tool_bar.SettingsPressed.connect(self.ShowSettingsWindow)

        self.EmulationStarted.connect(self._tool_bar.EmulationStarted)
        self.EmulationPaused.connect(self._tool_bar.EmulationPaused)
        self.EmulationStopped.connect(self._tool_bar.EmulationStopped)
        
    def ConnectRenderWidget(self):
        self._rendering_to_main = False
        self._render_widget.hide()
        self._render_widget.escape_pressed.connect(self.Stop)
        self._render_widget.closed.connect(self.ForceStop)

    def ConnectStack(self):
        self._stack.setMinimumSize(800, 600)
        self._stack.addWidget(self._game_list)
        self.setCentralWidget(self._stack)

    def ConnectMenuBar(self):
        self.setMenuBar(self._menu_bar)

        # File
        self._menu_bar.Open.connect(self.Open)
        self._menu_bar.Exit.connect(self.close)

        # Emulation
        self._menu_bar.Pause.connect(self.Pause)
        self._menu_bar.Play.connect(self.Play)
        self._menu_bar.Stop.connect(self.Stop)
        self._menu_bar.Reset.connect(self.Reset)
        self._menu_bar.Fullscreen.connect(self.FullScreen)
        self._menu_bar.FrameAdvance.connect(self.FrameAdvance)
        self._menu_bar.Screenshot.connect(self.ScreenShot)
        self._menu_bar.StateLoad.connect(self.StateLoad)
        self._menu_bar.StateSave.connect(self.StateSave)
        self._menu_bar.StateLoadSlot.connect(self.StateLoadSlot)
        self._menu_bar.StateSaveSlot.connect(self.StateSaveSlot)
        self._menu_bar.StateLoadSlotAt.connect(self.StateLoadSlotAt)
        self._menu_bar.StateSaveSlotAt.connect(self.StateSaveSlotAt)
        self._menu_bar.StateLoadUndo.connect(self.StateLoadUndo)
        self._menu_bar.StateSaveUndo.connect(self.StateSaveUndo)
        self._menu_bar.StateSaveOldest.connect(self.StateSaveOldest)
        self._menu_bar.SetStateSlot.connect(self.SetStateSlot)

        # View
        self._menu_bar.ShowTable.connect(self._game_list.SetTableView)
        self._menu_bar.ShowList.connect(self._game_list.SetListView)
        self._menu_bar.ShowAboutDialog.connect(self.ShowAboutDialog)

        self.EmulationStarted.connect(self._menu_bar.EmulationStarted)
        self.EmulationPaused.connect(self._menu_bar.EmulationPaused)
        self.EmulationStopped.connect(self._menu_bar.EmulationStopped)

    def Open(self):
        file = QFileDialog.getOpenFileName(
            self, self.tr("Select a File"), QDir.currentPath(),
            self.tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.wbfs *.ciso *.gcz *.wad);;" +
                    "All Files (*)"))
        if file:
            self.StartGame(file)

    def Play(self):
        if Core.GetState() == Core.CORE_PAUSE:
            Core.SetState(Core.CORE_RUN)
            self.EmulationStarted.emit()
        else:
            selection = self._game_list.GetSelectedGame()
            if selection:
                self.StartGame(selection)
            else:
                last_path = Settings().GetLastGame()
                if last_path and QFile.exists(last_path):
                    self.StartGame(last_path)
                else:
                    self.Open()

    def Pause(self):
        Core.SetState(Core.CORE_PAUSE)
        self.EmulationPaused.emit()

    def Stop(self):
        stop = True
        if Settings().GetConfirmStop():
            # We could pause the game here and resume it if they say no.
            confirm = QMessageBox.question(self._render_widget, 
                                           self.tr("Confirm"),
                                           self.tr("Stop emulation?"))
            stop = (confirm == QMessageBox.Yes)

        if stop:
            self.ForceStop()

            if sys.platform == 'win32':
                # XXX I didn't wrap SetThreadExecutionState
                #SetThreadExecutionState(ES_CONTINUOUS)
                pass

        return stop

    def ForceStop(self):
        BootManager.Stop()
        self.HideRenderWidget()
        self.EmulationStopped.emit()

    def Reset(self):
        if Movie.IsRecordingInput():
            Movie.SetReset(True)
        ProcessorInterface.ResetButton_Tap()

    def FrameAdvance(self):
        Movie.DoFrameStep()
        self.EmulationPaused.emit()

    def FullScreen(self):
        # If the render widget is fullscreen we want to reset it to
        # whatever is in settings. If it's set to be fullscreen then
        # it just remakes the window, which probably isn't ideal.
        was_fullscreen = self._render_widget.is_full_screen()
        self.HideRenderWidget()
        if was_fullscreen:
            self.ShowRenderWidget()
        else:
            self._render_widget.show_full_screen()

    def ScreenShot(self):
        Core.SaveScreenShot()

    def StartGame(self, path):
        if Core.GetState() != Core.CORE_UNINITIALIZED:
            if not self.Stop():
                return

        if not BootManager.BootCore(path):
            QMessageBox.critical(self, self.tr("Error"),
                                 self.tr("Failed to init core"),
                                 QMessageBox.Ok)
            return

        Settings().SetLastGame(path)
        self.ShowRenderWidget()
        self.EmulationStarted.emit()

        if sys.platform == 'win32':
            # Prevents Windows from sleeping, turning off the display, or idling
            #XXX I haven't wrapepd these functions
            # EXECUTION_STATE shouldScreenSave = \
            #     SConfig::GetInstance().bDisableScreenSaver ? ES_DISPLAY_REQUIRED : 0;
            # SetThreadExecutionState(ES_CONTINUOUS | shouldScreenSave | ES_SYSTEM_REQUIRED);
            pass

    def ShowRenderWidget(self):
        settings = Settings()
        if settings.GetRenderToMain():
            self._rendering_to_main = True
            self._stack.setCurrentIndex(self._stack.addWidget(self._render_widget))
            get_host().request_title.connect(self.setWindowTitle)
        else:
            self._rendering_to_main = False
            if settings.GetFullScreen():
                self._render_widget.showFullScreen()
            else:
                self._render_widget.setFixedSize(settings.GetRenderWindowSize())
                self._render_widget.showNormal()

    def HideRenderWidget(self):
        if self._rendering_to_main:
            # Remove the widget from the stack and reparent it to
            # None, so that it can draw itself in a new window if
            # it wants. Disconnect the title updates.
            self._stack.removeWidget(self._render_widget)
            self._render_widget.setParent(None)
            self._rendering_to_main = False
            get_host().RequestTitle.disconnect(self.setWindowTitle)
            self.setWindowTitle(self.tr("Dolphin"))
        self._render_widget.hide()

    def ShowPathsDialog(self):
        self._paths_dialog.show()
        self._paths_dialog.raise_()
        self._paths_dialog.activateWindow()

    def ShowSettingsWindow(self):
        self._settings_window.show()
        self._settings_window.raise_()
        self._settings_window.activateWindow()

    def ShowAboutDialog(self):
        global about
        about = AboutDialog(self)
        about.show()

    def StateLoad(self):
        path = QFileDialog.getOpenFilename(
            self, self.tr("Select a File"), QDir.currentPath(),
            self.tr("All Save States (*.sav *.s##);; All Files (*)"))
        State.LoadAs(path.encode(sys.getfilesystemencoding()))

    def StateSave(self):
        path = QFileDialog.getSaveFilename(
            self, self.tr("Select a File"), QDir.currentPath(),
            self.tr("All Save States (*.sav *.s##);; All Files (*)"))
        State.SaveAs(path.encode(sys.getfilesystemencoding()))

    def StateLoadSlot(self):
        State.Load(self._state_slot)

    def StateSaveSlot(self):
        State.Save(self._state_slot, True)
        self._menu_bar.UpdateStateSlotMenu()

    def StateLoadSlotAt(self, slot):
        State.Load(slot)

    def StateSaveSlotAt(self, slot):
        State.Save(slot, True)
        self._menu_bar.UpdateStateSlotMenu()

    def StateLoadUndo(self):
        State.UndoLoadState()

    def StateSaveUndo(self):
        State.UndoSaveState()

    def StateSaveOldest(self):
        State.SaveFirstSaved()

    def SetStateSlot(self, slot):
        Settings().SetStateSlot(slot)
        self._state_slot = slot

    EmulationStarted = pyqtSignal([])
    EmulationPaused = pyqtSignal([])
    EmulationStopped = pyqtSignal([])
    
