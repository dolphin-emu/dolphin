import QtQuick 2.0

import "../Controller"

Controller
{
    width: 650
    height: 500

    Image {
        id: image
        anchors.centerIn: parent
        source: "base.svg"
        z: -1
    }

    // Buttons
    Button {
        name: "Start"

        orientation: "bottom"

        image: "qrc:/GCPad/Start.svg"

        x: 325
        y: 175
    }

    Button {
        name: "A"

        orientation: "bottom"

        image: "qrc:/GCPad/A.svg"

        x: 535
        y: 150
    }

    Button {
        name: "B"

        image: "qrc:/GCPad/B.svg"

        x: 465
        y: 195
    }

    Button {
        name: "X"

        orientation: "bottom"

        image: "qrc:/GCPad/X.svg"

        x: 610
        y: 125
    }

    Button {
        name: "Y"

        image: "qrc:/GCPad/Y.svg"

        x: 520
        y: 100
    }

    Button {
        name: "Z"

        orientation: "right"

        image: "qrc:/GCPad/Z.svg"

        x: 535
        y: 35
        z: -2
    }

    // D-Pad
    Button {
        group: "DPad"
        name: "Up"

        image: "qrc:/GCPad/DPad_Up.svg"

        x: 211
        y: 304
    }

    Button {
        group: "DPad"
        name: "Down"

        orientation: "bottom"

        image: "qrc:/GCPad/DPad_Down.svg"

        x: 211
        y: 367
    }

    Button {
        group: "DPad"
        name: "Left"

        orientation: "left"

        image: "qrc:/GCPad/DPad_Left.svg"

        x: 179
        y: 335
    }

    Button {
        group: "DPad"
        name: "Right"
        orientation: "right"

        image: "qrc:/GCPad/DPad_Right.svg"

        x: 243
        y: 335
    }

    // Sticks
    Stick {
        name: "Main"

        image: "qrc:/GCPad/Stick_Main.svg"

        x: 110
        y: 140
    }

    Stick {
        name: "C"

        image: "qrc:/GCPad/Stick_C.svg"

        radius: 65
        radiusMargin: 10

        x: 440
        y: 315
    }

    // Triggers
    Trigger {
        name: "L"

        image: "qrc:/GCPad/Trigger_L.svg"

        x: 80
        y: 46
        z: -3
    }

    Trigger {
        name: "R"

        image: "qrc:/GCPad/Trigger_R.svg"

        x: 540
        y: 25
        z: -3
    }
}
