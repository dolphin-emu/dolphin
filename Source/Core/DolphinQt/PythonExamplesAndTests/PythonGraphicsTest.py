import graphics

totalTests = 1
testsFailed = 0
testsPassed = 0
YES = 0
NO = 1

WRONG_CHECKBOX_1 = 4
CORRECT_CHECKBOX = 5
WRONG_CHECKBOX_2 = 6
WRONG_CHECKBOX_3 = 7

submittedName = False
name = ""
CHECKBOX_LOCKED_TEST_ID = 77
TEXTBOX_LOCKED_TEST_ID = 78
RADIO_GROUP_LOCKED_TEST_ID = 79
pressedButtonCheckbox = False
funcRef = -1


def submitButtonClickedFunction():
    global YES
    global testsPassed
    global testsFailed
    global totalTests
    answer = graphics.getRadioButtonGroupValue(42)
    if answer == YES:
        testsPassed += 1
    else:
        testsFailed += 1
    totalTests += 1

def checkboxSubmittedFunction():
    global CORRECT_CHECKBOX
    global WRONG_CHECKBOX_1
    global WRONG_CHECKBOX_2
    global WRONG_CHECKBOX_3
    global testsPassed
    global testsFailed
    global totalTests
    answer = graphics.getCheckboxValue(CORRECT_CHECKBOX)
    wrongAnswer1 = graphics.getCheckboxValue(WRONG_CHECKBOX_1)
    wrongAnswer2 = graphics.getCheckboxValue(WRONG_CHECKBOX_2)
    wrongAnswer3 = graphics.getCheckboxValue(WRONG_CHECKBOX_3)
    if answer and not wrongAnswer1 and not wrongAnswer2 and not wrongAnswer3:
        testsPassed += 1
    else:
        testsFailed += 1
    totalTests += 1

def submittedNameFunction():
    global submittedName
    global name
    submittedName = True
    name = graphics.getTextBoxValue(42)

def submitLockedInputAnswersFunction():
    global CHECKBOX_LOCKED_TEST_ID
    global testsPassed
    global testsFailed
    global totalTests
    if graphics.getCheckboxValue(CHECKBOX_LOCKED_TEST_ID):
        testsPassed += 1
    else:
        testsFailed += 1
	
    if graphics.getCheckboxValue(TEXTBOX_LOCKED_TEST_ID):
        testsPassed += 1
    else:
        testsFailed += 1
	
    if graphics.getCheckboxValue(RADIO_GROUP_LOCKED_TEST_ID):
        testsPassed += 1
    else:
        testsFailed += 1
    totalTests += 3

def PressTestButton():
    global pressedButtonCheckbox
    pressedButtonCheckbox = True
   
def DrawHouse():
    graphics.drawTriangle( 460, 230, 660, 230, 560, 100, 15, "black", "0Xe6000dff")
    
    graphics.drawRectangle(800, 800, 300, 360, 3, "black", "TURQUOISE") #house
    
    graphics.drawRectangle(350, 730, 450, 630, 3, "black", "yellow") #lower left window
    graphics.drawLine(400, 730, 400, 630, 3, "black")
    graphics.drawLine(350, 680, 450, 680, 3, "black")
    
    graphics.drawRectangle(650, 730, 750, 630, 3, "black", "yellow") #lower right window
    graphics.drawLine(700, 730, 700, 630, 3, "black")
    graphics.drawLine(650, 680, 750, 680, 3, "black")
    
    graphics.drawRectangle(350, 525, 450, 425, 3, "black", "yellow") #upper left window
    graphics.drawLine(400, 525, 400, 425, 3, "black")
    graphics.drawLine(350, 475, 450, 475, 3,  "black")
    
    graphics.drawRectangle(650, 525, 750, 425, 3, "black", "yellow") #upper right window
    graphics.drawLine(700, 525, 700, 425, 3, "black")
    graphics.drawLine(650, 475, 750, 475, 3, "black")
    
    graphics.drawRectangle(505, 800, 605, 600, 3, "black", "brown") #door
    graphics.drawCircle(580, 700, 15, 3, "black", "golden")
    graphics.drawCircle(580, 700, 5, 3, "black", "golden")
    
    graphics.drawRectangle(450, 800, 650, 900, 3, "white", "black") #welcome mat
    graphics.drawText(500, 835, "white", "Welcome!")
    
    graphics.drawPolygon( [ [250, 399], [450, 150], [650, 150], [850, 399] ], 2, "black", "red") #roof
    graphics.drawTriangle( 460, 230, 660, 230, 560, 100, 15, "black", "0Xe6002dff")
    graphics.drawRectangle(535, 200, 590, 175, 3, "black", "golden")


