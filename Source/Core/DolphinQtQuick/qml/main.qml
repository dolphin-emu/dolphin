import QtQuick 1.1
import QtDesktop 0.1
import "components"

Window {
    id: window
    // The geometry can be set in main.cpp, according to defaults or save
    width: 648
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
        StyleItem {
            id: itemstyle
            elementType: "item"
            visible:false
        }
        TableView {
            id: gameBrowser
            visible: model.count > 0
            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: parent.width;
            itemDelegate: Item {
                property int implicitWidth: sizehint.paintedWidth + 4
                height: Math.max(image.paintedHeight,Math.max(16, styleitem.implicitHeight))
                Image {
                    id: image
                    visible: columnIndex == 0 || columnIndex == 1 || columnIndex == 4 || columnIndex == 6
                    source: {
                        var image = "";
                        switch (columnIndex)
                        {
                        case 0:
                            image = "images/platform" + itemValue + ".png"
                            break;
                        case 1:
                            image = "images/no_banner.png"
                            break;
                        case 4:
                            image = "images/flag" + itemValue + ".png"
                            break;
                        case 6:
                            image = "images/star" + itemValue + ".png"
                            break;
                        default:
                            image = "";
                            break;
                        }
                        return image;
                    }
                }
                Text {
                    visible: !image.visible
                    id: label
                    width: parent.width
                    anchors.margins: 6
                    font.pointSize: itemstyle.fontPointSize
                    anchors.left: parent.left
                    anchors.right: parent.right
                    horizontalAlignment: (columnIndex == 5) ? Text.AlignRight : itemTextAlignment
                    anchors.verticalCenter: parent.verticalCenter
                    elide: itemElideMode
                    text: itemValue ? itemValue : ""
                    color: itemForeground
                }
                Text {
                    id: sizehint
                    font: label.font
                    text: itemValue ? itemValue : ""
                    visible: false
                }
            }

            TableColumn { role: "type"; title: ""; width: 56 }
            TableColumn { role: "logo"; title: "Banner"; width: 100 }
            TableColumn { role: "name"; title: "Title"; width: 200 }
            TableColumn { role: "desc"; title: "Notes"; width: 120 }
            TableColumn { role: "flag"; title: ""; width: 38 }
            TableColumn { role: "size"; title: "Size"; width: 80 }
            TableColumn { role: "star"; title: "State"; width: 65 }
            model: gameList // from C++
        }
    }
}
