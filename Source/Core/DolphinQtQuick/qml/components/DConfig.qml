import QtQuick 1.1
import QtDesktop 0.1

Window {
    title: "Dolphin Configuration"
    visible: false
    deleteOnClose: false
    modality: Qt.ApplicationModal

    width: 500; height: 500
    ListModel {
        id: choices
        ListElement { text: "Banana" }
        ListElement { text: "Orange" }
        ListElement { text: "Apple" }
        ListElement { text: "Coconut" }
    }
    TabFrame {
        id:frame
        position: tabPositionGroup.checkedButton == r2 ? "South" : "North"
        KeyNavigation.tab:button1
        KeyNavigation.backtab: button2
        property int margins : styleitem.style == "mac" ? 16 : 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.margins: margins

        Tab {
            title: "General"
            Item {
                id: flickable
                anchors.fill: parent

                Row {
                    id: contentRow
                    anchors.fill:parent
                    anchors.margins: 8
                    spacing: 16
                    Column {
                        spacing: 9
                        Row {
                            spacing:8
                            Button {
                                id: button1
                                text:"Button 1"
                                width: 96
                                tooltip:"This is an interesting tool tip"
                                KeyNavigation.tab: button2
                                KeyNavigation.backtab: frame.tabBar
                            }
                            Button {
                                id:button2
                                text:"Button 2"
                                width:96
                                KeyNavigation.tab: combo
                                KeyNavigation.backtab: button1
                            }
                        }
                        ComboBox {
                            id: combo;
                            model: choices;
                            width: parent.width;
                            KeyNavigation.tab: t1
                            KeyNavigation.backtab: button2
                        }
                        Row {
                            spacing: 8
                            SpinBox {
                                id: t1
                                width: 97

                                minimumValue: -50
                                value: -20

                                KeyNavigation.tab: t2
                                KeyNavigation.backtab: combo
                            }
                            SpinBox {
                                id: t2
                                width:97
                                KeyNavigation.tab: t3
                                KeyNavigation.backtab: t1
                            }
                        }
                        TextField {
                            id: t3
                            KeyNavigation.tab: slider
                            KeyNavigation.backtab: t2
                            placeholderText: "This is a placeholder for a TextField"
                        }
                        ProgressBar {
                            // normalize value [0.0 .. 1.0]
                            value: (slider.value - slider.minimumValue) / (slider.maximumValue - slider.minimumValue)
                        }
                        ProgressBar {
                            indeterminate: true
                        }
                        Slider {
                            id: slider
                            value: 0.5
                            tickmarksEnabled: tickmarkCheck.checked
                            KeyNavigation.tab: frameCheckbox
                            KeyNavigation.backtab: t3
                        }
                    }
                    Column {
                        id: rightcol
                        spacing: 12
                        GroupBox {
                            id: group1
                            title: "CheckBox"
                            width: area.width
                            adjustToContentSize: true
                            ButtonRow {
                                exclusive: false
                                CheckBox {
                                    id: frameCheckbox
                                    text: "Text frame"
                                    checked: true
                                    KeyNavigation.tab: tickmarkCheck
                                    KeyNavigation.backtab: slider
                                }
                                CheckBox {
                                    id: tickmarkCheck
                                    text: "Tickmarks"
                                    checked: true
                                    KeyNavigation.tab: r1
                                    KeyNavigation.backtab: frameCheckbox
                                }
                            }
                        }
                        GroupBox {
                            id: group2
                            title:"Tab Position"
                            width: area.width
                            adjustToContentSize: true
                            ButtonRow {
                                id: tabPositionGroup
                                exclusive: true
                                RadioButton {
                                    id: r1
                                    text: "North"
                                    KeyNavigation.tab: r2
                                    KeyNavigation.backtab: tickmarkCheck
                                    checked: true
                                }
                                RadioButton {
                                    id: r2
                                    text: "South"
                                    KeyNavigation.tab: area
                                    KeyNavigation.backtab: r1
                                }
                            }
                        }

                        TextArea {
                            id: area
                            frame: frameCheckbox.checked
                            text: "Dolphin Dolphin Dolphin Dolphin Dolphin Dolphin Qt"
                            KeyNavigation.tab: button1
                        }
                    }
                }
            }
        }
        Tab {
            title: "Interface"
            Row {
                anchors.fill: parent
                anchors.margins:16
                spacing:16

                Column {
                    spacing:12

                    GroupBox {
                        title: "Animation options"
                        adjustToContentSize: true
                        ButtonRow {
                            exclusive: false
                            CheckBox {
                                id:fade
                                text: "Fade on hover"
                            }
                            CheckBox {
                                id: scale
                                text: "Scale on hover"
                            }
                        }
                    }
                    Row {
                        spacing: 20
                        Column {
                            spacing: 10
                            Button {
                                width:200
                                text: "Push button"
                                scale: scale.checked && containsMouse ? 1.1 : 1
                                opacity: !fade.checked || containsMouse ? 1 : 0.5
                                Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                                Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                            }
                            Slider {
                                value: 0.5
                                scale: scale.checked && containsMouse ? 1.1 : 1
                                opacity: !fade.checked || containsMouse ? 1 : 0.5
                                Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                                Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                            }
                            Slider {
                                id : slider1
                                value: 50
                                tickmarksEnabled: false
                                scale: scale.checked && containsMouse ? 1.1 : 1
                                opacity: !fade.checked || containsMouse ? 1 : 0.5
                                Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                                Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                            }
                            ProgressBar {
                                value: 0.5
                                scale: scale.checked && containsMouse ? 1.1 : 1
                                opacity: !fade.checked || containsMouse ? 1 : 0.5
                                Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                                Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                            }
                            ProgressBar {
                                indeterminate: true
                                scale: scale.checked && containsMouse ? 1.1 : 1
                                opacity: !fade.checked || containsMouse ? 1 : 0.5
                                Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                                Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                            }
                        }
                        Dial{
                            width: 120
                            height: 120
                            scale: scale.checked && containsMouse ? 1.1 : 1
                            opacity: !fade.checked || containsMouse ? 1 : 0.5
                            Behavior on scale { NumberAnimation { easing.type: Easing.OutCubic ; duration: 120} }
                            Behavior on opacity { NumberAnimation { easing.type: Easing.OutCubic ; duration: 220} }
                        }
                    }
                }
            }
        }
        Tab {
            title: "Audio"
        }
        Tab {
            title: "Gamecube"
        }
        Tab {
            title: "Wii"
        }
        Tab {
            title: "Paths"
        }
    }
}
