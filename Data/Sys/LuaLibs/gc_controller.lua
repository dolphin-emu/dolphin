dolphin:importModule("GameCubeControllerAPI", "1.0.0")

GameCubeControllerAPILuaLibrary = {set_controller_input_list = {}, add_controller_input_list = {}, probability_controller_change_input_list = {}}
math.randomseed(os.time())

function checkPortNumber(portNumber, functionExample)
	if (portNumber == nil or portNumber < 1 or portNumber > 4) then
		error("Error: in gc_controller:" .. functionExample .. ", portNumber wasn't in the valid range of 1-4")
	end
end

function checkProbability(probability, functionExample)
	if probability < 0.0 or probability > 100.0 then
		error("Error: probability in gc_controller:" .. functionExample .. " was outside the acceptable range of 0-100")
	end
	probability = probability * 1.0
	return probability
end

function checkIfEventHappened(probability, functionName)
	if probability < 0.0 then
		error("Error: probability passed into gc_controller:" .. tostring(functionName) .. " function was less than 0!")
	end
	
	if probability > 100.0 then
		error("Error: probability passed into gc_controller:" .. tostring(functionName) .. " function was more than 100!")
	end
	
	local localEventHappened
	if (probability == 0.0) then
		localEventHappened = false
	elseif (probability == 100.0) then
		localEventHappened = true
	else
		localEventHappened = ((probability / 100.0) >= math.random())
	end
	
	return localEventHappened
end

function isDigitalButton(standardizedButtonName)
	if standardizedButtonName == "A" or standardizedButtonName == "B" or standardizedButtonName == "X" or standardizedButtonName == "Y"
	or standardizedButtonName == "Z" or standardizedButtonName == "L" or standardizedButtonName == "R" or standardizedButtonName == "dPadUp"
	or standardizedButtonName == "dPadDown" or standardizedButtonName == "dPadLeft" or standardizedButtonName == "dPadRight"
	or standardizedButtonName == "Start" or standardizedButtonName == "Reset" or standardizedButtonName == "disc" or standardizedButtonName == "getOrigin"
	or standardizedButtonName == "isConnected" then
		return true
	else
		return false
	end
end

function isAnalogButton(standardizedButtonName)
	if standardizedButtonName == "triggerL" or standardizedButtonName == "triggerR" or standardizedButtonName == "analogStickX" or standardizedButtonName == "analogStickY"
	or standardizedButtonName == "cStickX" or standardizedButtonName == "cStickY" then
		return true
	else
		return false
	end
end

function checkAndStandardizeButtonNameAndValue(buttonName, buttonValue)
	buttonTable = {}
	standardizedButtonName = checkAndStandardizeButtonName(buttonName)
	
	if isDigitalButton(standardizedButtonName) then
		
		if type(buttonValue) == "boolean" then
			buttonTable[standardizedButtonName] = buttonValue
		else
			error("Error: buttonName of " .. buttonName .. " needs a boolean value for its value. However, the value associated with the button was " .. buttonValue)
		end
		
	elseif isAnalogButton(standardizedButtonName) then
		if type(buttonValue) == "number" then
			if buttonValue < 0 or buttonValue > 255 then
				error("Error: value of button " .. buttonName .. " was outside the bounds of 0-255, with a value of " .. buttonValue)
			else
				buttonTable[standardizedButtonName] = math.floor(buttonValue)
			end
		else
			error("Error: buttonName of " .. buttonName .. " needs an integer value for its value. However, the value associated with the button was " .. buttonValue)
		end
	else
		error("Error: unknown buttonName of " .. buttonName .. " was encountered, which had a value of " .. buttonValue)
	end
	return buttonTable
end

function checkAndStandardizeButtonTable(buttonTable, functionExample)
	if buttonTable == nil then
		error("Error: in gc_controller:" .. functionExample .. "buttonTable was nil")
	end
	
	finalButtonTable = {}
	
	for buttonName, buttonValue in pairs(buttonTable) do
	
		tempButtonTable = checkAndStandardizeButtonNameAndValue(buttonName, buttonValue)
		for tempButton, tempValue in pairs(tempButtonTable) do
			finalButtonTable[tempButton] = tempValue
		end	
	end
	
	buttonTable = finalButtonTable
	return finalButtonTable
end

