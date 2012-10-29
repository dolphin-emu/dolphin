import QtQuick 1.1
import QtDesktop 0.1
import "components"

Window {
    id: window
    // The geometry can be set in main.cpp, according to defaults or save
    width: 638
    height: 360
    DMenuBar { id: menubar }

    Rectangle {
        id: gallery
        width:  parent.width
        height: parent.height

        SystemPalette {id: syspal}
        StyleItem{ id: styleitem}
        color: syspal.window

        DToolBar { id: toolbar }
        DConfig { id: config }
        DGraphicsConfig { id: graphicsconfig }
        DPadConfig { id: padconfig }
        DWiimoteConfig { id: wiimoteconfig }
        FileDialog {
            id: fileDialogBrowse
            folder: currentDir
            modality: Qt.ApplicationModal
            title: "Choose a folder"
            selectFolder: true
            onAccepted: { tools.addISOFolder(filePaths) }
        }
        Text {
            visible: !gameBrowser.visible
            anchors { horizontalCenter: parent.horizontalCenter;
                      verticalCenter: parent.verticalCenter;
                      verticalCenterOffset: toolbar.height / 2 }
            text: "Could not find any games. Click <a href='#'>here</a> to browse..."
            onLinkActivated: fileDialogBrowse.open()
        }
        TableView {
            id: gameBrowser
            visible: model.count > 0
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: parent.width;
            TableColumn { role: "type"; title: ""; width: 40 }
            TableColumn { role: "logo"; title: "Banner"; width: 60 }
            TableColumn { role: "name"; title: "Title"; width: 180 }
            TableColumn { role: "desc"; title: "Notes"; }
            TableColumn { role: "flag"; title: ""; width: 45 }
            TableColumn { role: "size"; title: "Size"; width: 80 }
            TableColumn { role: "star"; title: "State"; width: 65 }
            model: gameList // from C++
        }
    }
}
