import dolphin
dolphin.importModule("GraphicsAPI", "1.0")
import GraphicsAPI


_INTERNAL_colorsTable = {}
_INTERNAL_colorsTable["BLACK"] = "0X000000ff"
_INTERNAL_colorsTable["BLUE"] = "0X0000ffff"
_INTERNAL_colorsTable["BROWN"] = "0X5E2605ff"
_INTERNAL_colorsTable["GOLD"] = "0Xcdad00ff"
_INTERNAL_colorsTable["GOLDEN"] = "0Xcdad00ff"
_INTERNAL_colorsTable["GRAY"] = "0X919191ff"
_INTERNAL_colorsTable["GREY"] = "0X919191ff"
_INTERNAL_colorsTable["GREEN"] = "0X008b00ff"
_INTERNAL_colorsTable["LIME"] = "0X00ff00ff"
_INTERNAL_colorsTable["ORANGE"] = "0Xcd8500ff"
_INTERNAL_colorsTable["PINK"] = "0Xee6aa7ff"
_INTERNAL_colorsTable["PURPLE"] = "0X8a2be2ff"
_INTERNAL_colorsTable["RED"] = "0Xff0000ff"
_INTERNAL_colorsTable["SILVER"] = "0Xc0c0c0ff"
_INTERNAL_colorsTable["TURQUOISE"] = "0X00ffffff"
_INTERNAL_colorsTable["WHITE"] = "0Xffffffff"
_INTERNAL_colorsTable["YELLOW"] = "0Xffff00ff"

def _INTERNAL_ParseColor(colorString):
    if len(colorString) < 3:
        return ""
	
    firstChar = colorString[0]
    secondChar = colorString[1]
	
    if firstChar == "0" and (secondChar == "x" or secondChar == "X"):
        return colorString
	
    if firstChar == "x" or firstChar == "X":
        return colorString
	
    colorString = colorString.upper()
    returnValue = _INTERNAL_colorsTable.get(colorString, None)
	
    if returnValue is None:
        return ""
	
    return returnValue


def drawLine(x1, y1, x2, y2, thickness, colorString):
    GraphicsAPI.drawLine(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, thickness * 1.0, _INTERNAL_ParseColor(colorString))

def drawRectangle(x1, y1, x2, y2, thickness, outlineColor, innerColor = None):
    if innerColor is not None:
        GraphicsAPI.drawFilledRectangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, _INTERNAL_ParseColor(innerColor))
    GraphicsAPI.drawEmptyRectangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, thickness, _INTERNAL_ParseColor(outlineColor))

def drawTriangle(x1, y1, x2, y2, x3, y3, thickness, outlineColor, innerColor = None):
    if innerColor is not None:
        GraphicsAPI.drawFilledTriangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, x3 * 1.0, y3 * 1.0, _INTERNAL_ParseColor(innerColor))
    GraphicsAPI.drawEmptyTriangle(x1 * 1.0, y1 * 1.0, x2 * 1.0, y2 * 1.0, x3 * 1.0, y3 * 1.0, thickness * 1.0, _INTERNAL_ParseColor(outlineColor))

def drawCircle(x1, y1, radius, thickness, outlineColor, innerColor = None):
    if innerColor is not None:
        GraphicsAPI.drawFilledCircle(x1 * 1.0, y1 * 1.0, radius * 1.0, _INTERNAL_ParseColor(innerColor))
    GraphicsAPI.drawEmptyCircle(x1 * 1.0, y1 * 1.0, radius * 1.0, thickness * 1.0, _INTERNAL_ParseColor(outlineColor))

def drawPolygon(listOfPoints, thickness, outlineColor, innerColor = None):
    for i in range(len(listOfPoints)):
        listOfPoints[i] = [listOfPoints[i][0] * 1.0, listOfPoints[i][1] * 1.0]
    if innerColor is not None:
        GraphicsAPI.drawFilledPolygon(listOfPoints, _INTERNAL_ParseColor(innerColor))
    GraphicsAPI.drawEmptyPolygon(listOfPoints, thickness * 1.0, _INTERNAL_ParseColor(outlineColor))

def drawText(x, y, textColor, textContents):
    GraphicsAPI.drawText(x * 1.0, y * 1.0, _INTERNAL_ParseColor(textColor), textContents)

def addCheckbox(checkboxLabel, checkboxID):
    GraphicsAPI.addCheckbox(checkboxLabel, checkboxID)

def getCheckboxValue(checkboxID):
    return GraphicsAPI.getCheckboxValue(checkboxID)

def setCheckboxValue(checkboxID, newBoolValue):
    GraphicsAPI.setCheckboxValue(checkboxID, newBoolValue)

def addRadioButtonGroup(radioButtonGroupID):
    GraphicsAPI.addRadioButtonGroup(radioButtonGroupID)

def addRadioButton(buttonName, radioButtonGroupID, radioButtonID):
    GraphicsAPI.addRadioButton(buttonName, radioButtonGroupID, radioButtonID)

def getRadioButtonGroupValue(radioButtonGroupID):
    return GraphicsAPI.getRadioButtonGroupValue(radioButtonGroupID)

def setRadioButtonGroupValue(radioButtonGroupID, newRadioIntValue):
    GraphicsAPI.setRadioButtonGroupValue(radioButtonGroupID, newRadioIntValue)

def addTextBox(textBoxID, textBoxLabel):
    GraphicsAPI.addTextBox(textBoxID, textBoxLabel)

def getTextBoxValue(textBoxID):
    return GraphicsAPI.getTextBoxValue(textBoxID)

def setTextBoxValue(textBoxID, newTextBoxValue):
    GraphicsAPI.setTextBoxValue(textBoxID, newTextBoxValue)

def addButton(buttonName, buttonID, callbackFunction, width, height):
    GraphicsAPI.addButton(buttonName, buttonID, callbackFunction, width * 1.0, height * 1.0)

def pressButton(buttonID):
    GraphicsAPI.pressButton(buttonID)

def newLine(verticalOffset):
    GraphicsAPI.newLine(verticalOffset * 1.0)

def beginWindow(windowName):
    GraphicsAPI.beginWindow(windowName)

def endWindow():
    GraphicsAPI.endWindow()
