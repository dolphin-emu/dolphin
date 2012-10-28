import QtQuick 1.1
import QtDesktop 0.1
// Doesn't always work? This is the system menu bar.
// Currently, the WX Widgets UI has been replicated:
// File Emulation Options Tools View Help
MenuBar {
    Menu {
        text: "File"
        MenuItem {
            text: "Open..."
            shortcut: "Ctrl+O"
            onTriggered: fileDialogLoad.open();
        }
        MenuItem {
            text: "Change Disc..."
            enabled: false
            // TODO
        }
        MenuItem {
            text: "Boot from DVD drive ->"
            // TODO: This should be a menu that shows available drives
        }
        MenuItem {
            text: "Refresh List"
            // TODO
        }
        MenuItem {
            text: "Browse for ISOs..."
            // TODO
        }
        MenuItem {
            text: "Exit"
            shortcut: "Ctrl+Q"
            onTriggered: Qt.quit()
        }
    }
    Menu { // TODO: ALL
        text: "Emulation"
        MenuItem {
            text: "Play"
            shortcut: "F10"
        }
        MenuItem {
            text: "Stop"
            shortcut: "Escape"
        }
        MenuItem {
            text: "Reset"
        }
        MenuItem {
            text: "Fullscreen"
            shortcut: "Alt+Return"
        }
        MenuItem {
            text: "Start Recording"
        }
        MenuItem {
            text: "Play Recording..."
        }
        MenuItem {
            text: "Export Recording..."
        }
        MenuItem {
            text: "Read-only Mode"
        }
        MenuItem {
            text: "TAS Input"
        }
        MenuItem {
            text: "Frame Advance"
        }
        MenuItem {
            text: "Frame Skipping"
            // TODO: Menu with 0->9
        }
        MenuItem {
            text: "Take Screenshot"
        }
        MenuItem {
            text: "Load State"
        }
        MenuItem {
            text: "Save State"
        }
    }
    Menu { // TODO: ALL
        text: "Options"
    }
    Menu { // TODO: ALL
        text: "Tools"
    }
    Menu { // TODO: ALL
        text: "View"
    }
    Menu { // TODO: ALL
        text: "Help"
    }
}