uppercaseInputButtonNameToFunctionSwitchTable = {
	["A"] = function () return "A" end,
	["B"] = function () return "B" end,
	["X"] = function () return "X" end,
	["Y"] = function () return "Y" end,
	["Z"] = function () return "Z" end,
	["L"] = function () return "L" end,
	["R"] = function () return "R" end,
	["DPADUP"] = function () return "dPadUp" end,
	["DPADDOWN"] = function () return "dPadDown" end,
	["DPADLEFT"] = function () return "dPadLeft" end,
	["DPADRIGHT"] = function () return "dPadRight" end,
	["START"] = function () return "Start" end,
	["RESET"] = function () return "Reset" end,
	["DISC"] = function () return "disc" end,
	["GETORIGIN"] = function () return "getOrigin" end,
	["ISCONNECTED"] = function () return  "isConnected" end,
	["TRIGGERL"] = function () return "triggerL" end,
	["TRIGGERR"] = function () return "triggerR" end,
	["ANALOGSTICKX"] = function () return "analogStickX" end,
	["ANALOGSTICKY"] = function () return "analogStickY" end,
	["CSTICKX"] = function () return "cStickX" end,
	["CSTICKY"] = function () return "cStickY" end
}

function checkAndStandardizeButtonName(buttonName)
	buttonName = string.upper(buttonName)
	
	buttonNameChangeFunction = uppercaseInputButtonNameToFunctionSwitchTable[buttonName]
	if buttonNameChangeFunction then
		buttonName = buttonNameChangeFunction()
		return buttonName
	else
		error("Error: unknown button name of " .. buttonName .. " was passed into function call. Valid values are A, B, X, Y, Z, L, R, dpadUp, dPadDown, dPadLeft, dPadRight, Start, Reset, triggerL, triggerR, analogStickX, analogStickY, cStickX, and cStickY")
	end
end

function GameCubeControllerAPILuaLibrary:new (o) 
	o = {}
	setmetatable(o, self)
	self.__index = self
	self.set_controller_input_list = {}
	self.add_controller_input_list = {}
	self.probability_controller_change_input_list = {}

	for i = 1, 4 do
		self.set_controller_input_list[i] = {}
		self.add_controller_input_list[i] = {}
		self.probability_controller_change_input_list[i] = {}
	end

	return o
end

gc_controller = GameCubeControllerAPILuaLibrary:new(nil)


function clearMappings()
	gc_controller.set_controller_input_list = {}
	gc_controller.add_controller_input_list = {}
	gc_controller.probability_controller_change_input_list = {}
	
	for i = 1, 4 do
		gc_controller.set_controller_input_list[i] = {}
		gc_controller.add_controller_input_list[i] = {}
		gc_controller.probability_controller_change_input_list[i] = {}
	end
end

OnFrameStart:registerWithAutoDeregistration(clearMappings)

function GameCubeControllerAPILuaLibrary:addInputs(portNumber, controllerValuesTable)
	checkPortNumber("addInputs(portNumber, controllerValuesTable)", portNumber)
	
	if (controllerValuesTable == nil) then
		error("Error: in gc_controller:addInputs(portNumber, controllerValuesTable), controllerValuesTable was nil")
	end
	
	table.insert(self.add_controller_input_list[portNumber], controllerValuesTable)
end


ProbabilityOfAlteringGCButtonClass = {}

function ProbabilityOfAlteringGCButtonClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

function ProbabilityOfAlteringGCButtonClass:applyProbability(inputtedControllerState)
	error("Error: the applyProbability() function needs to be overwritten by subclasses of the ProbabilityOfAlteringGCButtonClass class")
	return nil
end


AddButtonFlipProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddButtonFlipProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	inputtedControllerState[self.buttonName] = not inputtedControllerState[self.buttonName]
	return inputtedControllerState
end


AddButtonPressProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddButtonPressProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	inputtedControllerState[self.buttonName] = true
	return inputtedControllerState
end


AddButtonReleaseProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddButtonReleaseProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	inputtedControllerState[self.buttonName] = false
	return inputtedControllerState
end

AddOrSubtractFromCurrentAnalogValueProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddOrSubtractFromCurrentAnalogValueProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	analogValue = inputtedControllerState[self.buttonName]
	if self.amountToChangeInputBy + analogValue < 0 then
		analogValue = 0
	elseif self.amountToChangeInputBy + analogValue > 255 then
		analogValue = 255
	else
		analogValue = analogValue + self.amountToChangeInputBy
	end
	inputtedControllerState[self.buttonName] = analogValue
	return inputtedControllerState
