import dolphin
import OnFrameStart
import OnGCControllerPolled
dolphin.importModule("GameCubeControllerAPI", "1.0")
import GameCubeControllerAPI

import math
import random
import traceback
from datetime import datetime
random.seed(datetime.now().timestamp())

_INTERNAL_set_controller_inputs_array = [ [], [], [], [] ]
_INTERNAL_add_controller_inputs_array = [ [], [], [], [] ]
_INTERNAL_probability_controller_change_array = [ [], [], [], [] ]

def _INTERNAL_check_port_number(portNumber, functionExample):
    if portNumber < 1 or portNumber > 4:
        raise Exception("Error: in gc_controller." + functionExample + ", portNumber wasn't in the valid range of 1-4")

def _INTERNAL_check_probability(probability, functionExample):
    probability = 1.0 * probability
    if probability < 0.0 or probability > 100.0:
        raise Exception("Error: in gc_controller." + functionExample +", probability was outside the acceptable range of 0.0-100.0\n\n")
    return probability
    
def _INTERNAL_check_if_event_happened(probability):
    if probability < 0.0:
        raise Exception("Error: probability in gc_controller function was less than 0\n\n" + str(traceback.format_exec()))
    elif probability > 100.0:
        raise Exception("Error: probability in gc_controller function was greater than 100\n\n" + str(traceback.format_exec()))
    elif probability == 0.0:
        return False
    elif probability == 100.0:
        return True
    else:
        return ((probability * 1.0) / 100.0) >= random.random()

_INTERNAL_button_uppercase_to_button_table = {"A": "A", "B": "B", "X": "X", "Y": "Y", "Z": "Z", "L": "L", "R": "R", "DPADUP": "dPadUp", "DPADDOWN": "dPadDown", "DPADLEFT": "dPadLeft", "DPADRIGHT": "dPadRight", "START": "Start", "RESET": "Reset", "DISC" : "disc", "GETORIGIN" : "getOrigin", "ISCONNECTED" : "isConnected", "TRIGGERL": "triggerL", "TRIGGERR": "triggerR", "ANALOGSTICKX": "analogStickX", "ANALOGSTICKY": "analogStickY", "CSTICKX": "cStickX", "CSTICKY": "cStickY"}

def _INTERNAL_is_digital_button(standardizedButtonName):
    return standardizedButtonName == "A" or standardizedButtonName == "B" or standardizedButtonName == "X" or standardizedButtonName == "Y" or standardizedButtonName == "Z" or standardizedButtonName == "L" or standardizedButtonName == "R" or standardizedButtonName == "dPadUp" or standardizedButtonName == "dPadDown" or standardizedButtonName == "dPadLeft" or standardizedButtonName == "dPadRight" or standardizedButtonName == "Start" or standardizedButtonName == "Reset" or standardizedButtonName == "disc" or standardizedButtonName == "getOrigin" or standardizedButtonName == "isConnected"

def _INTERNAL_is_analog_button(standardizedButtonName):
    return standardizedButtonName == "triggerL" or standardizedButtonName == "triggerR" or standardizedButtonName == "analogStickX" or standardizedButtonName == "analogStickY" or standardizedButtonName == "cStickX" or standardizedButtonName == "cStickY"

def _INTERNAL_check_and_standardize_button_name(buttonName):
    standardizedButtonName = buttonName.upper()
    standardizedButtonName = _INTERNAL_button_uppercase_to_button_table.get(standardizedButtonName, None)
    if standardizedButtonName == None:
        raise Exception("Error: unknown button name of " + buttonName + " was passed into function call. Valid values are A, B, X, Y, Z, L, R, dpadUp, dPadDown, dPadLeft, dPadRight, Start, Reset, disc, getOrigin, isConnected, triggerL, triggerR, analogStickX, analogStickY, cStickX, and cStickY")
    return standardizedButtonName

def _INTERNAL_check_and_standardize_button_name_and_value(buttonName, buttonValue):
    buttonTable = {}
    standardizedButtonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    if _INTERNAL_is_digital_button(standardizedButtonName):
        if isinstance(buttonValue, bool):
            buttonTable[standardizedButtonName] = buttonValue
        else:
            raise Exception("Error: button name of " + standardizedButtonName + " needs a boolean for its value, but had type of " + str(type(buttonValue)) + " instead!")
    elif _INTERNAL_is_analog_button(standardizedButtonName):
        if isinstance(buttonValue, (int, float)):
            if buttonValue < 0.0 or buttonValue > 255.0:
                raise Exception("Value of button " + standardizedButtonName + " was outside the valid bounds of 0-255")
            else:
                buttonTable[standardizedButtonName] = math.floor(buttonValue)
        else:
            raise Exception("Analog button of " + standardizedButtonName + " requires a value of type int between 0-255. However, value was of type " + str(type(buttonValue)) + " instead!")
    else:
        raise Exception("Unknown button name of " + buttonName + " was encountered!")
    return buttonTable
    
