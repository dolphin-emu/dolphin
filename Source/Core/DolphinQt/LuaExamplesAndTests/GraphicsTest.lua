require ("graphics")
require ("emu")

totalTests = 0
testsFailed = 0
testsPassed = 0
questionNum = 1
YES = 0
NO = 1

WRONG_CHECKBOX_1 = 4
CORRECT_CHECKBOX = 5
WRONG_CHECKBOX_2 = 6
WRONG_CHECKBOX_3 = 7

submittedName = false
name = ""
CHECKBOX_LOCKED_TEST_ID = 77
TEXTBOX_LOCKED_TEST_ID = 78
RADIO_GROUP_LOCKED_TEST_ID = 79
pressedButtonCheckbox = false

function submitButtonClickedFunction()
	answer = graphics:getRadioButtonGroupValue(42)
	if answer == YES then
		testsPassed = testsPassed + 1
	else
		testsFailed = testsFailed + 1
	end	
	totalTests = totalTests + 1
	questionNum = questionNum + 1
end

function checkboxSubmittedFunction()
	answer = graphics:getCheckboxValue(CORRECT_CHECKBOX)
	wrongAnswer1 = graphics:getCheckboxValue(WRONG_CHECKBOX_1)
	wrongAnswer2 = graphics:getCheckboxValue(WRONG_CHECKBOX_2)
	wrongAnswer3 = graphics:getCheckboxValue(WRONG_CHECKBOX_3)
	if answer and not wrongAnswer1 and not wrongAnswer2 and not wrongAnswer3 then
		testsPassed = testsPassed + 1
	else
		testsFailed = testsFailed + 1
	end
	totalTests = totalTests + 1
	questionNum = questionNum + 1
end

function submittedNameFunction()
	submittedName = true
	name = graphics:getTextBoxValue(42)
end

function submitLockedInputAnswersFunction()
	answer = graphics:getCheckboxValue(CHECKBOX_LOCKED_TEST_ID)
	if answer then
		testsPassed = testsPassed + 1
	else
		testsFailed = testsFailed + 1
	end
	
	answer = graphics:getCheckboxValue(TEXTBOX_LOCKED_TEST_ID)
	if answer then
		testsPassed = testsPassed + 1
	else
		testsFailed = testsFailed + 1
	end
	
	answer = graphics:getCheckboxValue(RADIO_GROUP_LOCKED_TEST_ID)
	if answer then
		testsPassed = testsPassed + 1
	else
		testsFailed = testsFailed + 1
	end
	totalTests = totalTests + 3
	questionNum  = questionNum + 1
end

function PressTestButton()
	pressedButtonCheckbox = true
end
function DrawHouse()
	graphics:drawTriangle( 460, 230, 660, 230, 560, 100, 15, "black", "0Xe6000dff")

	graphics:drawRectangle(800, 800, 300, 360, 3, "black", "TURQUOISE") -- house
	
	graphics:drawRectangle(350, 730, 450, 630, 3, "black", "yellow") -- lower left window
	graphics:drawLine(400, 730, 400, 630, 3, "black")
	graphics:drawLine(350, 680, 450, 680, 3, "black")
	
	graphics:drawRectangle(650, 730, 750, 630, 3, "black", "yellow") -- lower right window
	graphics:drawLine(700, 730, 700, 630, 3, "black")
	graphics:drawLine(650, 680, 750, 680, 3, "black")
	
	graphics:drawRectangle(350, 525, 450, 425, 3, "black", "yellow") -- upper left window
	graphics:drawLine(400, 525, 400, 425, 3, "black")
	graphics:drawLine(350, 475, 450, 475, 3,  "black")
	
	graphics:drawRectangle(650, 525, 750, 425, 3, "black", "yellow") -- upper right window
	graphics:drawLine(700, 525, 700, 425, 3, "black")
	graphics:drawLine(650, 475, 750, 475, 3, "black")
	
	graphics:drawRectangle(505, 800, 605, 600, 3, "black", "brown") -- door
	graphics:drawCircle(580, 700, 15, 3, "black", "golden")
	graphics:drawCircle(580, 700, 5, 3, "black", "golden")
	
	graphics:drawRectangle(450, 800, 650, 900, 3, "white", "black") -- welcome matt
	graphics:drawText(500, 835, "white", "Welcome!")
	
	graphics:drawPolygon( {  {250, 399}, {450, 150}, {650, 150}, {850, 399}}, 2, "black", "red")    --roof
	graphics:drawTriangle( 460, 230, 660, 230, 560, 100, 15, "black", "0Xe6002dff")
	graphics:drawRectangle(535, 200, 590, 175, 3, "black", "golden")
	
