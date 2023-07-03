require ("gc_controller")
require ("emu")

testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

baseRecording = "testBaseRecording.dtm"
originalSaveState = "testEarlySaveState.sav"
newMovieName = "newTestMovie.dtm"

function getDefaultValue(buttonName)
	if buttonName == "A" then
		return false
	elseif buttonName == "B" then
		return false
	elseif buttonName == "X" then
		return false
	elseif buttonName == "Y" then
		return false
	elseif buttonName == "Z" then
		return false
	elseif buttonName == "L" then
		return false
	elseif buttonName == "R" then
		return false
	elseif buttonName == "Start" then
		return false
	elseif buttonName == "Reset" then
		return false
	elseif buttonName == "dPadUp" then
		return false
	elseif buttonName == "dPadDown" then
		return false
	elseif buttonName == "dPadLeft" then
		return false
	elseif buttonName == "dPadRight" then
		return false
	elseif buttonName == "disc" then
		return false
	elseif buttonName == "getOrigin" then
		return false;
	elseif buttonName == "isConnected" then
		return true
	elseif buttonName == "triggerL" then
		return 0
	elseif buttonName == "triggerR" then
		return 0
	elseif buttonName == "analogStickX" then
		return 128
	elseif buttonName == "analogStickY" then
		return 128
	elseif buttonName == "cStickX" then
		return 128
	elseif buttonName == "cStickY" then
		return 128
	else
		io.write("Button name was unknown - was " .. tostring(buttonName) .. "\n")
		io.flush() 
		return nill
	end
end

function testSingleFrameInputFunction(testDescription, inputAlterFunction, portNumber, buttonTable, testCheckFunction)
	io.write("Test: " .. tostring(testNum) .. "\n")
	io.write("\t" .. testDescription .. "\n")
	testNum = testNum + 1
	emu:playMovie(baseRecording)
	emu:loadState(originalSaveState)
	for i = 0, 5 do
		emu:frameAdvance()
	end
	inputAlterFunction(portNumber, buttonTable)
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:saveMovie(newMovieName)
	emu:playMovie(newMovieName)
	emu:loadState(originalSaveState)
	for i = 0, 6 do
		emu:frameAdvance()
	end
	finalInputList = gc_controller:getControllerInputsForPreviousFrame(portNumber)
	if not testCheckFunction(finalInputList, buttonTable) then
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		print("Test Failed!")
		io.write("FAILURE!\n\n")
	else
		resultsTable["PASS"] = resultsTable["PASS"] + 1
		io.write("PASS!\n\n")
	end
end