def _INTERNAL_check_and_standardize_button_table(inputButtonTable, functionExample):
    finalButtonTable = {}
    
    for buttonName, buttonValue in inputButtonTable.items():
        tempButtonTable = _INTERNAL_check_and_standardize_button_name_and_value(buttonName, buttonValue)
        for tempButton, tempValue in tempButtonTable.items():
            finalButtonTable[tempButton] = tempValue
    return finalButtonTable
    
def _INTERNAL_clear_mappings():
    global _INTERNAL_set_controller_inputs_array
    global _INTERNAL_add_controller_inputs_array
    global _INTERNAL_probability_controller_change_array
    _INTERNAL_set_controller_inputs_array = [ [], [], [], [] ]
    _INTERNAL_add_controller_inputs_array = [ [], [], [], [] ]
    _INTERNAL_probability_controller_change_array = [ [], [], [], [] ]
    
OnFrameStart.registerWithAutoDeregistration(_INTERNAL_clear_mappings)

class _INTERNAL_probability_of_altering_gc_button:
    def __init__(self, buttonName, probability):
        self.buttonName = buttonName
        self.eventHappened = False
        if _INTERNAL_check_if_event_happened(probability):
            self.eventHappened = True
        
    def applyProbability(self, inputtedControllerState):
        raise Exception("This is an abstract method that should not be called directly by the base class of _INTERNAL_probability_of_altering_gc_button")

class _INTERNAL_add_button_flip_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonName, probability):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, buttonName, probability)
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        originalButtonValue = inputtedControllerState[self.buttonName]
        inputtedControllerState[self.buttonName] = not originalButtonValue
        return
        
class _INTERNAL_add_button_press_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonName, probability):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, buttonName, probability)
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        inputtedControllerState[self.buttonName] = True
        return
        
class _INTERNAL_add_button_release_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonName, probability):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, buttonName, probability)
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        inputtedControllerState[self.buttonName] = False
        return
        
class _INTERNAL_add_or_subtract_from_current_analog_value_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonName, probability, maxToSubtract, maxToAdd):
        _INTERNAL_probability_of_altering_gc_button.__init__(self,buttonName, probability)
        if maxToSubtract > 0:
            maxToSubtract = maxToSubtract * -1
        if maxToAdd < 0:
            maxToAdd = maxToAdd * -1
        self.amountToAdd = random.randint(maxToSubtract, maxToAdd)
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        newValue = inputtedControllerState[self.buttonName] + self.amountToAdd
        if newValue < 0:
            newValue = 0
        elif newValue > 255:
            newValue = 255
        inputtedControllerState[self.buttonName] = newValue

class _INTERNAL_add_or_subtract_from_specific_analog_value_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonName, probability, baseValue, maxToSubtract, maxToAdd):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, buttonName, probability)
        if maxToSubtract > 0:
            maxToSubtract = maxToSubtraxt * -1
        if maxToAdd < 0:
            maxToAdd = maxToAdd * -1
        newOffset = random.randint(maxToSubtract, maxToAdd)
        self.newAnalogValue = newOffset + baseValue
        if self.newAnalogValue < 0:
            self.newAnalogValue = 0
        elif self.newAnalogValue > 255:
            self.newAnalogValue = 255
            
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        inputtedControllerState[self.buttonName] = self.newAnalogValue
        return
        
def _INTERNAL_clearControllerStateToDefaultValues(inputtedControllerState):
    inputtedControllerState["A"] = False
    inputtedControllerState["B"] = False
    inputtedControllerState["X"] = False
    inputtedControllerState["Y"] = False
    inputtedControllerState["Z"] = False
    inputtedControllerState["L"] = False
    inputtedControllerState["R"] = False
    inputtedControllerState["dPadUp"] = False
    inputtedControllerState["dPadDown"] = False
    inputtedControllerState["dPadLeft"] = False
    inputtedControllerState["dPadRight"] = False
    inputtedControllerState["Start"] = False
    inputtedControllerState["Reset"] = False
    inputtedControllerState["disc"] = False
    inputtedControllerState["getOrigin"] = False
    inputtedControllerState["isConnected"] = True
    inputtedControllerState["triggerL"] = 0
    inputtedControllerState["triggerR"] = 0
    inputtedControllerState["analogStickX"] = 128
    inputtedControllerState["analogStickY"] = 128
    inputtedControllerState["cStickX"] = 128
    inputtedControllerState["cStickY"] = 128
    