end


function CreateHouseQuizWindow()
	graphics:beginWindow("Questions Window")
	questionText = tostring(questionNum) .. ". "
	if questionNum == 1 then
		questionText = questionText .. "Do you see a house drawn on the screen?"
	elseif questionNum == 2 then
		questionText = questionText .. "Do you see a house drawn in this window?"
	elseif questionNum == 3 then
		questionText = questionText .. "Do you see a house drawn inside the nested subwindow?"
	end
	graphics:drawText(250, 2, "white", questionText)
	graphics:newLine(40.0)
	graphics:addRadioButtonGroup(42)
	graphics:addRadioButton("Yes", 42, YES)

	graphics:addRadioButton("No", 42, NO)
	graphics:newLine(1)
	graphics:addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
	if questionNum == 2 then
		DrawHouse()
	elseif questionNum == 3 then
		graphics:newLine(10.0)
		graphics:beginWindow("Nested Window")
		DrawHouse()
		graphics:endWindow()
	end
	graphics:endWindow()
	
	if questionNum == 1 then
		DrawHouse()
	end
end

function CreateCheckboxQuizWindow()
	graphics:beginWindow("Questions Window")
	questionText = tostring(questionNum) .. ". "
	questionText = questionText .. "Click on the checkbox labeled: \"Correct Answer\""
	graphics:drawText(250, 2, "white", questionText)
	graphics:newLine(40)
	graphics:addCheckbox("First Wrong Answer", WRONG_CHECKBOX_1)
	graphics:addCheckbox("Second Wrong Answer", WRONG_CHECKBOX_2)
	graphics:addCheckbox("Correct Answer", CORRECT_CHECKBOX)
	graphics:addCheckbox("Third Wrong Answer", WRONG_CHECKBOX_3)
	graphics:newLine(1)
	graphics:addButton("Submit", 42, checkboxSubmittedFunction, 200, 100)
	graphics:endWindow()
end

function CreateTextboxQuizWindow()
	graphics:beginWindow("Questions Window")
	questionText = tostring(questionNum) .. ". "
	if not submittedName then
		questionText = questionText .. "Type your name in the box:"
		graphics:drawText(250, 100, "white", questionText)
		graphics:addTextBox(42, "Name")
		graphics:newLine(1)
		graphics:addButton("Submit", 24, submittedNameFunction, 200, 100)
	else
		questionText = questionText .. "Is your name correctly displayed on the screen?"
		graphics:drawText(250, 2, "white", questionText)
		graphics:drawText(450, 180, "yellow", name)
		graphics:newLine(10)
		graphics:addRadioButtonGroup(42)
		graphics:addRadioButton("Yes", 42, YES)
		graphics:addRadioButton("No", 42, NO)
		graphics:newLine(1)
		graphics:addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
	end
	graphics:endWindow()
end

