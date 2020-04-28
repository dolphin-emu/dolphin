import QtQuick 2.0
import QtGraphicalEffects 1.13

Item {
    id: button

    property string group : "Button"
    property string name
    property bool analog : false

    property Controller controller : parent;

    property string image
    property bool showImage: true

    property string orientation: "top"
    property int margin: 5

    property int labelWidth: 0

    property bool detecting_input : false

    // Input
    signal detectedInput(string expression);
    signal update(string expression, variant value);

    function setPressed(pressed)
    {
        buttonExpression.font.bold = pressed;
        colorOverlay.color = pressed ? Qt.rgba(0, 1, 0, 0.5) : "transparent";
    }

    function detect()
    {
        button.detecting_input = true;
        buttonExpression.text = "...";

        // We need to call the signal this way, otherwise the text update will not occur
        detectInputTimer.start();
    }

    function setExpression(expression)
    {
        if (button.detecting_input)
            return;

        buttonExpression.text = expression === "" ? "<Unmapped>" : expression;
    }

    Component.onCompleted: {
        controller.registerControl(button.group, button.name, button);
    }

    onDetectedInput: {
        button.detecting_input = false;
        setExpression(expression);
    }

    onUpdate: {
        setExpression(expression);
        setPressed(value === 1.0);

        if (button.analog)
          progressBar.progress = value;
    }

    Timer {
        id: detectInputTimer
        interval: 100
        repeat: false
        onTriggered: controller.detectInput(button.group, button.name)
    }

    Image {
        id: buttonImage
        source: button.image
        anchors.horizontalCenter: parent.horizontalCenter
        visible: button.showImage

        MouseArea
        {
            anchors.fill: parent
            onClicked: button.detect()
        }
    }

    ColorOverlay {
        id: colorOverlay
        visible: button.showImage
        anchors.fill: buttonImage
        source: buttonImage
        color: "transparent"
    }

    Rectangle {
        id: buttonRectangle

        Rectangle {
            property double progress : 0

            id: progressBar
            color: "lime"
            height: parent.height
            width: parent.width * progress
        }

        Text {
            id: buttonExpression
            text: "<Uninitialized>"
            anchors.centerIn: parent
            color: "white"
        }

        width: labelWidth == 0 ? buttonExpression.width + 5 : labelWidth
        height: buttonExpression.height + 5

        color: "black"
        radius: 3

        anchors.horizontalCenter: button.orientation == "bottom" || button.orientation == "top" ? buttonImage.horizontalCenter : undefined
        anchors.verticalCenter: button.orientation == "left" || button.orientation == "right" ? buttonImage.verticalCenter : undefined
        anchors.top: button.orientation == "bottom" ? buttonImage.bottom : undefined
        anchors.bottom: button.orientation == "top" ? buttonImage.top : undefined
        anchors.left: button.orientation == "right" ? buttonImage.right : undefined
        anchors.right: button.orientation == "left" ? buttonImage.left : undefined

        anchors.margins: button.margin

        MouseArea
        {
            anchors.fill: parent
            onClicked: button.detect()
        }
    }
}