function testProbabilityButton(testDescription, inputAlterFunction, buttonName, probability, expectedValue, testCheckFunction)
	numberOfFramesImpacted = 0
	io.write("Test " .. tostring(testNum) .. ":\n")
	io.write("\t" .. testDescription .. "\n")
	testNum = testNum + 1
	emu:playMovie(baseRecording)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		inputAlterFunction(buttonName, probability)
		emu:frameAdvance()
	end
	emu:saveMovie(newMovieName)
	emu:playMovie(newMovieName)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		emu:frameAdvance()
		if testCheckFunction(gc_controller:getControllerInputsForPreviousFrame(1), expectedValue) then
			numberOfFramesImpacted = numberOfFramesImpacted + 1	
		end
	end
	actualPercent = (numberOfFramesImpacted / 1001) * 100
	if actualPercent - probability < 8 and actualPercent - probability > -8 then
		io.write("\tImpacted frames made up " .. tostring(actualPercent) .. "% of all frames, which was in the acceptable range of +- 8% of the selected probability of " .. tostring(probability) .. "%\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("\tPercentage of impacted frames was " .. tostring(actualPercent) .. "%, which was not in the acceptable range of +- 8% of the probability of " .. tostring(probability) .. "%\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		print("Test Failed!")
	end
	
end

function abs(num) 
	if num >= 0 then
		return num
	else
		return num * -1
	end
end
function testProbabilityAnalogFromCurrent(testDescription, inputAlterFunction, probability, buttonName, lowerOffset, upperOffset)
	totalAnalogValue = 0
	io.write("Test " .. tostring(testNum) .. ":\n")
	io.write("\t" .. testDescription .. "\n")
	testNum = testNum + 1
	emu:playMovie(baseRecording)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		inputAlterFunction(buttonName, probability, lowerOffset, upperOffset)
		emu:frameAdvance()
	end
	emu:saveMovie(newMovieName)
	emu:playMovie(newMovieName)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		emu:frameAdvance()
		currentValue = gc_controller:getControllerInputsForPreviousFrame(1)[buttonName]
		totalAnalogValue = totalAnalogValue + currentValue
		if currentValue < 0 or currentValue > 255 or currentValue < getDefaultValue(buttonName) - lowerOffset or currentValue > getDefaultValue(buttonName) + upperOffset then
			io.write("\tERROR: current value of " .. buttonName .. " was out of range with value of " .. tostring(currentValue) .. "\nFAILURE!\n\n")
			resultsTable["FAIL"] = resultsTable["FAIL"] + 1
			print("Test Failed!")
			return
		end
	end
	finalAverageInput = totalAnalogValue / 1001
	expectedLowestValue = getDefaultValue(buttonName) - lowerOffset
	expectedHighestValue = getDefaultValue(buttonName) + upperOffset
	
	expectedFinalAverage = ((1 - probability/100) * getDefaultValue(buttonName)) + ((probability/100) * ((expectedHighestValue + expectedLowestValue)/2))

	if ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 > 10 or ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 < -10 then
		io.write("\tFinal average input of " .. tostring(finalAverageInput) .. " for " .. buttonName .. " input, which was outside the acceptable range of +- 10% of the expected final average of " .. tostring(expectedFinalAverage) .. "\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		print("Test Failed!")
	else
		io.write("\tFinal average input of " .. tostring(finalAverageInput) .. " for " .. buttonName .. " input, which was inside the acceptable range of +- 10% of the expected final average of " .. tostring(expectedFinalAverage) .. "\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	end

end

function testProbabilityAnalogFromSpecific(testDescription, inputAlterFunction, probability, buttonName, specificValue, lowerOffset, upperOffset)
	totalAnalogValue = 0
	io.write("Test " .. tostring(testNum) .. ":\n")
	io.write("\t" .. testDescription .. "\n")
	testNum = testNum + 1
	emu:playMovie(baseRecording)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		inputAlterFunction(buttonName, probability, specificValue, lowerOffset, upperOffset)
		emu:frameAdvance()
	end
	emu:saveMovie(newMovieName)
	emu:playMovie(newMovieName)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		emu:frameAdvance()
		currentValue = gc_controller:getControllerInputsForPreviousFrame(1)[buttonName]
		totalAnalogValue = totalAnalogValue + currentValue
		if currentValue < 0 or currentValue > 255 or (currentValue ~= getDefaultValue(buttonName) and (currentValue < specificValue - lowerOffset or currentValue > specificValue + upperOffset)) then
			io.write("\tERROR: current value of " .. buttonName .. " was out of range with value of " .. tostring(currentValue) .. "\nFAILURE!\n\n")
			resultsTable["FAIL"] = resultsTable["FAIL"] + 1
			print("Test Failed!")
			return
		end
	end
	finalAverageInput = totalAnalogValue / 1001
	expectedLowestValue = specificValue - lowerOffset
	expectedHighestValue = specificValue + upperOffset
	expectedFinalAverage = (1 - probability/100) * getDefaultValue(buttonName) + ((probability/100) * ((expectedHighestValue + expectedLowestValue)/2))
	
	if ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 > 10 or ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 < -10 then
		io.write("\tFinal average input of " .. tostring(finalAverageInput) .. " for " .. buttonName .. " input, which was outside the acceptable range of +- 10% of the expected final average of " .. tostring(expectedFinalAverage) .. "\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		print("Test failed!")
	else
		io.write("\tFinal average input of " .. tostring(finalAverageInput) .. " for " .. buttonName .. " input, which was inside the acceptable range of +- 10% of the expected final average of " .. tostring(expectedFinalAverage) .. "\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	end
	
end

function setInputFunction(portNumber, buttonTable)
	gc_controller:setInputs(portNumber, buttonTable)
end


function addInputFunction(portNumber, buttonTable)
	gc_controller:addInputs(portNumber, buttonTable)
end

function addOrSubtractFromCurrentAnalogValueFunction(portNumber, buttonTable)
	minusOffset = 0
	plusOffset = 0
	buttonName = ""
	for key, value in pairs(buttonTable) do
		buttonName = key
		minusOffset = value["minusOffset"]
		plusOffset = value["plusOffset"]
	end

	if minusOffset == plusOffset then
		gc_controller:addOrSubtractFromCurrentAnalogValueChance(portNumber, 100, buttonName, plusOffset)
	else
		gc_controller:addOrSubtractFromCurrentAnalogValueChance(portNumber, 100, buttonName, minusOffset, plusOffset)
	end
end

function addOrSubtractFromSpecificAnalogValueChanceFunction(portNumber, buttonTable)
	specificValue = 0
	minusOffset = 0
	plusOffset = 0
	buttonName = ""
	for key, value in pairs(buttonTable) do
		buttonName = key
		specificValue = value["specificValue"]
		minusOffset = value["minusOffset"]
		plusOffset = value["plusOffset"]
	end
	if minusOffset == plusOffset then
		gc_controller:addOrSubtractFromSpecificAnalogValueChance(portNumber, 100, buttonName, specificValue, plusOffset)
	else
		gc_controller:addOrSubtractFromSpecificAnalogValueChance(portNumber, 100, buttonName, specificValue, minusOffset, plusOffset)
	end
end

function addOrSubtractFromSpecifcAnalogValueZeroChanceFunction(portNumber, buttonTable)
	gc_controller:addOrSubtractFromSpecificAnalogValueChance(1, 0, "analogStickX", 170, 30, 45)
end

function addOrSubtractFromCurrentAnalogValueZeroChanceFunction(portNumber, buttonTable)
	gc_controller:addOrSubtractFromCurrentAnalogValueChance(1, 0, "cStickY", 20, 50)
end

function addButtonComboChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonComboChance(portNumber, 100, buttonTable,  true)
end

function testProbabilityAddButtonFlipInput(buttonName, probability)
	gc_controller:addButtonFlipChance(1, probability, buttonName)
end

function testProbabilityAddButtonPressInput(buttonName, probability)
	gc_controller:addButtonPressChance(1, probability, buttonName)
end

function testProbabilityAddButtonReleaseInput(buttonName, probability)
	gc_controller:setInputs(1, {[buttonName] = true})
	gc_controller:addButtonReleaseChance(1, probability, buttonName)
end

function testProbabilityAddOrSubtractFromCurrentAnalogValueInput(buttonName, probability, minusOffset, plusOffset)
	if minusOffset == plusOffset then
		gc_controller:addOrSubtractFromCurrentAnalogValueChance(1, probability, buttonName, plusOffset)
	else
		gc_controller:addOrSubtractFromCurrentAnalogValueChance(1, probability, buttonName, minusOffset, plusOffset)
	end
end

function testProbabilityAddOrSubtractFromSpecificAnalogValueInput(buttonName, probability, specificValue, minusOffset, plusOffset)
	if minusOffset == plusOffset then
		gc_controller:addOrSubtractFromSpecificAnalogValueChance(1, probability, buttonName, specificValue, plusOffset)
	else
		gc_controller:addOrSubtractFromSpecificAnalogValueChance(1, probability, buttonName, specificValue, minusOffset, plusOffset)
	end
end

function testProbabilityAddButtonComboInput(buttonName, probability)
	gc_controller:addButtonComboChance(1, probability, {[buttonName] = true }, true)
end

function addZeroChanceButtonComboFunction(portNumber, buttonTable)
	gc_controller:addButtonComboChance(portNumber, 0, {A = true}, true)
end

function addOverwritingButtonComboChancesFunction(portNumber, buttonTable)
	gc_controller:addButtonComboChance(portNumber, 100, {B = true, X = true},  true)
	gc_controller:addButtonComboChance(portNumber, 100, {A = true, Z = true}, true)
end

function addNonOverwritingButtonComboChancesFunction(portNumber, buttonTable)
	gc_controller:addButtonComboChance(portNumber, 100, {B = true, X = true}, false)
	gc_controller:addButtonComboChance(portNumber, 100, {A = true, Z = true}, false)
end

function addReversedButtonFlipChanceFunction(portNumber, buttonTable)
	gc_controller:setInputs(1, {B = true})
	gc_controller:addButtomComboChance(1, 100, {B = false }, true )
end

function extractButtonName(buttonTable)
	for key, value in pairs(buttonTable) do
		return key
	end
end


function testActualButtonsEqualExpectedFunction(actualButtonTable, expectedButtonTable)
	for key, value in pairs(actualButtonTable) do
		if expectedButtonTable[key] ~= nill then
			if expectedButtonTable[key] ~= value then
				return false
			end
		elseif value ~= getDefaultValue(key) then
			return false
		end
	end
	return true
end


function testActualButtonsInRangeFunction(actualButtonTable, expectedButtonTable)
	for key, value in pairs(actualButtonTable) do
		if value ~= getDefaultValue(key) then
			if expectedButtonTable[key] ~= nill then
				baseValue = getDefaultValue(key)
				if expectedButtonTable[key]["specificValue"] ~= nill then
					baseValue = expectedButtonTable[key]["specificValue"]
				end
				if value < baseValue - expectedButtonTable[key]["minusOffset"] or value > baseValue + expectedButtonTable[key]["plusOffset"] then
					return false
				end
			else
				return false
			end
		end
	end
	return true
end

function testButtonWithinBounds(actualButtonTable, buttonName, lowerBound, upperBound)
	return actualButtonTable[buttonName] >= lowerBound and actualButtonTable[buttonName] <= upperBound
end

function testButtonWithinBoundsOrDefault(actualButtonTable, buttonName, lowerBound, upperBound)
	if actualButtonTable[buttonName] ~= getDefaultValue(buttonName) then
		return actualButtonTable[buttonName] >= lowerBound and actualButtonTable[buttonName] <= upperBound
	end
	return true
end

function setInputTwiceFunction(ignoredValue, ignoredButtonTable)
	gc_controller:setInputs(1, {A = true, X = true})
	gc_controller:setInputs(1, {B = true, Z = true})
end

function testSetInputsTwice(ignoredValue, ignoreTable)
	return testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(1), {B = true, Z = true})
end

function setInputForAllControllers(ignoreValue, ignoreTable)
	gc_controller:setInputs(1, {A = true, B = true})
	gc_controller:setInputs(2, {X = true, Y = true})
	gc_controller:setInputs(3, {Z = true, B = true})
	gc_controller:setInputs(4, {A = true, X = true})
end

function addButtonComboChanceForAllControllers(ignoreValue, ignoreTable)
	gc_controller:addButtonComboChance(1, 100, {A = true, B = true}, true)
	gc_controller:addButtonComboChance(2, 100, {X = true, Y = true}, true)
	gc_controller:addButtonComboChance(3, 100, {Z = true, B = true}, true)
	gc_controller:addButtonComboChance(4, 100, {A = true, X = true}, true)
end

function testSetInputsForAllControllers() 
	if not testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(1), {A = true, B = true}) then
		return false
	elseif not testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(2), {X = true, Y = true}) then
		return false
	elseif not testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(3), {Z = true, B = true}) then
		return false
	elseif not testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(4), {A = true, X = true}) then
		return false
	else
		return true
	end