end

AddOrSubtractFromSpecificAnalogValueProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddOrSubtractFromSpecificAnalogValueProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	inputtedControllerState[self.buttonName] = self.finalAnalogValue
	return inputtedControllerState
end

function clearControllerStateToDefaultValues(inputtedControllerState)
	inputtedControllerState["A"] = false
	inputtedControllerState["B"] = false
	inputtedControllerState["X"] = false
	inputtedControllerState["Y"] = false
	inputtedControllerState["Z"] = false
	inputtedControllerState["L"] = false
	inputtedControllerState["R"] = false
	inputtedControllerState["dPadUp"] = false
	inputtedControllerState["dPadDown"] = false
	inputtedControllerState["dPadLeft"] = false
	inputtedControllerState["dPadRight"] = false
	inputtedControllerState["Start"] = false
	inputtedControllerState["Reset"] = false
	inputtedControllerState["disc"] = false
	inputtedControllerState["getOrigin"] = false
	inputtedControllerState["isConnected"] = true
	inputtedControllerState["triggerL"] = 0
	inputtedControllerState["triggerR"] = 0
	inputtedControllerState["analogStickX"] = 128
	inputtedControllerState["analogStickY"] = 128
	inputtedControllerState["cStickX"] = 128
	inputtedControllerState["cStickY"] = 128
	return inputtedControllerState
end

AddButtonComboProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddButtonComboProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	
	if self.shouldSetUnspecifiedInputsToDefaultValues then
		inputtedControllerState = clearControllerStateToDefaultValues(inputtedControllerState)
	end
	
	for key, value in pairs(self.newButtonsTable) do
		inputtedControllerState[key] = value	
	end
	
	return inputtedControllerState
end

AddControllerClearProbabilityClass = ProbabilityOfAlteringGCButtonClass:new()

function AddControllerClearProbabilityClass:applyProbability(inputtedControllerState)
	if not self.eventHappened then
		return inputtedControllerState
	end
	
	inputtedControllerState = clearControllerStateToDefaultValues(inputtedControllerState)
	return inputtedControllerState
end


function applyRequestedInputAlterFunctions()
	currentPortNumber = OnGCControllerPolled:getCurrentPortNumberOfPoll()
	if #gc_controller.set_controller_input_list[currentPortNumber] == 0 and #gc_controller.add_controller_input_list[currentPortNumber] == 0 and #gc_controller.probability_controller_change_input_list[currentPortNumber] == 0 then
		return
	end
	currentControllerInput = OnGCControllerPolled:getInputsForPoll()
	
	for key, controllerTable in pairs(gc_controller.set_controller_input_list[currentPortNumber]) do
		currentControllerInput = controllerTable
	end
	
	for key, controllerTable in pairs(gc_controller.add_controller_input_list[currentPortNumber]) do
		for buttonName, buttonValue in pairs(controllerTable) do
			currentControllerInput[buttonName] = buttonValue
		end
	end
	
	for key, inputProbabilityEvent in pairs(gc_controller.probability_controller_change_input_list[currentPortNumber]) do
		currentControllerInput = inputProbabilityEvent:applyProbability(currentControllerInput)
	end
	OnGCControllerPolled:setInputsForPoll(currentControllerInput)
end

OnGCControllerPolled:registerWithAutoDeregistration(applyRequestedInputAlterFunctions)

