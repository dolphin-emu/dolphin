from PyQt5.QtCore import pyqtSignal, Qt, QEvent
from PyQt5.QtWidgets import QWidget

from wrap_dolphin import *

class RenderWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.setAttribute(Qt.WA_OpaquePaintEvent, True)
        self.setAttribute(Qt.WA_NoSystemBackground, True)
        
        host = get_host()
        host.request_title.connect(self.setWindowTitle)
        self.focus_changed.connect(host.set_render_focus)
        self.state_changed.connect(host.set_render_fullscreen)
        self.handle_changed.connect(host.set_render_handle)
        self.handle_changed.emit(self.winId())

    def event(self, event):
        ty = event.type()
        if ty == QEvent.KeyPress:
            if event.key() == Qt.Key_Escape:
                self.escape_pressed.emit()
        elif ty == QEvent.WinIdChange:
            self.handle_changed.emit(self.winId())
        elif ty == QEvent.FocusIn or ty == QEvent.FocusOut:
            self.focus_changed.emit(self.hasFocus())
        elif ty == QEvent.WindowStateChange:
            self.state_changed.emit(self.isFullScreen())
        elif ty == QEvent.Close:
            self.closed.emit()

        return super().event(event)

    escape_pressed = pyqtSignal([])
    closed = pyqtSignal([])
    handle_changed = pyqtSignal([int])
    focus_changed = pyqtSignal([bool])
    state_changed = pyqtSignal([bool])
