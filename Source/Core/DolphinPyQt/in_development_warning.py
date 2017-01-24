from PyQt5.QtCore import Qt, QFileInfo
from PyQt5.QtGui import QFont, QFontMetrics, QIcon, QPalette
from PyQt5.QtWidgets import QApplication, QCommandLinkButton, \
    QDialog, QDialogButtonBox, QHBoxLayout, \
    QLabel, QLayout, QMessageBox, QSizePolicy, QStyle, QVBoxLayout, QWidget

from wrap_dolphin import Resources

def launch_dolphin_wx():
    app = QApplication.instance()
    
    file_info = QFileInfo()
    for path in ("/Dolphin.exe", "/dolphin-emu", "/Dolphin"):
        file_info.setFile(app.applicationDirPath() + path)
        if file_info.isExecutable():
            return QProcess.startDetached(file_info.absoluteFilePath(), {})

    return False

class InDevelopmentWarning(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent, Qt.WindowSystemMenuHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint)

        container = QWidget(self)
        std_buttons = QDialogButtonBox(QDialogButtonBox.Cancel, Qt.Horizontal, self)
        heading = QLabel(container)
        icon = QLabel(container)
        body = QLabel(container)

        btn_dolphinwx = QCommandLinkButton(
            self.tr("Run DolphinWX Instead"),
            self.tr("Recommended for normal users"),
            container)
        btn_run = QCommandLinkButton(
            self.tr("Use DolphinPyQt Anyway"), container)

        container.setForegroundRole(QPalette.Text)
        container.setBackgroundRole(QPalette.Base)
        container.setAutoFillBackground(True)

        std_buttons.setContentsMargins(10, 0, 10, 0)

        heading_font = QFont(heading.font())
        heading_font.setPointSizeF(heading_font.pointSizeF() * 1.5)
        heading.setFont(heading_font)
        heading.setText(self.tr("DolphinPyQt Experimental GUI"))
        heading.setForegroundRole(QPalette.Text)
        heading.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        heading.setSizePolicy(QSizePolicy.MinimumExpanding, QSizePolicy.Fixed)

        icon.setPixmap(self.style().standardPixmap(QStyle.SP_MessageBoxWarning, None, self))
        icon.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)

        body_text = self.tr(
            """<p>DolphinPyQt is a very new, very experimental GUI that may 
            never even be merged upstream.  The implementation is <b>very
            incomplete</b>, even basic functionality like changing settings
            and attaching gamepads may be missing or not work at all.</p>
            <p>Nobody should use it.</p>""")
        body.setText(body_text)
        body.setWordWrap(True)
        body.setForegroundRole(QPalette.Text)
        body.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        body.setSizePolicy(QSizePolicy.MinimumExpanding, QSizePolicy.MinimumExpanding)
        body.setMinimumWidth(QFontMetrics(body.font()).averageCharWidth() * 76)

        btn_dolphinwx.setDefault(True)

        def trylaunch(_):
            if not launch_dolphin_wx():
                QMessageBox.critical(
                    self, self.tr("Failed to launch"),
                    self.tr("Could not start DolphinWX. Check for dolphin.exe in the installation directory."))
            self.reject()

        btn_dolphinwx.clicked.connect(trylaunch)

        btn_run.clicked.connect(lambda _: self.accept())

        std_buttons.button(QDialogButtonBox.Cancel).clicked.connect(lambda _: self.reject())

        body_column = QVBoxLayout()
        body_column.addWidget(heading)
        body_column.addWidget(body)
        body_column.addWidget(btn_dolphinwx)
        body_column.addWidget(btn_run)
        body_column.setContentsMargins(0, 0, 0, 0)
        body_column.setSpacing(10)

        icon_layout = QHBoxLayout(container)
        icon_layout.addWidget(icon, 0, Qt.AlignTop)
        icon_layout.addLayout(body_column)
        icon_layout.setContentsMargins(15, 10, 10, 10)
        icon_layout.setSpacing(15)

        top_layout = QVBoxLayout(self)
        top_layout.addWidget(container)
        top_layout.addWidget(std_buttons)
        top_layout.setSpacing(10)
        top_layout.setContentsMargins(0, 0, 0, 10)
        top_layout.setSizeConstraint(QLayout.SetMinimumSize)

        self.setWindowIcon(QIcon(Resources.GetMisc(Resources.LOGO_SMALL)))
        self.setWindowTitle(self.tr("DolphinPyQt Experimental GUI"))
