import QtQuick 2.0

Item {
    id: controller

    // Output
    signal detectInput(string group, string name)

    // Input
    signal registerControl(string group, string name, variant control)
    signal registerStick(string name, variant control)

    signal detectedInput(string group, string name, string value)
    signal updateControl(string group, string name, string expression, double value)
    signal updateStick(string name, double x, double y)

    property variant controls : ({})
    property variant sticks : ({})

    onDetectedInput: {
        controller.controls[group][name].detectedInput(value);
    }

    onUpdateControl: {
        if (!controller.controls[group].hasOwnProperty(name))
            return;

        controller.controls[group][name].update(expression, value);
    }

    onUpdateStick: {
        controller.sticks[name].update(x, y);
    }

    onRegisterStick: function(name, control) {
        controller.sticks[name] = control;
    }

    onRegisterControl: function(group, name, control) {
        if (!controller.controls.hasOwnProperty(group))
            controller.controls[group] = {};

        controller.controls[group][name] = control;
    }
}
