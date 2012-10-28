import QtQuick 1.1
import QtDesktop 0.1

// Toolbar that goes across the top of the UI
// Currently the WX Widgets UI has been replicated:
// Open Refresh Browse | Play Stop FullScr ScrShot | Config Graphics DSP GCPad Wiimote |
ToolBar {
    id: toolbar
    width: parent.width
    height: 40
    Row {
        spacing: 2
        anchors.verticalCenter: parent.verticalCenter
        ToolButton{
            iconName: "document-open"
            anchors.verticalCenter: parent.verticalCenter
            onClicked: fileDialogLoad.open()
        }
        ToolButton{
            iconName: "view-refresh"
            anchors.verticalCenter: parent.verticalCenter
        }
        ToolButton{
            iconName: "folder-open"
            anchors.verticalCenter: parent.verticalCenter
            onClicked: fileDialogLoad.open()
            //text: "Browse"
        }
        ToolButton{
            iconName: "media-playback-start"
            anchors.verticalCenter: parent.verticalCenter
            //text: "Start"
        }
        ToolButton{
            iconName: "media-playback-stop"
            anchors.verticalCenter: parent.verticalCenter
            //text: "Stop"
        }
        ToolButton{
            iconName: "view-fullscreen"
            anchors.verticalCenter: parent.verticalCenter
            //text: "Fullscreen"
        }
        ToolButton{ // Screenshot
            iconName: "camera-photo"
            anchors.verticalCenter: parent.verticalCenter
            onClicked: fileDialogSave.open()
            //text: "Screenshot"
        }
        ToolButton{
            iconName: "preferences-other"
            //text: "Config"
            onClicked: config.visible = true
        }
        ToolButton{
            iconName: "video-display"
            //text: "Graphics"
            onClicked: graphicsconfig.visible = true
        }
        ToolButton{
            iconName: "audio-x-generic"
            //text: "DSP"
            onClicked: config.visible = true
        }
        ToolButton{
            iconName: "input-keyboard"
            //text: "GCPad"
            onClicked: padconfig.visible = true
        }
        ToolButton{
            iconName: "input-mouse"
            //text: "Wiimote"
            onClicked: wiimoteconfig.visible = true
        }
    }

    FileDialog {
        id: fileDialogLoad
        folder: currentDir
        title: "Choose a file to open"
        selectMultiple: true
        nameFilters: [ "Image files (*.png *.jpg)", "All files (*)" ]

        onAccepted: { console.log("Accepted: " + filePaths) }
    }

    FileDialog {
        id: fileDialogSave
        folder: "/tmp"
        title: "Save as..."
        modality: Qt.WindowModal
        selectExisting: false

        onAccepted: { console.log("Accepted: " + filePath) }
    }
}