function CreateLockedInputTest()
	graphics:beginWindow("Questions Window")
	questionText = tostring(questionNum) .. ". "
	questionText = questionText .. "Check off each of the checkboxes at the bottom of this window which are true:"
	graphics:drawText(250, 2, "white", questionText)
	
	graphics:newLine(30)
	graphics:addCheckbox("Overwriting Checkbox (A)", 20)
	graphics:addCheckbox("Overwritten Checkbox (B)", 10)
	graphics:setCheckboxValue(10, graphics:getCheckboxValue(20))
	
	graphics:newLine(10)
	graphics:addTextBox(15, "Overwriting Textbox (A)")
	graphics:addTextBox(17, "Overwritten Textbox (B)")
	graphics:setTextBoxValue(17, graphics:getTextBoxValue(15))
	
	graphics:newLine(10)
	graphics:addRadioButtonGroup(13)
	graphics:addRadioButton("Ice Cream", 13, 0)
	graphics:addRadioButton("Sugar", 13, 1)
	graphics:addRadioButton("Peas", 13, 2)
	
	graphics:newLine(10)
	graphics:addRadioButtonGroup(14)
	graphics:addRadioButton("Bike", 14, 0)
	graphics:addRadioButton("Chair", 14, 1)
	graphics:addRadioButton("Car", 14, 2)
	
	graphics:setRadioButtonGroupValue(14, graphics:getRadioButtonGroupValue(13))
	
	graphics:newLine(25)
	graphics:addCheckbox("Does Clicking/unclicking checkbox A cause checkbox B to be clicked/unclicked in the same way?", 77)
	graphics:addCheckbox("Does writing text in textbox A cause textbox B to be overwritten with the same text (but not vice-versa)?", 78)
	graphics:addCheckbox("Does clicking on a radio button from the group of foods cause the same radio button to be clicked from the group of objects?", 79)
	graphics:newLine(1)
	graphics:addButton("Submit Answers", 55, submitLockedInputAnswersFunction, 300, 100)
	graphics:endWindow()
end

function CreateButtonPressTest()
	graphics:beginWindow("Questions Window")
	if not pressedButtonCheckbox then
		graphics:addCheckbox("Click on this checkbox...", 66)
		if graphics:getCheckboxValue(66) then
			graphics:newLine(1)
			graphics:addButton("Test Button", 100, PressTestButton, 200, 100)
			graphics:pressButton(100)
		end
	else
		questionText = tostring(questionNum) .. ". "
		questionText = questionText .. "Do you see the string \"Hello World!\" on screen now?"
		graphics:drawText(250, 10, "white", questionText)
		graphics:drawText(400, 200, "yellow", "Hello World!")
		graphics:newLine(10)
		graphics:addRadioButtonGroup(42)
		graphics:addRadioButton("Yes", 42, YES)
		graphics:addRadioButton("No", 42, NO)
		graphics:newLine(1)
		graphics:addButton("Submit", 42, submitButtonClickedFunction, 200, 100)
	end
	graphics:endWindow()
end


function CreateQuizWindow()
	if (questionNum >= 1 and questionNum <= 3) then
		CreateHouseQuizWindow()
	elseif questionNum == 4 then
		CreateCheckboxQuizWindow()
	elseif questionNum == 5 then
		CreateTextboxQuizWindow()
	elseif questionNum == 6 then
		CreateLockedInputTest()
	elseif questionNum == 7 then
		CreateButtonPressTest()
	end
end

function mainLoop()
	while true do
		CreateQuizWindow()
		if questionNum >= 8 then
			return
		end
		emu:frameAdvance()
	end
end

file = io.open("LuaExamplesAndTests/TestResults/GraphicsTestResults.txt", "w")
io.output(file)
mainLoop()
io.write("Total Tests Run: " .. tostring(totalTests) .. "\n")
io.write("\tTests Passed: " .. tostring(testsPassed) .. "\n")
io.write("\tTests Failed: " .. tostring(testsFailed))
print("Total Tests Run: " .. tostring(totalTests) .. "\n")
print("\tTests Passed: " .. tostring(testsPassed) .. "\n")
print("\tTests Failed: " .. tostring(testsFailed))
io.flush()
io.close()