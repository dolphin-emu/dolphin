dolphin:importModule("GraphicsAPI", "1.0")

GraphicsClass = {}

function GraphicsClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

graphics = GraphicsClass:new(nil)


colorsTable = {
["BLACK"] = "0X000000ff",
["BLUE"] = "0X0000ffff",
["BROWN"] = "0X5E2605ff",
["GOLD"] = "0Xcdad00ff",
["GOLDEN"] = "0Xcdad00ff",
["GRAY"] = "0X919191ff",
["GREY"] = "0X919191ff",
["GREEN"] = "0X008b00ff",
["LIME"] = "0X00ff00ff",
["ORANGE"] = "0Xcd8500ff",
["PINK"] = "0Xee6aa7ff",
["PURPLE"] = "0X8a2be2ff",
["RED"] = "0Xff0000ff",
["SILVER"] = "0Xc0c0c0ff",
["TURQUOISE"] = "0X00ffffff",
["WHITE"] = "0Xffffffff",
["YELLOW"] = "0Xffff00ff"
}

function ParseColor(colorString)
	if colorString == nil or string.len(colorString) < 3 then
		return ""
	end
	
	firstChar = string.sub(colorString, 1, 1)
	secondChar = string.sub(colorString, 2, 2)
	
	if firstChar == "0" and (secondChar == "x" or secondChar == "X") then
		return colorString
	end
	
	if firstChar == "x" or firstChar == "X" then
		return colorString
	end
	
	colorString = string.upper(colorString)
	returnValue = colorsTable[colorString]
	
	if returnValue == nil then
		return ""
	end
	
	return returnValue
end


function GraphicsClass:drawLine(x1, y1, x2, y2, thickness, colorString)
	GraphicsAPI:drawLine(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, thickness * 1.0, ParseColor(colorString))
end

function GraphicsClass:drawRectangle(x1, y1, x2, y2, thickness, outlineColor, innerColor)
	if innerColor ~= nil then
		GraphicsAPI:drawFilledRectangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, ParseColor(innerColor))
	end
	GraphicsAPI:drawEmptyRectangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, thickness, ParseColor(outlineColor))
end

function GraphicsClass:drawTriangle(x1, y1, x2, y2, x3, y3, thickness, outlineColor, innerColor)
	if innerColor ~= nil then
		GraphicsAPI:drawFilledTriangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, x3 * 1.0, y3 * 1.0, ParseColor(innerColor))
	end
	GraphicsAPI:drawEmptyTriangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, x3 * 1.0, y3 * 1.0, thickness * 1.0, ParseColor(outlineColor))
end

function GraphicsClass:drawCircle(x1, y1, radius, thickness, outlineColor, innerColor)
	if innerColor ~= nil then
		GraphicsAPI:drawFilledCircle(x1 * 1.0, y1 * 1.0, radius * 1.0, ParseColor(innerColor))
	end
	GraphicsAPI:drawEmptyCircle(x1 * 1.0, y1 * 1.0, radius * 1.0, thickness * 1.0, ParseColor(outlineColor))
end

function GraphicsClass:drawPolygon(listOfPoints, thickness, outlineColor, innerColor)
	for key, value in pairs(listOfPoints) do
		value[1] = value[1] * 1.0
		value[2] = value[2] * 1.0
	end
	if innerColor ~= nil then
		GraphicsAPI:drawFilledPolygon(listOfPoints, ParseColor(innerColor))
	end
	GraphicsAPI:drawEmptyPolygon(listOfPoints, thickness * 1.0, ParseColor(outlineColor))
end

function GraphicsClass:drawText(x, y, textColor, textContents)
	GraphicsAPI:drawText(x * 1.0, y * 1.0, ParseColor(textColor), textContents)
end

function GraphicsClass:addCheckbox(checkboxLabel, checkboxID)
	GraphicsAPI:addCheckbox(checkboxLabel, checkboxID)
end

function GraphicsClass:getCheckboxValue(checkboxID)
	return GraphicsAPI:getCheckboxValue(checkboxID)
end

function GraphicsClass:setCheckboxValue(checkboxID, newBoolValue)
	GraphicsAPI:setCheckboxValue(checkboxID, newBoolValue)
end

function GraphicsClass:addRadioButtonGroup(radioButtonGroupID)
	GraphicsAPI:addRadioButtonGroup(radioButtonGroupID)
end

function GraphicsClass:addRadioButton(buttonName, radioButtonGroupID, radioButtonID)
	GraphicsAPI:addRadioButton(buttonName, radioButtonGroupID, radioButtonID)
end

function GraphicsClass:getRadioButtonGroupValue(radioButtonGroupID)
	return GraphicsAPI:getRadioButtonGroupValue(radioButtonGroupID)
end

function GraphicsClass:setRadioButtonGroupValue(radioButtonGroupID, newRadioIntValue)
	GraphicsAPI:setRadioButtonGroupValue(radioButtonGroupID, newRadioIntValue)
end

function GraphicsClass:addTextBox(textBoxID, textBoxLabel)
	GraphicsAPI:addTextBox(textBoxID, textBoxLabel)
end

function GraphicsClass:getTextBoxValue(textBoxID)
	return GraphicsAPI:getTextBoxValue(textBoxID)
end

function GraphicsClass:setTextBoxValue(textBoxID, newTextBoxValue)
	GraphicsAPI:setTextBoxValue(textBoxID, newTextBoxValue)
end

function GraphicsClass:addButton(buttonName, buttonID, callbackFunction, width, height)
	GraphicsAPI:addButton(buttonName, buttonID, callbackFunction, width * 1.0, height * 1.0)
end

function GraphicsClass:pressButton(buttonID)
	GraphicsAPI:pressButton(buttonID)
end

function GraphicsClass:newLine(verticalOffset)
	GraphicsAPI:newLine(verticalOffset * 1.0)
end

function GraphicsClass:beginWindow(windowName)
	GraphicsAPI:beginWindow(windowName)
end

function GraphicsClass:endWindow()
	GraphicsAPI:endWindow()
end