class _INTERNAL_add_button_combo_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, buttonTable, probability, shouldSetUnspecifiedInputsToDefaultValues):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, "A", probability) #We don't use buttonName in this class, so we just set it to "A"
        self.buttonTable = buttonTable
        self.shouldSetUnspecifiedInputsToDefaultValues = shouldSetUnspecifiedInputsToDefaultValues
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        
        if self.shouldSetUnspecifiedInputsToDefaultValues:
            _INTERNAL_clearControllerStateToDefaultValues(inputtedControllerState)
            
        for buttonName, buttonValue in self.buttonTable.items():
            inputtedControllerState[buttonName] = buttonValue
         
        return
        
class _INTERNAL_add_controller_clear_probability(_INTERNAL_probability_of_altering_gc_button):
    def __init__(self, probability):
        _INTERNAL_probability_of_altering_gc_button.__init__(self, "A", probability) #We don't use buttonName in this class, so we just set it to "A"
        
    def applyProbability(self, inputtedControllerState):
        if not self.eventHappened:
            return
        _INTERNAL_clearControllerStateToDefaultValues(inputtedControllerState)
        return
        
def _INTERNAL_apply_requested_input_alter_functions():
    currentPortNumber = OnGCControllerPolled.getCurrentPortNumberOfPoll()
    
    if len(_INTERNAL_set_controller_inputs_array[currentPortNumber - 1]) == 0 and len(_INTERNAL_add_controller_inputs_array[currentPortNumber - 1]) == 0 and len(_INTERNAL_probability_controller_change_array[currentPortNumber - 1]) == 0:
        return
    currentControllerInput = OnGCControllerPolled.getInputsForPoll()
    
    for tempButtonSetTable in _INTERNAL_set_controller_inputs_array[currentPortNumber - 1]:
        currentControllerInput = tempButtonSetTable
        
    for tempButtonAddTable in _INTERNAL_add_controller_inputs_array[currentPortNumber - 1]:
        for tempButtonName, tempButtonValue in tempButtonAddTable.items():
            currentControllerInput[tempButtonName] = tempButtonValue
            
    for probabilityEventObject in _INTERNAL_probability_controller_change_array[currentPortNumber - 1]:
        probabilityEventObject.applyProbability(currentControllerInput)
        
    OnGCControllerPolled.setInputsForPoll(currentControllerInput)
    
OnGCControllerPolled.registerWithAutoDeregistration(_INTERNAL_apply_requested_input_alter_functions)


#The code below this line is the public code that the user can call in their programs (the user should never try to directly call the functions/classes listed above this line)

def setInputs(portNumber, rawControllerTable):
    exampleFunctionCall = "setInputs(portNumber, controllerInputsDictionary)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    standardizedButtonTable = _INTERNAL_check_and_standardize_button_table(rawControllerTable, exampleFunctionCall)
    
    newInputs = {}
    _INTERNAL_clearControllerStateToDefaultValues(newInputs)
    
    for buttonName, buttonValue in standardizedButtonTable.items():
        newInputs[buttonName] = buttonValue
    _INTERNAL_set_controller_inputs_array[portNumber - 1] = []
    _INTERNAL_set_controller_inputs_array[portNumber - 1].append(newInputs)
    
def addInputs(portNumber, rawControllerTable):
    exampleFunctionCall = "addInputs(portNumber, controllerInputsDictionary)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    standardizedButtonTable = _INTERNAL_check_and_standardize_button_table(rawControllerTable, exampleFunctionCall)
    _INTERNAL_add_controller_inputs_array[portNumber - 1].append(standardizedButtonTable)
    
def addButtonFlipChance(portNumber, probability, buttonName):
    exampleFunctionCall = "addButtonFlipChance(portNumber, probability, buttonName)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    buttonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    if not _INTERNAL_is_digital_button(buttonName):
        raise Exception("Error: In gc_controller." + exampleFunctionCall + ", " + buttonName + " was not a digital button.")
    else:
        _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_button_flip_probability(buttonName, probability))
        
def addButtonPressChance(portNumber, probability, buttonName):
    exampleFunctionCall = "addButtonPressChance(portNumber, probability, buttonName)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    buttonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    if not _INTERNAL_is_digital_button(buttonName):
        raise Exception("Error: In gc_controller." + exampleFunctionCall + ", " + buttonName + " was not a digital button.")
    else:
        _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_button_press_probability(buttonName, probability))
        