end

function testSetInputsUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting A button...", setInputFunction, i, {A = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting B button...", setInputFunction, i, {B = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting X button...", setInputFunction, i, {X = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Y button...", setInputFunction, i, {Y = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Z button...", setInputFunction, i, {Z = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting L button...", setInputFunction, i, {L = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting R button...", setInputFunction, i, {R = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Start button...", setInputFunction, i, {Start = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Reset button...", setInputFunction, i, {Reset=true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting triggerL to 183...", setInputFunction, i, {triggerL=183}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting triggerR to 112...", setInputFunction, i, {triggerR=112}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadUp...", setInputFunction, i, {dPadUp=true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadDown button...", setInputFunction, i, {dPadDown=true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadLeft button...", setInputFunction, i, {dPadLeft=true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadRight button...", setInputFunction, i, {dPadRight=true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting analogStickX to 31...", setInputFunction, i, {analogStickX=31}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting analogStickY to 90...", setInputFunction, i, {analogStickY=90}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting cStickX to 201...", setInputFunction, i, {cStickX=201}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting cStickY button to 64...", setInputFunction, i, {cStickY=64}, testActualButtonsEqualExpectedFunction)
		
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with setting A to pressed and cStickX to 232...", setInputFunction, i, {A = true, cStickX = 232}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling setInputs() on controller " .. tostring(i) .. " with setting all buttons to pressed and all analog inputs to their maximum values...", setInputFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling setInputs() on all controllers...", setInputForAllControllers, 1, {}, testSetInputsForAllControllers)
	testSingleFrameInputFunction("Calling setInput() twice in a row to make sure the 2nd call overwrites the 1st one...", setInputTwiceFunction, 1, {}, testSetInputsTwice)
end

function addInputTwiceFunction(ignoredValue, ignoredButtonTable)
	gc_controller:addInputs(1, {A = true, X = true})
	gc_controller:addInputs(1, {B = true, Z = true})
end

function testAddInputsTwice(ignoredValue, ignoredTable)
	return testActualButtonsEqualExpectedFunction(gc_controller:getControllerInputsForPreviousFrame(1), {A = true, X = true, B = true, Z = true})
end

function addInputForAllControllers(ignoreValue, ignoreTable)
	gc_controller:addInputs(1, {A = true, B = true})
	gc_controller:addInputs(2, {X = true, Y = true})
	gc_controller:addInputs(3, {Z = true, B = true})
	gc_controller:addInputs(4, {A = true, X = true})
end

function testAddInputsUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting A button...", addInputFunction, i, {A = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting B button...", addInputFunction, i, {B = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting X button...", addInputFunction, i, {X = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Y button...", addInputFunction, i, {Y = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Z button...", addInputFunction, i, {Z = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting L button...", addInputFunction, i, {L = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting R button...", addInputFunction, i, {R = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Start button...", addInputFunction, i, {Start = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Reset button...", addInputFunction, i, {Reset = true}, testActualButtonsEqualExpectedFunction)		
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting triggerL to 183...", addInputFunction, i, {triggerL = 183}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting triggerR to 112...", addInputFunction, i, {triggerR = 112}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadUp button...", addInputFunction, i, {dPadUp = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadDown button...", addInputFunction, i, {dPadDown = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadLeft button...", addInputFunction, i, {dPadLeft = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadRight button...", addInputFunction, i, {dPadRight = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting analogStickX to 131...", addInputFunction, i, {analogStickX = 131}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting analogStickY to 190...", addInputFunction, i, {analogStickY = 190}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting cStickX to 99...", addInputFunction, i, {cStickX = 99}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting cStickY to 43...", addInputFunction, i, {cStickY = 43}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInput() on controller " .. tostring(i) .. " with setting A to pressed and cStickX to 232...", addInputFunction, i, {A = true, cStickX = 232}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller  " .. tostring(i) .. " with setting all buttons to pressed and all analog inputs to their maximum values...", addInputFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling addInputs() on all controllers...", addInputForAllControllers, 1, {}, testSetInputsForAllControllers)
	testSingleFrameInputFunction("Calling addInput() twice in a row to make sure that the calls add on to each other...", addInputTwiceFunction, 1, {}, testAddInputsTwice)

end

function addButtonFlipChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
end

function addReversedButtonFlipChanceFunction(portNumber, buttonTable)
	gc_controller:addInputs(portNumber, {B = true})
	gc_controller:addButtonFlipChance(portNumber, 100, "B")
end

function addButtonFlipChanceTwiceFunction(portNumber, buttonTable)
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
end

function addButtonFlipChanceThriceFunction(portNumber, buttonTable)
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
	gc_controller:addButtonFlipChance(portNumber, 100, extractButtonName(buttonTable))
end

function addZeroChanceButtonFlipChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonFlipChance(portNumber, 0, extractButtonName(buttonTable))
end


function testAddButtonFlipChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addButtonFlipChance() on controller " .. tostring(i) .. " with 100% probability of flipping button A...", addButtonFlipChanceFunction, i, {A = true}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button B...", addButtonFlipChanceFunction, 1, {B = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button X...", addButtonFlipChanceFunction, 1, {X = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Y...", addButtonFlipChanceFunction, 1, {Y = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Z...", addButtonFlipChanceFunction, 1, {Z = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button L...", addButtonFlipChanceFunction, 1, {L = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button R...", addButtonFlipChanceFunction, 1, {R = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Start...", addButtonFlipChanceFunction, 1, {Start = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Reset...", addButtonFlipChanceFunction, 1, {Reset = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadUp...", addButtonFlipChanceFunction, 1, {dPadUp = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadDown...", addButtonFlipChanceFunction, 1, {dPadDown = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadLeft...", addButtonFlipChanceFunction, 1, {dPadLeft = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadRight...", addButtonFlipChanceFunction, 1, {dPadRight = true}, testActualButtonsEqualExpectedFunction)
	
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button B (but after setting button B to pressed)...", addReversedButtonFlipChanceFunction, 1, {B = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 twice in a row with 100% probability of flipping button X...", addButtonFlipChanceTwiceFunction, 1, {X = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 three times in a row with 100% probability of flipping button Y...", addButtonFlipChanceThriceFunction, 1, {Y = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 0% probability of flipping button A to pressed...", addZeroChanceButtonFlipChanceFunction, 1, {A = false}, testActualButtonsEqualExpectedFunction)

	testProbabilityButton("Running through 1000 frames of calling addButtonFlipChance() on controller 1 with 13% chance of flipping button B...", testProbabilityAddButtonFlipInput, "B", 13, {B = true}, testActualButtonsEqualExpectedFunction) 
end

function addButtonPressChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonPressChance(portNumber, 100, extractButtonName(buttonTable))
end

function addReversedButtonPressChanceFunction(portNumber, buttonTable)
	gc_controller:addInputs(portNumber, {B = true})
	gc_controller:addButtonPressChance(portNumber, 100, "B")
end

function addZeroChanceButtonPressChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonPressChance(portNumber, 0, extractButtonName(buttonTable))
end


function testAddButtonPressChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addButtonPressChance() on controller " .. tostring(i) .. " with 100% probability of pressing button A...", addButtonPressChanceFunction, i, {A = true}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button B...", addButtonPressChanceFunction, 1, {B = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button X...", addButtonPressChanceFunction, 1, {X = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button Y...", addButtonPressChanceFunction, 1, {Y = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button Z...", addButtonPressChanceFunction, 1, {Z = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button L...", addButtonPressChanceFunction, 1, {L = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button R...", addButtonPressChanceFunction, 1, {R = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button Start...", addButtonPressChanceFunction, 1, {Start = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button Reset...", addButtonPressChanceFunction, 1, {Reset = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button dPadUp...", addButtonPressChanceFunction, 1, {dPadUp = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button dPadDown...", addButtonPressChanceFunction, 1, {dPadDown = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button dPadLeft...", addButtonPressChanceFunction, 1, {dPadLeft = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button dPadRight...", addButtonPressChanceFunction, 1, {dPadRight = true}, testActualButtonsEqualExpectedFunction)

	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button B (but after setting button B to pressed)...", addReversedButtonPressChanceFunction, 1, {B = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonPressChance() on controller 1 with 0% probability of setting button A to pressed...", addZeroChanceButtonPressChanceFunction, 1, {A = false}, testActualButtonsEqualExpectedFunction)
	
	testProbabilityButton("Running through 1000 frames of calling addButtonPressChance() on controller 1 with 34% chance of pressing X button...", testProbabilityAddButtonPressInput, "X", 34, {X = true}, testActualButtonsEqualExpectedFunction)
end

function addButtonReleaseChanceWithSetFunction(portNumber, buttonTable)
	buttonName = extractButtonName(buttonTable)
	gc_controller:setInputs(portNumber, {[buttonName] = true})
	gc_controller:addButtonReleaseChance(portNumber, 100, extractButtonName(buttonTable))
end

function addButtonReleaseChanceFunction(portNumber, buttonTable)
	gc_controller:addButtonReleaseChance(portNumber, 100, extractButtonName(buttonTable))
end

function addButtonReleaseZeroChanceFunction(portNumber, buttonTable)
	buttonName = extractButtonName(buttonTable)
	gc_controller:setInputs(portNumber, {[buttonName] = true})
	gc_controller:addButtonReleaseChance(portNumber, 0, extractButtonName(buttonTable))
end


function testAddButtonReleaseChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller " .. tostring(i) .. " after setting A to pressed with 100% probability of releasing button A...", addButtonReleaseChanceWithSetFunction, i, {A = false}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting B to pressed with 100% probability of releasing button B...", addButtonReleaseChanceWithSetFunction, 1, {B = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting X to pressed with 100% probability of releasing button X...", addButtonReleaseChanceWithSetFunction, 1, {X = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting Y to pressed with 100% probability of releasing button Y...", addButtonReleaseChanceWithSetFunction, 1, {Y = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting Z to pressed with 100% probability of releasing button Z...", addButtonReleaseChanceWithSetFunction, 1, {Z = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting L to pressed with 100% probability of releasing button L...", addButtonReleaseChanceWithSetFunction, 1, {L = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting R to pressed with 100% probability of releasing button R...", addButtonReleaseChanceWithSetFunction, 1, {R = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting Start to pressed with 100% probability of releasing button Start...", addButtonReleaseChanceWithSetFunction, 1, {Start = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting Reset to pressed with 100% probability of releasing button Reset...", addButtonReleaseChanceWithSetFunction, 1, {Reset = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting dPadUp to pressed with 100% probability of releasing button dPadUp...", addButtonReleaseChanceWithSetFunction, 1, {dPadUp = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting dPadDown to pressed with 100% probability of releasing button dPadDown...", addButtonReleaseChanceWithSetFunction, 1, {dPadDown = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting dPadLeft to pressed with 100% probability of releasing button dPadLeft...", addButtonReleaseChanceWithSetFunction, 1, {dPadLeft = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 after setting dPadRight to pressed with 100% probability of releasing button dPadRight...", addButtonReleaseChanceWithSetFunction, 1, {dPadRight = false}, testActualButtonsEqualExpectedFunction)
	
	testSingleFrameInputFunction("Calling addButtonReleaseChance() on controller 1 with 100% probability of releasing button B...", addButtonReleaseChanceFunction, 1, {B = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonRelaseChance() on controller 1 after setting A to pressed with 0% probability of releasing button A...", addButtonReleaseZeroChanceFunction, 1, {A = true}, testActualButtonsEqualExpectedFunction)
	
	testProbabilityButton("Running through 1000 frames of calling addButtonReleaseChance() on controller 1 after setting B to pressed with 68% chance of releasing button B...", testProbabilityAddButtonReleaseInput, "B", 68, {B = false}, testActualButtonsEqualExpectedFunction) 
end
 
function testAddOrSubtractactFromCurrentAnalogValueChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller " .. tostring(i) .. " after setting a 100% probability of altering cStickX by -17 to +41...", addOrSubtractFromCurrentAnalogValueFunction, i, {cStickX = {minusOffset = 17, plusOffset = 41} }, testActualButtonsInRangeFunction)
	end
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering cStickY by -35 to +35...", addOrSubtractFromCurrentAnalogValueFunction, 1, {cStickY = {minusOffset = 35, plusOffset = 35} }, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering analogStickX by -0 to +41...", addOrSubtractFromCurrentAnalogValueFunction, 1, {analogStickX = {minusOffset = 0, plusOffset = 41} }, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering analogStickY by -62 to +0...", addOrSubtractFromCurrentAnalogValueFunction, 1, {analogStickY = {minusOffset = 62, plusOffset = 0} }, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering triggerL by -13 to +20...", addOrSubtractFromCurrentAnalogValueFunction, 1, {triggerL = {minusOffset = 13, plusOffset = 20} }, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering triggerR by -35 to +100...", addOrSubtractFromCurrentAnalogValueFunction, 1, {triggerR = {minusOffset = 35, plusOffset = 100} }, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 0% probability of altering cStickY by -20 to +50...", addOrSubtractFromCurrentAnalogValueZeroChanceFunction, 1, {cStickY = {minusOffset = 20, plusOffset = 50} }, testActualButtonsInRangeFunction)

	testProbabilityAnalogFromCurrent("Running through 1000 frames of calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 with 40% chance of altering cStickX by between -31 to +85...", testProbabilityAddOrSubtractFromCurrentAnalogValueInput, 40, "cStickX", 31, 85)
end


function testAddOrSubtractFromSpecificAnalogValueChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller " .. tostring(i) .. " after setting a 100% probability of setting cStickX to a value between 200 - 18 and 200 + 34...", addOrSubtractFromSpecificAnalogValueChanceFunction, i, {cStickX = {specificValue = 200, minusOffset = 18, plusOffset = 34 }}, testActualButtonsInRangeFunction)	
	end
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting cStickY to a value between 70 - 40 and 70 + 40...", addOrSubtractFromSpecificAnalogValueChanceFunction, 1, {cStickY = {specificValue = 70, minusOffset = 40, plusOffset = 40}}, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting analogStickX to a value between 4 - 20 and 4 + 200...", addOrSubtractFromSpecificAnalogValueChanceFunction, 1, {analogStickX = {specificValue = 4, minusOffset = 20, plusOffset = 200}}, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting analogStickY to a value between 200 - 80 and 200 + 100...", addOrSubtractFromSpecificAnalogValueChanceFunction, 1, {analogStickY = {specificValue = 200, minusOffset = 80, plusOffset = 100}}, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting triggerL to a value between 31 - 8 and 31 + 6...", addOrSubtractFromSpecificAnalogValueChanceFunction, 1, {triggerL = {specificValue = 31, minusOffset = 8, plusOffset = 6}}, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting triggerR to a value between 100 - 150 and 100 + 180...", addOrSubtractFromSpecificAnalogValueChanceFunction, 1, {triggerR = {specificValue = 100, minusOffset = 150, plusOffset = 180}}, testActualButtonsInRangeFunction)
	testSingleFrameInputFunction("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 0% probability of setting analogStickX to a value between 170 - 30 and 170 + 45...", addOrSubtractFromSpecifcAnalogValueZeroChanceFunction, 1, {analogStickX = {specificValue = 170, minusOffset = 30, plusOffset = 45}}, testActualButtonsInRangeFunction)

	testProbabilityAnalogFromSpecific("Running through 1000 frames of calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 with a 65% chance of setting cStickX to a random value between 50 - 30 and 50 + 10...", testProbabilityAddOrSubtractFromSpecificAnalogValueInput, 65, "cStickX", 50, 30, 10)
end

function testAddButtonComboChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting A button...", addButtonComboChanceFunction, i, {A = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting B button...", addButtonComboChanceFunction, i, {B = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting X button...", addButtonComboChanceFunction, i, {X = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting Y button...", addButtonComboChanceFunction, i, {Y = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting Z button...", addButtonComboChanceFunction, i, {Z = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting L button...", addButtonComboChanceFunction, i, {L = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting R button...", addButtonComboChanceFunction, i, {R = true}, testActualButtonsEqualExpectedFunction)	
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting Start button...", addButtonComboChanceFunction, i, {Start = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting Reset button...", addButtonComboChanceFunction, i, {Reset = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting triggerL to 183...", addButtonComboChanceFunction, i, {triggerL = 183}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting triggerR to 112...", addButtonComboChanceFunction, i, {triggerR = 112}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting dPadUp button...", addButtonComboChanceFunction, i, {dPadUp = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting dPadDown button...", addButtonComboChanceFunction, i, {dPadDown = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting dPadLeft button...", addButtonComboChanceFunction, i, {dPadLeft = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting dPadRight button...", addButtonComboChanceFunction, i, {dPadRight = true}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting analogStickX to 131...", addButtonComboChanceFunction, i, {analogStickX = 131}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting analogStickY to 190...", addButtonComboChanceFunction, i, {analogStickY = 190}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting cStickX to 99...", addButtonComboChanceFunction, i, {cStickX = 99}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting cStickY to 43...", addButtonComboChanceFunction, i, {cStickY = 43}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting A to pressed and cStickX to 232...", addButtonComboChanceFunction, i, {A = true, cStickX = 232}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addButtonComboChance() on controller " .. tostring(i) .. " with 100% probability of setting all buttons to pressed and all analog inputs to their maximum values...", addButtonComboChanceFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testActualButtonsEqualExpectedFunction)
	end
	testSingleFrameInputFunction("Calling addButtonComboChance() on controller 1 with 0% probability of setting button A to pressed...", addZeroChanceButtonComboFunction, 1, {A = false}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonComboChance() on all controllers...", addButtonComboChanceForAllControllers, 1, {}, testSetInputsForAllControllers)
	testSingleFrameInputFunction("Calling addButtonComboChance() with overwrite-non-specified-values set to true. First call has 100% probability of setting B and X to pressed, and 2nd call has 100% probability of setting A and Z to pressed. Final input should be just A and Z are pressed...", addOverwritingButtonComboChancesFunction, 1, {A = true, Z = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling addButtonComboChance() with overwrite-non-specified-values set to false. First call has 100% probability of setting B and X to pressed, and 2nd call has 100% probability of setting A and Z to pressed. Final input should be A, Z, X and B are pressed...", addNonOverwritingButtonComboChancesFunction, 1, {A = true, B = true, X = true, Z = true}, testActualButtonsEqualExpectedFunction)
	
	testProbabilityButton("Running through 1000 frames of calling addButtonComboChance() on controller 1 with 80% chance of pressing button B...", testProbabilityAddButtonComboInput, "B", 80, {B = true}, testActualButtonsEqualExpectedFunction) 
end

function addControllerClearChanceFunction(portNumber, buttonTable)
	gc_controller:setInputs(portNumber, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255})
	gc_controller:addControllerClearChance(portNumber, 100)
end

function addControllerClearZeroChanceFunction(portNumber, buttonTable)
	gc_controller:setInputs(portNumber, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255})
	gc_controller:addControllerClearChance(portNumber, 0)
end

function testProbabilityAddControllerClearInput(buttonName, probability)
	gc_controller:setInputs(1, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255})
	gc_controller:addControllerClearChance(1, probability)
end

function testAddControllerClearChanceUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addControllerClearChance() on controller " .. tostring(i) .. " after setting all inputs to max values with 100% chance of clearing inputs...", addControllerClearChanceFunction, i, {}, testActualButtonsEqualExpectedFunction)
		testSingleFrameInputFunction("Calling addControllerClearChance() on controller " .. tostring(i) .. " after setting all inputs to max values with 0% chance of clearing inputs...", addControllerClearZeroChanceFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, Start = true, Reset = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testActualButtonsEqualExpectedFunction)
	end
	
	testProbabilityButton("Running through 1000 frames of calling addControllerClearChance() on controller 1 with 75% chance of clearing the controller inputs to their default values...", testProbabilityAddControllerClearInput, "", 75, {}, testActualButtonsEqualExpectedFunction)
end


function setAndThenAddInputs(portNumber, buttonTable)
	gc_controller:setInputs(1, {A = true, B = true, L = true, dPadUp = true})
	gc_controller:setInputs(1, {X = true, B = false})
	gc_controller:addInputs(1, {Z = true, Y = true})
	gc_controller:addInputs(1, {Y = true, dPadUp = false})
end

function setAndThenAddAndThenProbability(portNumber, buttonTable)
	setAndThenAddInputs(portNumber, buttonTable)
	gc_controller:addButtonFlipChance(1, 100, "dPadUp")
	gc_controller:addButtonPressChance(1, 100, "Y")
end

function integrationTests()
	testSingleFrameInputFunction("Calling setInputs() and addInputs() multiple times to perform integration testing. Final results should be only X, Y and Z pressed for controller 1...", setAndThenAddInputs, 1, {X = true, Y = true, Z = true}, testActualButtonsEqualExpectedFunction)
	testSingleFrameInputFunction("Calling setInputs(), addInputs() and multiple probability functions in order to perform integration testing. Final results should be only X, Y, Z, and dPadUp pressed for controller 1...", setAndThenAddAndThenProbability, 1, {X = true, Y = true, Z = true, dPadUp = true}, testActualButtonsEqualExpectedFunction)
end

file = io.open("LuaExamplesAndTests/TestResults/LuaGameCubeControllerTestsResults.txt", "w")
io.output(file)

io.write("Running setInputs() unit tests:\n\n")
testSetInputsUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addInputs() unit tests:\n\n")
testAddInputsUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addButtonFlipChance() unit tests:\n\n")
testAddButtonFlipChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addButtonPressChance() unit tests:\n\n")
testAddButtonPressChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addButtonReleaseChance() unit tests:\n\n")
testAddButtonReleaseChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addOrSubtractFromCurrentAnalogValueChance() unit tests:\n\n")
testAddOrSubtractactFromCurrentAnalogValueChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addOrSubtractFromSpecificAnalogValueChance() unit tests:\n\n")
testAddOrSubtractFromSpecificAnalogValueChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addButtonComboChance() unit tests:\n\n")
testAddButtonComboChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running addControllerClearChance() unit tests:\n\n")
testAddControllerClearChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("Running integration tests:\n\n")
integrationTests()
io.write("----------------------------------------------------------------------------------\n\n")
io.write("\nTotal Tests: " .. tostring(testNum - 1) .. "\n")
io.write("\tTests Passed: " .. tostring(resultsTable["PASS"]) .. "\n")
io.write("\tTests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n")
print("Total Tests: " .. tostring(testNum - 1) .. "\n")
print("\tTests Passed: " .. tostring(resultsTable["PASS"]) .. "\n")
print("\tTests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n")
io.flush()
io.close()