def CreateHouseQuizWindow():
    global totalTests
    global submitButtonClickedFunction
    global YES
    global NO
    graphics.beginWindow("Questions Window")
    questionText = str(totalTests) + ". "
    if totalTests == 1:
        questionText = questionText + "Do you see a house drawn on the screen?"
    elif totalTests == 2:
        questionText = questionText + "Do you see a house drawn in this window?"
    elif totalTests == 3:
        questionText = questionText + "Do you see a house drawn inside the nested subwindow?"

    graphics.drawText(250, 2, "white", questionText)
    graphics.newLine(40.0)
    graphics.addRadioButtonGroup(42)
    graphics.addRadioButton("Yes", 42, YES)
    graphics.addRadioButton("No", 42, NO)
    graphics.newLine(1)
    
    graphics.addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
    
    if totalTests == 2:
        DrawHouse()
    elif totalTests == 3:
        graphics.newLine(10.0)
        graphics.beginWindow("Nested Window")
        DrawHouse()
        graphics.endWindow()
        
    graphics.endWindow()
	
    if totalTests == 1:
        DrawHouse()

def CreateCheckboxQuizWindow():
    global totalTests
    global WRONG_CHECKBOX_1
    global WRONG_CHECKBOX_2
    global CORRECT_CHECKBOX
    global WRONG_CHECKBOX_3
    global checkboxSubmittedFunction
    graphics.beginWindow("Questions Window")
    questionText = str(totalTests) + ". Click on the checkbox labeled: \"Correct Answer\""
    graphics.drawText(250, 2, "white", questionText)
    graphics.newLine(40)
    graphics.addCheckbox("First Wrong Answer", WRONG_CHECKBOX_1)
    graphics.addCheckbox("Second Wrong Answer", WRONG_CHECKBOX_2)
    graphics.addCheckbox("Correct Answer", CORRECT_CHECKBOX)
    graphics.addCheckbox("Third Wrong Answer", WRONG_CHECKBOX_3)
    graphics.newLine(1)
    graphics.addButton("Submit", 42, checkboxSubmittedFunction, 200, 100)
    graphics.endWindow()

def CreateTextboxQuizWindow():
    global totalTests
    global submittedName
    global submittedNameFunction
    global name
    global YES
    global NO
    global submitButtonClickedFunction
    graphics.beginWindow("Questions Window")
    questionText = str(totalTests) + ". "
    if not submittedName:
        questionText = questionText + "Type your name in the box:"
        graphics.drawText(250, 100, "white", questionText)
        graphics.addTextBox(42, "Name")
        graphics.newLine(1)
        graphics.addButton("Submit", 24, submittedNameFunction, 200, 100)
    else:
        questionText = questionText + "Is your name correctly displayed on the screen?"
        graphics.drawText(250, 2, "white", questionText)
        graphics.drawText(450, 180, "yellow", name)
        graphics.newLine(10)
        graphics.addRadioButtonGroup(42)
        graphics.addRadioButton("Yes", 42, YES)
        graphics.addRadioButton("No", 42, NO)
        graphics.newLine(1)
        graphics.addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
    graphics.endWindow()