def addButtonReleaseChance(portNumber, probability, buttonName):
    exampleFunctionCall = "addButtonReleaseChance(portNumber, probability, buttonName)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    buttonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    if not _INTERNAL_is_digital_button(buttonName):
        raise Exception("Error: In gc_controller." + exampleFunctionCall + ", " + buttonName + " was not a digital button.")
    else:
        _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_button_release_probability(buttonName, probability))
        
def addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, maxToSubtract, maxToAdd = None):
    exampleFunctionCall = "addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, maxToSubtract, maxToAdd)"
    if maxToAdd == None:
        exampleFunctionCall = "addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, maxToAddOrSubtract)"
        maxToAdd = maxToSubtract * -1
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    buttonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    maxToSubtract = math.floor(maxToSubtract)
    maxToAdd = math.floor(maxToAdd)
    if not _INTERNAL_is_analog_button(buttonName):
        raise Exception("Error: in gc_controller." + exampleFunctionCall + ", " + buttonName + " was not an analog button.")
    if maxToSubtract > 0:
        maxToSubtract = maxToSubtract * -1
    if maxToAdd < 0:
        maxToAdd = maxToAdd * -1
    if maxToSubtract < -255 or maxToAdd > 255:
        raise Exception("Error: in gc_controller." + exampleFunctionCall + ", maximum amount to add or subtract was outside the acceptable range of -255 to 255")
    _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_or_subtract_from_current_analog_value_probability(buttonName, probability, maxToSubtract, maxToAdd))

def addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, maxToSubtract, maxToAdd = None):
    exampleFunctionCall = "addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, maxToSubtract, maxToAdd)"
    if maxToAdd == None:
        exampleFunctionCall = "addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, maxToAddOrSubtract)"
        maxToAdd = maxToSubtract * -1
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    buttonName = _INTERNAL_check_and_standardize_button_name(buttonName)
    baseValue = math.floor(baseValue)
    maxToSubtract = math.floor(maxToSubtract)
    maxToAdd = math.floor(maxToAdd)
    if not _INTERNAL_is_analog_button(buttonName):
        raise Exception("Error: in gc_controller." + exampleFunctionCall + ", " + buttonName + " was not an analog button.")
    if maxToSubtract > 0:
        maxToSubtract = maxToSubtract * -1
    if maxToAdd < 0:
        maxToAdd = maxToAdd * -1
    if maxToSubtract < -255 or maxToAdd > 255:
        raise Exception("Error: in gc_controller." + exampleFunctionCall + ", maximum amount to add or subtract was outside the acceptable range of -255 to 255")
    if baseValue < 0 or baseValue > 255:
        raise Exception("Error: in gc_controller." + exampleFunctionCall + ", baseValue was outside the acceptable range of 0-255")
    _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_or_subtract_from_specific_analog_value_probability(buttonName, probability, baseValue, maxToSubtract, maxToAdd))
    
def addButtonComboChance(portNumber, probability, newButtonTable, shouldSetUnspecifiedInputsToDefaultValues):
    exampleFunctionCall = "addButtonComboChance(portNumber, probability, newButtonTable, shouldSetUnspecifiedInputsToDefaultValues)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    newButtonTable = _INTERNAL_check_and_standardize_button_table(newButtonTable, exampleFunctionCall)
    _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_button_combo_probability(newButtonTable, probability, shouldSetUnspecifiedInputsToDefaultValues))
    
def addControllerClearChance(portNumber, probability):
    exampleFunctionCall = "addControllerClearChance(portNumber, probability)"
    _INTERNAL_check_port_number(portNumber, exampleFunctionCall)
    probability = _INTERNAL_check_probability(probability, exampleFunctionCall)
    _INTERNAL_probability_controller_change_array[portNumber - 1].append(_INTERNAL_add_controller_clear_probability(probability))
    
def getControllerInputsForPreviousFrame(portNumber):
    _INTERNAL_check_port_number(portNumber, "getControllerInputsForPreviousFrame(portNumber)")
    return GameCubeControllerAPI.getInputsForPreviousFrame(portNumber)
    
def isGcControllerInPort(portNumber):
    _INTERNAL_check_port_number(portNumber, "isGcControllerInPort(portNumber)")
    return GameCubeControllerAPI.isGcControllerInPort(portNumber)
    
def isUsingPort(portNumber):
    _INTERNAL_check_port_number(portNumber, "isUsingPort(portNumber)")
    return GameCubeControllerAPI.isUsingPort(portNumber)
