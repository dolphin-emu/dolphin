import QtQuick 2.0

Rectangle {
    width: gcpad.width + 75
    height: gcpad.height + 75

    color: "transparent"

    GCPad {
        id: gcpad
        anchors.centerIn: parent
        scale: Math.min(parent.width / gcpad.width, parent.height / gcpad.height) * 0.85
        objectName: "gcpad"
    }

}
