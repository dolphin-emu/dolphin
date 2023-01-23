testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

baseRecording = "testBaseRecording.dtm"
originalSaveState = "testEarlySaveState.sav"

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
	elseif buttonName == "START" then
		return false
	elseif buttonName == "RESET" then
		return false
	elseif buttonName == "dPadUp" then
		return false
	elseif buttonName == "dPadDown" then
		return false
	elseif buttonName == "dPadLeft" then
		return false
	elseif buttonName == "dPadRight" then
		return false
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
		io.write("Button name was unknown - was " .. tostring(buttonName) + "\n")
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
	emu:saveMovie("setInputTest.dtm")
	emu:playMovie("setInputTest.dtm")
	emu:loadState(originalSaveState)
	for i = 0, 6 do
		emu:frameAdvance()
	end
	finalInputList = gc_controller:getControllerInputs(portNumber)
	if not testCheckFunction(finalInputList, buttonTable) then
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		io.write("FAILURE!\n\n")
	else
		resultsTable["PASS"] = resultsTable["PASS"] + 1
		io.write("PASS!\n\n")
	end
	io.flush()

end


function testProbability(testDescription, inputAlterFunction, buttonName, buttonValue, lowerOffset, upperOffset, probability, expectedValue, testCheckFunction)
	numberOfFramesImpacted = 0
	io.write("Test " .. tostring(testNum) .. ":\n")
	io.write("\t" .. testDescription .. "\n")
	testNum = testNum + 1
	emu:playMovie(baseRecording)
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		inputAlterFunction(buttonName, probability, buttonValue, lowerOffset, upperOffset)
		emu:frameAdvance()
	end
	emu:saveMovie("probabilityTest.dtm")
	emu:playMovie("probabilityTest.dtm")
	emu:loadState(originalSaveState)
	for i = 0, 1000 do
		emu:frameAdvance()
		if testCheckFunction(gc_controller:getControllerInputs(1), expectedValue) then
			numberOfFramesImpacted = numberOfFramesImpacted + 1	
		end
	end
	actualPercent = (numberOfFramesImpacted / 1001) * 100
	if actualPercent - probability < 8 and actualPercent - probability > -8 then
		io.write("\tImpacted frames made up " .. tostring(actualPercent) .. "% of all frames, which was in the acceptable range of +- 8% of the selected probability of " .. tostring(probability) .. "%\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("\tPercentage of impacted frames was " .. tostring(actualPercent) .. "%, which was not in the acceptable range of +- 8% of the probability of " .. tostring(probabiliy) .. "%\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
	
end

function testProbabilityAddButtonFlipInput(buttonName, probability, ignoredValue, ignoredLowerOffset, ignoredUpperOffset)
	gc_controller:addButtonFlipChance(1, probability, buttonName)
end

function setInputFunction(portNumber, buttonTable)
	gc_controller:setInputs(portNumber, buttonTable)
end


function addInputFunction(portNumber, buttonTable)
	gc_controller:addInputs(portNumber, buttonTable)
end


function setInputTwiceFunction(ignoredValue, ignoredButtonTable)
	gc_controller:setInputs(1, {A = true, X = true})
	gc_controller:setInputs(1, {B = true, Z = true})
end

function testSetInputsTwice(ignoredValue, ignoreTable)
	return testSetInputsFunction(gc_controller:getControllerInputs(1), {B = true, Z = true})
end

function addInputTwiceFunction(ignoredValue, ignoredButtonTable)
	gc_controller:addInputs(1, {A = true, X = true})
	gc_controller:addInputs(1, {B = true, Z = true})
end

function testAddInputsTwice(ignoredValue, ignoredTable)
	return testSetInputsFunction(gc_controller:getControllerInputs(1), {A = true, X = true, B = true, Z = true})
end

function extractButtonName(buttonTable)
	for key, value in pairs(buttonTable) do
		return key
	end
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

function testSetInputsFunction(actualButtonTable, expectedButtonTable)
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

function setInputForAllControllers(ignoreValue, ignoreTable)
	gc_controller:setInputs(1, {A = true, B = true})
	gc_controller:setInputs(2, {X = true, Y = true})
	gc_controller:setInputs(3, {Z = true, B = true})
	gc_controller:setInputs(4, {A = true, X = true})
end

function testSetInputsForAllControllers() 
	if not testSetInputsFunction(gc_controller:getControllerInputs(1), {A = true, B = true}) then
		return false
	elseif not testSetInputsFunction(gc_controller:getControllerInputs(2), {X = true, Y = true}) then
		return false
	elseif not testSetInputsFunction(gc_controller:getControllerInputs(3), {Z = true, B = true}) then
		return false
	elseif not testSetInputsFunction(gc_controller:getControllerInputs(4), {A = true, X = true}) then
		return false
	else
		return true
	end

end

function addInputForAllControllers(ignoreValue, ignoreTable)
	gc_controller:addInputs(1, {A = true, B = true})
	gc_controller:addInputs(2, {X = true, Y = true})
	gc_controller:addInputs(3, {Z = true, B = true})
	gc_controller:addInputs(4, {A = true, X = true})
end

function testSetInputsUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting A button...", setInputFunction, i, {A = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting B button...", setInputFunction, i, {B = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting X button...", setInputFunction, i, {X = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Y button...", setInputFunction, i, {Y = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting Z button...", setInputFunction, i, {Z = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting L button...", setInputFunction, i, {L = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting R button...", setInputFunction, i, {R = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting START button...", setInputFunction, i, {START = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting RESET button...", setInputFunction, i, {RESET=true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting triggerL to 183...", setInputFunction, i, {triggerL=183}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting triggerR to 112...", setInputFunction, i, {triggerR=112}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadUp...", setInputFunction, i, {dPadUp=true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadDown button...", setInputFunction, i, {dPadDown=true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadLeft button...", setInputFunction, i, {dPadLeft=true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting dPadRight button...", setInputFunction, i, {dPadRight=true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting analogStickX to 31...", setInputFunction, i, {analogStickX=31}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting analogStickY to 90...", setInputFunction, i, {analogStickY=90}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting cStickX to 201...", setInputFunction, i, {cStickX=201}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with just setting cStickY button to 64...", setInputFunction, i, {cStickY=64}, testSetInputsFunction)
		
		testSingleFrameInputFunction("Calling setInput() on controller " .. tostring(i) .. " with setting A to pressed and cStickX to 232...", setInputFunction, i, {A = true, cStickX = 232}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling setInputs() on controller  " .. tostring(i) .. " with setting all buttons to pressed and all analog inputs to their maximum values...", setInputFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, START = true, RESET = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testSetInputsFunction)
	end
	testSingleFrameInputFunction("Calling setInputs() on all controllers...", setInputForAllControllers, 1, {}, testSetInputsForAllControllers)
	testSingleFrameInputFunction("Calling setInput() twice in a row to make sure the 2nd call overwrites the 1st one...", setInputTwiceFunction, 1, {}, testSetInputsTwice)
end

function testAddInputsUnitTests()
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting A button...", addInputFunction, i, {A = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting B button...", addInputFunction, i, {B = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting X button...", addInputFunction, i, {X = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Y button...", addInputFunction, i, {Y = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting Z button...", addInputFunction, i, {Z = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting L button...", addInputFunction, i, {L = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting R button...", addInputFunction, i, {R = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting START button...", addInputFunction, i, {START = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting RESET button...", addInputFunction, i, {RESET = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting triggerL to 183...", addInputFunction, i, {triggerL = 183}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting triggerR to 112...", addInputFunction, i, {triggerR = 112}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadUp button...", addInputFunction, i, {dPadUp = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadDown button...", addInputFunction, i, {dPadDown = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadLeft button...", addInputFunction, i, {dPadLeft = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting dPadRight button...", addInputFunction, i, {dPadRight = true}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting analogStickX to 131...", addInputFunction, i, {analogStickX = 131}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting analogStickY to 190...", addInputFunction, i, {analogStickY = 190}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting cStickX to 99...", addInputFunction, i, {cStickX = 99}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller " .. tostring(i) .. " with just setting cStickY to 43...", addInputFunction, i, {cStickY = 43}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInput() on controller " .. tostring(i) .. " with setting A to pressed and cStickX to 232...", addInputFunction, i, {A = true, cStickX = 232}, testSetInputsFunction)
		testSingleFrameInputFunction("Calling addInputs() on controller  " .. tostring(i) .. " with setting all buttons to pressed and all analog inputs to their maximum values...", addInputFunction, i, {A = true, B = true, X = true, Y = true, Z = true, L = true, R = true, START = true, RESET = true, triggerL = 255, triggerR = 255, dPadUp = true, dPadDown = true, dPadLeft = true, dPadRight = true, analogStickX = 255, analogStickY = 255, cStickX = 255, cStickY = 255}, testSetInputsFunction)
	end
	testSingleFrameInputFunction("Calling addInputs() on all controllers...", addInputForAllControllers, 1, {}, testSetInputsForAllControllers)
	testSingleFrameInputFunction("Calling addInput() twice in a row to make sure that the calls add onto each other...", addInputTwiceFunction, 1, {}, testAddInputsTwice)

end


function testAddButtonFlipChanceUnitTests()
	testProbability("Running through 1000 frames of calling addButtonFlipChance() on controller 1 with 13% chance of flipping button B...", testProbabilityAddButtonFlipInput, "B", "", "", "", 13, {B = true}, testSetInputsFunction) 
	for i = 1, 4 do
		testSingleFrameInputFunction("Calling addButtonFlipChance() on controller " .. tostring(i) .. " with 100% probability of flipping button A...", addButtonFlipChanceFunction, i, {A = true}, testSetInputsFunction)
	end
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button B...", addButtonFlipChanceFunction, 1, {B = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button X...", addButtonFlipChanceFunction, 1, {X = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Y...", addButtonFlipChanceFunction, 1, {Y = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button Z...", addButtonFlipChanceFunction, 1, {Z = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button L...", addButtonFlipChanceFunction, 1, {L = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button R...", addButtonFlipChanceFunction, 1, {R = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button START...", addButtonFlipChanceFunction, 1, {START = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button RESET...", addButtonFlipChanceFunction, 1, {RESET = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadUp...", addButtonFlipChanceFunction, 1, {dPadUp = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadDown...", addButtonFlipChanceFunction, 1, {dPadDown = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadLeft...", addButtonFlipChanceFunction, 1, {dPadLeft = true}, testSetInputsFunction)
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button dPadRight...", addButtonFlipChanceFunction, 1, {dPadRight = true}, testSetInputsFunction)
	
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button B (but after setting button B to pressed)...", addReversedButtonFlipChanceFunction, 1, {B = false}, testSetInputsFunction)

	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 twice in a row with 100% probability of flipping button X...", addButtonFlipChanceTwiceFunction, 1, {X = false}, testSetInputsFunction)
	
	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 three times in a row with 100% probability of flipping button Y...", addButtonFlipChanceThriceFunction, 1, {Y = true}, testSetInputsFunction)

	testSingleFrameInputFunction("Calling addButtonFlipChance() on controller 1 with 0% probability of flipping button A to pressed...", addZeroChanceButtonFlipChanceFunction, 1, {A = false}, testSetInputsFunction)




end

file = io.open("LuaExamplesAndTests/TestResults/LuaGameCubeControllerTestsResults.txt", "w")
io.output(file)

emu:frameAdvance()
emu:frameAdvance()
testAddButtonFlipChanceUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")

testSetInputsUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")
testAddInputsUnitTests()
io.write("----------------------------------------------------------------------------------\n\n")

io.write("\nTotal Tests: " .. tostring(testNum - 1) .. "\n")
io.write("\tTests Passed: " .. tostring(resultsTable["PASS"]) .. "\n")
io.write("\tTests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n")
io.flush()
io.close()