-- The code below here is the public code that the user can use (the user should never try to call functions or classes listed above this point from another file, except for using the gc_controller object to call the functions specified below.

function GameCubeControllerAPILuaLibrary:setInputs(portNumber, rawControllerTable)
	checkPortNumber(portNumber, "setInputs(portNumber, controllerTable)")
	formattedUserControllerTable = checkAndStandardizeButtonTable(rawControllerTable, "setInputs(portNumber, controllerTable)")
	
	newInputs = {}
	newInputs = clearControllerStateToDefaultValues(newInputs)
	
	for key, value in pairs(formattedUserControllerTable) do
		newInputs[key] = value
	end	
	self.set_controller_input_list[portNumber] = {}
	table.insert(self.set_controller_input_list[portNumber], newInputs)
end


function GameCubeControllerAPILuaLibrary:addInputs(portNumber, rawControllerTable)
	checkPortNumber(portNumber, "addInputs(portNumber, controllerTable")
	formattedUserControllerTable = checkAndStandardizeButtonTable(rawControllerTable, "addInputs(portNumber, controllerTable)")
	table.insert(self.add_controller_input_list[portNumber], formattedUserControllerTable)
end


function GameCubeControllerAPILuaLibrary:addButtonFlipChance(portNumber, probability, buttonName)
	exampleFunctionCall = "addButtonFlipChance(portNumber, probability, buttonName)"
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	buttonName = checkAndStandardizeButtonName(buttonName)
	if not isDigitalButton(buttonName) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", " .. buttonName .. " was not a digital button")
	end
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	table.insert(self.probability_controller_change_input_list[portNumber], AddButtonFlipProbabilityClass:new({["probability"] = probability, ["buttonName"] = buttonName, ["eventHappened"] = eventHappened}))
end


function GameCubeControllerAPILuaLibrary:addButtonPressChance(portNumber, probability, buttonName)
	exampleFunctionCall = "addButtonPressChance(portNumber, probability, buttonName)"
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	buttonName = checkAndStandardizeButtonName(buttonName)
	if not isDigitalButton(buttonName) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", " .. buttonName .. " was not a digital button")
	end
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	table.insert(self.probability_controller_change_input_list[portNumber], AddButtonPressProbabilityClass:new({["probability"] = probability, ["buttonName"] = buttonName, ["eventHappened"] = eventHappened}))
end


function GameCubeControllerAPILuaLibrary:addButtonReleaseChance(portNumber, probability, buttonName)
	exampleFunctionCall = "addButtonReleaseChance(portNumber, probability, buttonName)"
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	buttonName = checkAndStandardizeButtonName(buttonName)
	if not isDigitalButton(buttonName) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", " .. buttonName .. " was not a digital button")
	end
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	table.insert(self.probability_controller_change_input_list[portNumber], AddButtonReleaseProbabilityClass:new({["probability"] = probability, ["buttonName"] = buttonName, ["eventHappened"] = eventHappened}))
end


function GameCubeControllerAPILuaLibrary:addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, ...)
	exampleFunctionCall = "addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, maxToAddOrSubtract)"
	maxToSubtract = 0
	maxToAdd = 0
	args = {...}
	maxToAdd = -1000
	maxToSubtract = -1000
	if #args == 1 then
		maxToSubtract = args[1]
		maxToAdd = args[1]
	else
		exampleFunctionCall = "addOrSubtractFromCurrentAnalogValueChance(portNumber, probability, buttonName, maxToSubtract, maxToAdd)"
		maxToSubtract = args[1]
		maxToAdd = args[2]
	end
	if maxToSubtract < 0 then
		maxToSubtract = maxToSubtract * -1
	end
	
	if maxToAdd < 0 then
		maxToAdd = maxToAdd * -1
	end
	
	maxToSubtract = math.floor(maxToSubtract)
	maxToAdd = math.floor(maxToAdd)
	
	if (maxToSubtract > 255 or maxToAdd > 255) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", amount to add or subtract was outside the bounds of 0-255")
	end
	
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	buttonName = checkAndStandardizeButtonName(buttonName)
	if not isAnalogButton(buttonName) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", " .. buttonName .. " was not an analog button")
	end
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	amountToChangeInputBy = 0
	if maxToSubtract == maxToAdd then
		amountToChangeInputBy = 0
	else
		amountToChangeInputBy = math.random(maxToSubtract * -1, maxToAdd)
	end
	table.insert(self.probability_controller_change_input_list[portNumber], AddOrSubtractFromCurrentAnalogValueProbabilityClass:new({["probability"] = probability, ["buttonName"] = buttonName, ["eventHappened"] = eventHappened, ["maxToSubtract"] = maxToSubtract, ["maxToAdd"] = maxToAdd, ["amountToChangeInputBy"] = amountToChangeInputBy}))
end


