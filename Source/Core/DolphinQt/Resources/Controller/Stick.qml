import QtQuick 2.0

Item {
    id: stick

    property string name

    property Controller controller : parent

    property string image
    property int radius: stickImage.width / 2

    property int radiusMargin: radius * 0.75

    signal update(double x, double y);

    function setPosition(x, y)
    {
        stickImage.x = -stickImage.width/2 + x * radius;
        stickImage.y = -y * radius;
    }

    Image
    {
        id: stickImage
        source: stick.image
        x: -stickImage.width/2
    }

    onUpdate: {
        setPosition(x, y);
    }

    Component.onCompleted: {
        controller.registerStick(stick.name, stick)
    }

    Button
    {
        id: buttonUp

        name: "Up"
        group: "Stick_" + stick.name

        controller: stick.controller
        image: stick.image
        showImage: false
        orientation: "top"
        margin: radiusMargin
        x: 0
        y: 0
    }

    Button
    {
        id: buttonDown

        name: "Down"
        group: "Stick_" + stick.name

        controller: stick.controller
        image: stick.image
        showImage: false
        orientation: "bottom"
        margin: radiusMargin
        x: 0
        y: 0
    }

    Button
    {
        id: buttonLeft

        controller: stick.controller

        name: "Left"
        group: "Stick_" + stick.name

        image: stick.image
        showImage: false
        orientation: "left"
        margin: radiusMargin
        x: 0
        y: 0
    }

    Button
    {
        id: buttonRight

        controller: stick.controller

        name: "Right"
        group: "Stick_" + stick.name

        image: stick.image
        showImage: false
        orientation: "right"
        margin: radiusMargin
        x: 0
        y: 0
    }
}
