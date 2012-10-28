import QtQuick 1.1
import QtDesktop 0.1
import "components"

Window {
    id: window
    // The geometry can be set in main.cpp, according to defaults or save
    width: 538
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
        TableView {
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: parent.width;
            TableColumn { role: "TYPE"; title: ""; width: 40 }
            TableColumn { role: "LOGO"; title: "Banner"; width: 60 }
            TableColumn { role: "NAME"; title: "Title"; width: 100 }
            TableColumn { role: "DESC"; title: "Notes" }
            TableColumn { role: "FLAG"; title: ""; width: 30 }
            TableColumn { role: "SIZE"; title: "Size"; width: 50 }
            TableColumn { role: "STAR"; title: "State"; width: 50 }
            //model: gameData // from C++
        }
    }
}