function GameCubeControllerAPILuaLibrary:addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, ...)
	exampleFunctionCall = "addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, maxToAddOrSubtract)"
	maxToSubtract = 0
	maxToAdd = 0
	args = {...}
	maxToAdd = -1000
	maxToSubtract = -1000
	if #args == 1 then
		maxToSubtract = args[1]
		maxToAdd = args[1]
	else
		exampleFunctionCall = "addOrSubtractFromSpecificAnalogValueChance(portNumber, probability, buttonName, baseValue, maxToSubtract, maxToAdd)"
		maxToSubtract = args[1]
		maxToAdd = args[2]
	end
	
	if maxToSubtract < 0 then
		maxToSubtract = maxToSubtract * -1
	end
	
	if maxToAdd < 0 then
		maxToAdd = maxToAdd * -1
	end
	
	maxToSubtract = math.floor(maxToSubtract)
	maxToAdd = math.floor(maxToAdd)
	baseValue = math.floor(baseValue)
	
	if baseValue < 0 or baseValue > 255 then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", base value was outside the acceptable range of 0-255, with a value of " .. baseValue)
	end
	
	if (maxToSubtract > 255 or maxToAdd > 255) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", amount to add or subtract was outside the bounds of 0-255")
	end
	
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	buttonName = checkAndStandardizeButtonName(buttonName)
	if not isAnalogButton(buttonName) then
		error("Error: in gc_controller:" .. exampleFunctionCall .. ", " .. buttonName .. " was not an analog button")
	end
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	
	amountToChangeInputBy = 0
	finalAnalogValue = 0
	if maxToSubtract == maxToAdd then
		amountToChangeInputBy = 0
	else
		amountToChangeInputBy = math.random(maxToSubtract * -1, maxToAdd)
	end
	
	if baseValue + amountToChangeInputBy < 0 then
		finalAnalogValue = 0
	elseif baseValue + amountToChangeInputBy > 255 then
		finalAnalogValue = 255
	else
		finalAnalogValue = baseValue + amountToChangeInputBy
	end
	table.insert(self.probability_controller_change_input_list[portNumber], AddOrSubtractFromSpecificAnalogValueProbabilityClass:new({["probability"] = probability, ["buttonName"] = buttonName, ["eventHappened"] = eventHappened, ["maxToSubtraxt"] = maxToSubtract, ["maxToAdd"] = maxToAdd, ["baseValue"] = baseValue, ["amountToChangeInputBy"] = amountToChangeInputBy, ["finalAnalogValue"] = finalAnalogValue}))
end


function GameCubeControllerAPILuaLibrary:addButtonComboChance(portNumber, probability, newButtonTable, shouldSetUnspecifiedInputsToDefaultValues)
	exampleFunctionCall = "addButtonComboChance(portNumber, probability, buttonComboTable, shouldSetUnspecifiedInputsToDefaultValues)"
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	newButtonTable = checkAndStandardizeButtonTable(newButtonTable, exampleFunctionCall)
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	table.insert(self.probability_controller_change_input_list[portNumber], AddButtonComboProbabilityClass:new({["probability"] = probability, ["buttonName"] = "", ["eventHappened"] = eventHappened, ["newButtonsTable"] = newButtonTable, ["shouldSetUnspecifiedInputsToDefaultValues"] = shouldSetUnspecifiedInputsToDefaultValues}))
end


function GameCubeControllerAPILuaLibrary:addControllerClearChance(portNumber, probability)
	exampleFunctionCall = "addControllerClearChance(portNumber, probability)"
	checkPortNumber(portNumber, exampleFunctionCall)
	probability = checkProbability(probability, exampleFunctionCall)
	eventHappened = checkIfEventHappened(probability, exampleFunctionCall)
	table.insert(self.probability_controller_change_input_list[portNumber], AddControllerClearProbabilityClass:new({["probability"] = probability, ["buttonName"] = "", ["eventHappened"] = eventHappened}))
end

function GameCubeControllerAPILuaLibrary:getControllerInputsForPreviousFrame(portNumber)
	checkPortNumber(portNumber, "getControllerInputsForPreviousFrame(portNumber)")
	return GameCubeControllerAPI:getInputsForPreviousFrame(portNumber)
end

function GameCubeControllerAPILuaLibrary:isGcControllerInPort(portNumber)
	checkPortNumber(portNumber, "isGcControllerInPort(portNumber)")
	return GameCubeControllerAPI:isGcControllerInPort(portNumber)
end

function GameCubeControllerAPILuaLibrary:isUsingPort(portNumber)
	checkPortNumber(portNumber, "isUsingPort(portNumber)")
	return GameCubeControllerAPI:isUsingPort(portNumber)
end