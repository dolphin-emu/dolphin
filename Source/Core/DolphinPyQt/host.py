from PyQt5.QtCore import pyqtSignal, QAbstractEventDispatcher, QObject
from PyQt5.QtWidgets import QApplication

from wrap_dolphin import Common

class Host(QObject):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._render_handle = None
        self._render_focus = False
        self._render_fullscreen = False

    def get_render_handle(self):
        return self._render_handle

    def set_render_handle(self, handle):
        self._render_handle = handle

    def get_render_focus(self):
        return self._render_focus

    def set_render_focus(self, focus):
        self._render_focus = focus

    def get_render_fullscreen(self):
        return self._render_fullscreen

    def set_render_fullscreen(self, fullscreen):
        self._render_fullscreen = fullscreen

    def message(self, id):
        if id == Common.WM_USER_STOP:
            self.request_stop.emit()
        elif id == Common.WM_USER_JOB_DISPATCH:
            # Just poke the main thread to get it to wake up, job dispatch
            # will happen automatically before it goes back to sleep again.
            QAbstractEventDispatcher.instance(QApplication.instance().thread()).wakeUp();
            
    def update_title(self, title):
        self.request_title.emit(title)
    
    request_title = pyqtSignal([str])
    request_stop = pyqtSignal([])
    request_render_size = pyqtSignal([int, int])