def CreateLockedInputTest():
    global totalTests
    global submitLockedInputAnswersFunction
    graphics.beginWindow("Questions Window")
    questionText = "Questions " + str(totalTests) + "-" + str(totalTests + 2) + ": Check off each of the checkboxes at the bottom of this window which are true:"
    graphics.drawText(250, 2, "white", questionText)
    
    graphics.newLine(30)
    graphics.addCheckbox("Overwriting Checkbox (A)", 20)
    graphics.addCheckbox("Overwritten Checkbox (B)", 10)
    graphics.setCheckboxValue(10, graphics.getCheckboxValue(20))
    
    graphics.newLine(10)
    graphics.addTextBox(15, "Overwriting Textbox (A)")
    graphics.addTextBox(17, "Overwritten Textbox (B)")
    graphics.setTextBoxValue(17, graphics.getTextBoxValue(15))
    
    graphics.newLine(10)
    graphics.addRadioButtonGroup(13)
    graphics.addRadioButton("Ice Cream", 13, 0)
    graphics.addRadioButton("Sugar", 13, 1)
    graphics.addRadioButton("Peas", 13, 2)
    
    graphics.newLine(10)
    graphics.addRadioButtonGroup(14)
    graphics.addRadioButton("Bike", 14, 0)
    graphics.addRadioButton("Chair", 14, 1)
    graphics.addRadioButton("Car", 14, 2)
    
    graphics.setRadioButtonGroupValue(14, graphics.getRadioButtonGroupValue(13))
    
    graphics.newLine(25)
    graphics.addCheckbox("Does Clicking/unclicking checkbox A cause checkbox B to be clicked/unclicked in the same way?", 77)
    graphics.addCheckbox("Does writing text in textbox A cause textbox B to be overwritten with the same text (but not vice-versa)?", 78)
    graphics.addCheckbox("Does clicking on a radio button from the group of foods cause the same radio button to be clicked from the group of objects?", 79)
    graphics.newLine(1)
    graphics.addButton("Submit Answers", 55, submitLockedInputAnswersFunction, 300, 100)
    graphics.endWindow()

def CreateButtonPressTest():
    global pressedButtonCheckbox
    global totalTests
    global PressTestButton
    global YES
    global NO
    global submitButtonClickedFunction
	
    graphics.beginWindow("Questions Window")
    if not pressedButtonCheckbox:
        graphics.addCheckbox("Click on this checkbox...", 66)
        if graphics.getCheckboxValue(66):
            graphics.newLine(1)
            graphics.addButton("Test Button", 100, PressTestButton, 200, 100)
            graphics.pressButton(100)
    else:
        questionText = str(totalTests) + ". Do you see the string \"Hello World!\" on screen now?"
        graphics.drawText(250, 10, "white", questionText)
        graphics.drawText(400, 200, "yellow", "Hello World!")
        graphics.newLine(10)
        graphics.addRadioButtonGroup(42)
        graphics.addRadioButton("Yes", 42, YES)
        graphics.addRadioButton("No", 42, NO)
        graphics.newLine(1)
        graphics.addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
    graphics.endWindow()


def CreateQuizWindow():
    global totalTests
    if (totalTests >= 1 and totalTests <= 3):
        CreateHouseQuizWindow()
    elif totalTests == 4:
        CreateCheckboxQuizWindow()
    elif totalTests == 5:
        CreateTextboxQuizWindow()
    elif totalTests >= 6 and totalTests <= 8:
        CreateLockedInputTest()
    elif totalTests == 9:
        CreateButtonPressTest()

def mainLoop():
    global funcRef
    global totalTests
    global testsPassed
    global testsFailed
    CreateQuizWindow()
    if totalTests >= 10:
        outputFile.write("Total Tests Run: " + str(totalTests - 1) + "\n")
        outputFile.write("\tTests Passed: " + str(testsPassed) + "\n")
        outputFile.write("\tTests Failed: " + str(testsFailed))
        outputFile.flush()
        outputFile.close()
        print("Total Tests Run: " + str(totalTests - 1))
        print("\tTests Passed: " + str(testsPassed))
        print("\tTests Failed: " + str(testsFailed))
        OnFrameStart.unregister(funcRef)
        


outputFile = open("PythonExamplesAndTests/TestResults/PythonGraphicsTestResults.txt", "w")
funcRef = OnFrameStart.register(mainLoop)
