import gc_controller
import emu
import dolphin_statistics

testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

baseRecording = "testBaseRecording.dtm"
originalSaveState = "testEarlySaveState.sav"
newMovieName = "newTestMovie.dtm"

outputFile = open("PythonExamplesAndTests/TestResults/PythonGameCubeControllerTestsResults.txt", "w")
testSuiteNumber = 1
outputtedSetInputsTestsHeader = False
outputtedAddInputsTestsHeader = False
outputtedAddButtonFlipChanceTestsHeader = False
outputtedAddButtonPressChanceTestsHeader = False
outputtedAddButtonReleaseChanceTestsHeader = False
outputtedAddOrSubtractFromCurrentAnalogValueChanceHeader = False
outputtedAddOrSubtractFromSpecificAnalogValueChanceHeader = False
outputtedAddButtonComboChanceHeader = False
outputtedAddControllerClearChanceHeader = False
outputtedIntegrationTestsHeader = False
def MainFunction():
    global testSuiteNumber
    global funcRef
    global outputtedSetInputsTestsHeader
    global outputtedAddInputsTestsHeader
    global outputtedAddButtonFlipChanceTestsHeader
    global outputtedAddButtonPressChanceTestsHeader
    global outputtedAddButtonReleaseChanceTestsHeader
    global outputtedAddOrSubtractFromCurrentAnalogValueChanceHeader
    global outputtedAddOrSubtractFromSpecificAnalogValueChanceHeader
    global outputtedAddButtonComboChanceHeader
    global outputtedAddControllerClearChanceHeader
    global outputtedIntegrationTestsHeader
        
    if testSuiteNumber == 1:
        if not outputtedSetInputsTestsHeader:
            outputFile.write("Running setInputs() unit tests:\n\n")
            print("Running setInputs() unit tests...")
            outputtedSetInputsTestsHeader = True
        testSetInputsUnitTests()
    elif testSuiteNumber == 2:
        testSetInputsMiscTests()
       
       
    elif testSuiteNumber == 3:
        if not outputtedAddInputsTestsHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addInputs() unit tests:\n\n")
            print("Running addInputs() unit tests...")
            outputtedAddInputsTestsHeader = True
        testAddInputsUnitTests()
    elif testSuiteNumber == 4:
        testAddInputsMiscTests()
        
        
    elif testSuiteNumber == 5:
        if not outputtedAddButtonFlipChanceTestsHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addButtonFlipChance() unit tests:\n\n")
            print("Running addButtonFlipChance() unit tests...")
            outputtedAddButtonFlipChanceTestsHeader = True
        testAddButtonFlipChanceUnitTests()
    elif testSuiteNumber == 6:
        testAddButtonFlipChanceMiscTests()
    elif testSuiteNumber == 7:
        testAddButtonFlipMultiFrameTest()
       
       
    elif testSuiteNumber == 8:
        if not outputtedAddButtonPressChanceTestsHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addButtonPressChance() unit tests:\n\n")
            print("Running addButtonPressChance() unit tests...")
            outputtedAddButtonPressChanceTestsHeader = True
        testAddButtonPressChanceUnitTests()
    elif testSuiteNumber == 9:
        testAddButtonPressChanceMiscTests()
    elif testSuiteNumber == 10:
        testAddButtonPressMultiFrameTest()
        
        
    elif testSuiteNumber == 11:
        if not outputtedAddButtonReleaseChanceTestsHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addButtonReleaseChance() unit tests:\n\n")
            print("Running addButtonReleaseChance() unit tests...")
            outputtedAddButtonReleaseChanceTestsHeader = True
        testAddButtonReleaseChanceUnitTests()
    elif testSuiteNumber == 12:
        testAddButtonReleaseMiscTests()
    elif testSuiteNumber == 13:
        testAddButtonReleaseMultiFrameTest()
        
        
    elif testSuiteNumber == 14:
        if not outputtedAddOrSubtractFromCurrentAnalogValueChanceHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addOrSubtractFromCurrentAnalogValueChance() unit tests:\n\n")
            print("Running addOrSubtractFromCurrentAnalogValueChance() unit tests...")
            outputtedAddOrSubtractFromCurrentAnalogValueChanceHeader = True
        testAddOrSubtractFromCurrentAnalogValueChanceUnitTests()
    elif testSuiteNumber == 15:
        testAddOrSubtractFromCurrentAnalogValueChanceMiscTests()
    elif testSuiteNumber == 16:
        testAddOrSubtractFromCurrentAnalogValueMultiFrameTest()
        
        
    elif testSuiteNumber == 17:
        if not outputtedAddOrSubtractFromSpecificAnalogValueChanceHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addOrSubtractFromSpecificAnalogValueChance() unit tests:\n\n")
            print("Running addOrSubtractFromSpecificAnalogValueChance() unit tests...")
            outputtedAddOrSubtractFromSpecificAnalogValueChanceHeader = True
        testAddOrSubtractFromSpecificAnalogValueChanceUnitTests()
    elif testSuiteNumber == 18:
        testAddOrSubtractFromSpecificAnalogValueChanceMiscTests()
    elif testSuiteNumber == 19:
        testAddOrSubtractFromSpecificAnalogValueMultiFrameTest()
        
    elif testSuiteNumber == 20:
        if not outputtedAddButtonComboChanceHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addButtonComboChance() unit tests:\n\n")
            print("Running addButtonComboChance() unit tests...")
            outputtedAddButtonComboChanceHeader = True
        testAddButtonComboChanceUnitTests()
    elif testSuiteNumber == 21:
        testAddButtonComboMiscTests()
    elif testSuiteNumber == 22:
        testAddButtonComboMultiFrameTest()
        
        
    elif testSuiteNumber == 23:
        if not outputtedAddControllerClearChanceHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning addControllerClearChance() unit tests:\n\n")
            print("Running addControllerClearChance() unit tests...")
            outputtedAddControllerClearChanceHeader = True
        testAddControllerClearChanceUnitTests()
    elif testSuiteNumber == 24:
        testAddControllerClearMultiFrameTest()
        
    elif testSuiteNumber == 25:
        if not outputtedIntegrationTestsHeader:
            outputFile.write("----------------------------------------------------------------------------------\n\nRunning integration tests:\n\n")
            print("Running integration tests...")
            outputtedIntegrationTestsHeader = True
        integrationTests()
        
    elif testSuiteNumber == 26:
        outputFile.write("----------------------------------------------------------------------------------\n\n")
        outputFile.write("\nTotal Tests: " + str(testNum - 1) + "\n")
        outputFile.write("\tTests Passed: " + str(resultsTable["PASS"]) + "\n")
        outputFile.write("\tTests Failed: " + str(resultsTable["FAIL"]) + "\n")
        outputFile.flush()
        outputFile.close()
        print("----------------------------------------------------------------------------------")
        print("\nTotal Tests: " + str(testNum - 1) + "\n")
        print("\tTests Passed: " + str(resultsTable["PASS"]) + "\n")
        print("\tTests Failed: " + str(resultsTable["FAIL"]) + "\n")
        OnFrameStart.unregister(funcRef)
    else:
        OnFrameStart.unregister(funcRef)


controllerNumberForTest = 1
indexInListOfAllCases = 0
listOfExampleCasesForMultiButtonFunctions = [ {"A": True}, {"B": True}, {"X": True}, {"Y": True}, {"Z": True}, {"L": True}, {"R": True}, {"Start": True}, {"Reset": True}, {"triggerL": 183}, {"triggerR": 112}, {"dPadUp": True}, {"dPadDown": True}, {"dPadLeft": True}, {"dPadRight": True}, {"analogStickX": 131}, {"analogStickY": 190}, {"cStickX": 99}, {"cStickY": 43}, {"A": True, "cStickX": 232}, {"A": True, "B": True, "X": True, "Y": True, "Z": True, "L": True, "R": True, "Start": True, "Reset": True, "triggerL": 255, "triggerR": 255, "dPadUp": True, "dPadDown": True, "dPadLeft": True, "dPadRight": True, "analogStickX": 255, "analogStickY": 255, "cStickX": 255, "cStickY": 255} ]
numberOfCases = len(listOfExampleCasesForMultiButtonFunctions)
inMiscTestsForController = False
inMiscTestsForAllControllers = False
inMiddleOfTest = False
inFirstIteration = True
frameStateLoadedOn = -1

def getDefaultValue(buttonName):
    if buttonName == "A" or buttonName == "B" or buttonName == "X" or buttonName == "Y" or buttonName == "Z" or buttonName == "L" or buttonName == "R" or buttonName == "Start" or buttonName == "Reset" or buttonName == "disc" or buttonName == "getOrigin" or buttonName == "dPadUp" or buttonName == "dPadDown" or buttonName == "dPadLeft" or buttonName == "dPadRight":
        return False
    elif buttonName == "isConnected":
        return True
    elif buttonName == "triggerL" or buttonName == "triggerR":
        return 0
    elif buttonName == "analogStickX" or buttonName == "analogStickY" or buttonName == "cStickX" or buttonName == "cStickY":
        return 128
    else:
        outputFile.write("Button name was unknown - was " + str(buttonName) + "\n")
        outputFile.flush()
        return 0

def testActualButtonsEqualExpectedFunction(actualButtonTable, expectedButtonTable):
    for buttonName, buttonValue in actualButtonTable.items():
        if expectedButtonTable.get(buttonName, None) != None:
            if expectedButtonTable[buttonName] != actualButtonTable[buttonName]:
                return False
        elif buttonValue != getDefaultValue(buttonName):
            return False
    return True

def getListOfButtonsPressed(buttonsDictionary): #will always be either 1 button pressed, 2 buttons pressed, or all buttons pressed to maximum values
    returnString = ""
    if len(buttonsDictionary) == 1:
        returnString = "with just setting "
        for buttonName, buttonValue in buttonsDictionary.items():
            returnString += str(buttonName) + " to " + str(buttonValue)
        return returnString
    elif len(buttonsDictionary) == 2:
        returnString = "with setting "
        inFirstLoop = True
        for buttonName, buttonValue  in buttonsDictionary.items():
            if not inFirstLoop:
                returnString += " and setting " + str(buttonName) + " to " + str(buttonValue)
            else:
                returnString += str(buttonName) + " to " + str(buttonValue)
            inFirstLoop = False
        return returnString
    else:
        return "with setting all buttons to pressed and all analog inputs to their maximum values"

#returns True when test has finished running, and False otherwise
def runSingleFrameInputTest(testDescription, portNumber, inputAlterFunction, buttonTable, checkFunction):
    global inMiddleOfTest
    global testNum
    global baseRecording
    global originalSaveState
    global newMovieName
    global frameStateLoadedOn
    global inFirstIteration
    numberOfFramesBeforeAlteringInput = 5
    if not inMiddleOfTest:
        outputFile.write("Test " + str(testNum) + "\n")
        outputFile.write("\t" + testDescription + "\n")
        testNum = testNum + 1
        emu.playMovie(baseRecording)
        emu.loadState(originalSaveState)
        frameStateLoadedOn = dolphin_statistics.getCurrentFrame()
        inMiddleOfTest = True
        inFirstIteration = True
        return False
    else:
        if inFirstIteration:
            currFrame = dolphin_statistics.getCurrentFrame()
            if frameStateLoadedOn + numberOfFramesBeforeAlteringInput > currFrame:
                return False
            elif frameStateLoadedOn + numberOfFramesBeforeAlteringInput == currFrame:
                inputAlterFunction(portNumber, buttonTable)
            elif frameStateLoadedOn + numberOfFramesBeforeAlteringInput + 4 > currFrame:
                return False
            else:
                emu.saveMovie(newMovieName)
                emu.playMovie(newMovieName)
                emu.loadState(originalSaveState)
                inFirstIteration = False
                return False
        else:
            currFrame = dolphin_statistics.getCurrentFrame()
            if currFrame >= frameStateLoadedOn + numberOfFramesBeforeAlteringInput + 1:
                actualButtonValues = gc_controller.getControllerInputsForPreviousFrame(portNumber)
                if not checkFunction(actualButtonValues, buttonTable):
                    print("FAILED!")
                    resultsTable["FAIL"] = resultsTable["FAIL"] + 1
                    outputFile.write("FAILURE!\n\n")
                    outputFile.flush()
                else:
                    resultsTable["PASS"] = resultsTable["PASS"] + 1
                    outputFile.write("PASS!\n\n")
                    outputFile.flush()
                inMiddleOfTest = False
                inFirstIteration = True
                return True
            else:
                return False
    return False

numberOfFramesImpacted = 0

#Returns true when the test has finished running, and false otherwise
def testProbabilityButton(testDescription, inputAlterFunction, buttonName, probability, expectedValue, testCheckFunction):
    global testNum
    global frameStateLoadedOn
    global newMovieName
    global originalSaveState
    global numberOfFramesImpacted
    global inMiddleOfTest
    global inFirstIteration
    if not inMiddleOfTest:
        outputFile.write("Test " + str(testNum) + ":\n")
        outputFile.write("\t" + testDescription + "\n")
        testNum = testNum + 1
        emu.playMovie(baseRecording)
        emu.loadState(originalSaveState)
        frameStateLoadedOn = dolphin_statistics.getCurrentFrame()
        numberOfFramesImpacted = 0
        inMiddleOfTest = True
        inFirstIteration = True
        return False
    else:
        if inFirstIteration:
            currFrame = dolphin_statistics.getCurrentFrame()
            if frameStateLoadedOn + 1000 > currFrame:
                inputAlterFunction(buttonName, probability)
                return False
            else:
                emu.saveMovie(newMovieName)
                emu.playMovie(newMovieName)
                emu.loadState(originalSaveState)
                inFirstIteration = False
                return False
        else:
            currFrame = dolphin_statistics.getCurrentFrame()
            if frameStateLoadedOn + 1000 > currFrame:
                if testCheckFunction(gc_controller.getControllerInputsForPreviousFrame(1), expectedValue):
                    numberOfFramesImpacted += 1
                return False
            else:
                actualPercent = (numberOfFramesImpacted / 1001) * 100
                if actualPercent - probability < 8 and actualPercent - probability > -8:
                    outputFile.write("\tImpacted frames made up " + str(actualPercent) + "% of all frames, which was in the acceptable range of +- 8% of the selected probability of " + str(probability) + "%\nPASS!\n\n")
                    outputFile.flush()
                    resultsTable["PASS"] = resultsTable["PASS"] + 1
                else:
                    outputFile.write("\tPercentage of impacted frames was " + str(actualPercent) + "%, which was not in the acceptable range of +- 8% of the probability of " + str(probability) + "%\nFAILURE!\n\n")
                    outputFile.flush()
                    resultsTable["FAIL"] = resultsTable["FAIL"] + 1
                inMiddleOfTest = False
                inFirstIteration = True
                return True
    return False

 
totalAnalogValue = 0
#Returns true when the test has finished running, and false otherwise
def testProbabilityAnalog(testDescription, inputAlterFunction, probability, buttonAlterTuple, testCheckFunction):
    global testNum
    global frameStateLoadedOn
    global newMovieName
    global originalSaveState
    global numberOfFramesImpacted
    global inMiddleOfTest
    global inFirstIteration
    global totalAnalogValue
    if not inMiddleOfTest:
        outputFile.write("Test " + str(testNum) + ":\n")
        outputFile.write("\t" + testDescription + "\n")
        testNum = testNum + 1
        emu.playMovie(baseRecording)
        emu.loadState(originalSaveState)
        frameStateLoadedOn = dolphin_statistics.getCurrentFrame()
        numberOfFramesImpacted = 0
        totalAnalogValue = 0
        inMiddleOfTest = True
        inFirstIteration = True
        return False
    else:
        if inFirstIteration:
            currFrame = dolphin_statistics.getCurrentFrame()
            if frameStateLoadedOn + 1000 > currFrame:
                inputAlterFunction(buttonAlterTuple, probability)
                return False
            else:
                emu.saveMovie(newMovieName)
                emu.playMovie(newMovieName)
                emu.loadState(originalSaveState)
                inFirstIteration = False
                return False
        else:
            currFrame = dolphin_statistics.getCurrentFrame()
            if frameStateLoadedOn + 1000 > currFrame:
                if not testCheckFunction(gc_controller.getControllerInputsForPreviousFrame(1), buttonAlterTuple):
                    print("FAIL!")
                    outputFile.write("Error: Input was out of valid bounds of function!")
                    resultsTable["FAIL"] = resultsTable["FAIL"] + 1
                    return True
                totalAnalogValue += gc_controller.getControllerInputsForPreviousFrame(1)[buttonAlterTuple[0]]
            else:
                finalAverageInput = (totalAnalogValue * 1.0) / 1001
                expectedLowestValue = getDefaultValue(buttonAlterTuple[0]) - buttonAlterTuple[1]["minusOffset"]
                expectedHighestValue = getDefaultValue(buttonAlterTuple[0]) + buttonAlterTuple[1]["plusOffset"]
                if buttonAlterTuple[1].get("specificValue", None) != None:
                    expectedLowestValue = buttonAlterTuple[1]["specificValue"] - buttonAlterTuple[1]["minusOffset"]
                    expectedHighestValue = buttonAlterTuple[1]["specificValue"] + buttonAlterTuple[1]["plusOffset"]
                expectedFinalAverage = ((1.0 - (probability * 1.0)/100.0) * getDefaultValue(buttonAlterTuple[0])) + ((probability/100) * ((expectedHighestValue + expectedLowestValue)/2))
                if ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 > 10 or ((finalAverageInput - expectedFinalAverage)/expectedFinalAverage) * 100 < -10:
                    outputFile.write("\tFinal average input of " + str(finalAverageInput) + " for " + buttonAlterTuple[0] + " input, which was outside the acceptable range of +- 10% of the expected final average of " + str(expectedFinalAverage) + "\nFAILURE!\n\n")
                    outputFile.flush()
                    resultsTable["FAIL"] = resultsTable["FAIL"] + 1
                else:
                    outputFile.write("\tFinal average input of " + str(finalAverageInput) + " for " + buttonAlterTuple[0] + " input, which was inside the acceptable range of +- 10% of the expected final average of " + str(expectedFinalAverage) + "\nPASS!\n\n")
                    outputFile.flush()
                    resultsTable["PASS"] = resultsTable["PASS"] + 1
                inMiddleOfTest = False
                inFirstIteration = True
                return True
    return False
            

def testSetInputsUnitTests():
    global controllerNumberForTest
    global indexInListOfAllCases
    global listOfExampleCasesForMultiButtonFunctions
    global numberOfCases
    global testSuiteNumber

    if indexInListOfAllCases >= numberOfCases:
            indexInListOfAllCases = 0
            controllerNumberForTest += 1
            
    if controllerNumberForTest >= 5:
        controllerNumberForTest = 1
        indexInListOfAllCases = 0
        testSuiteNumber += 1
    elif runSingleFrameInputTest("Calling setInputs() on controller " + str(controllerNumberForTest) + " " + getListOfButtonsPressed(listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases]) + "...", controllerNumberForTest, gc_controller.setInputs, listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases], testActualButtonsEqualExpectedFunction):
        indexInListOfAllCases += 1
    
def setInputForAllControllers(ignoreValue, ignoreTable):
	gc_controller.setInputs(1, {"A": True, "B": True})
	gc_controller.setInputs(2, {"X": True, "Y": True})
	gc_controller.setInputs(3, {"Z": True, "B": True})
	gc_controller.setInputs(4, {"A": True, "X": True})

def testSetInputsForAllControllers(ignoreValue, ignoreTable):
	if not testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(1), {"A": True, "B": True}):
		return False
	elif not testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(2), {"X": True, "Y": True}):
		return False
	elif not testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(3), {"Z": True, "B": True}):
		return False
	elif not testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(4), {"A": True, "X": True}):
		return False
	else:
		return True

def setInputTwiceFunction(ignoredValue, ignoredButtonTable):
	gc_controller.setInputs(1, {"A": True, "X": True})
	gc_controller.setInputs(1, {"B": True, "Z": True})

def testSetInputsTwice(ignoredValue, ignoreTable):
	return testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(1), {"B": True, "Z": True})

finishedFirstMiscTest = False
def testSetInputsMiscTests():
    global testSuiteNumber
    global finishedFirstMiscTest
    
    if not finishedFirstMiscTest:
        if runSingleFrameInputTest("Calling setInputs() on all controllers...", 1, setInputForAllControllers, {}, testSetInputsForAllControllers):
            finishedFirstMiscTest = True
    else:
        if runSingleFrameInputTest("Calling setInputs() twice in a row to make sure that the second call overwrites the first one...", 1, setInputTwiceFunction, {}, testSetInputsTwice):
            finishedFirstMiscTest = False
            testSuiteNumber += 1
            
def testAddInputsUnitTests():
    global controllerNumberForTest
    global indexInListOfAllCases
    global listOfExampleCasesForMultiButtonFunctions
    global numberOfCases
    global testSuiteNumber

    if indexInListOfAllCases >= numberOfCases:
            indexInListOfAllCases = 0
            controllerNumberForTest += 1
            
    if controllerNumberForTest >= 5:
        controllerNumberForTest = 1
        indexInListOfAllCases = 0
        testSuiteNumber += 1
    elif runSingleFrameInputTest("Calling addInputs() on controller " + str(controllerNumberForTest) + " " + getListOfButtonsPressed(listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases]) + "...", controllerNumberForTest, gc_controller.addInputs, listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases], testActualButtonsEqualExpectedFunction):
        indexInListOfAllCases += 1
        
def addInputTwiceFunction(ignoredValue, ignoredButtonTable):
	gc_controller.addInputs(1, {"A": True, "X": True})
	gc_controller.addInputs(1, {"B": True, "Z": True})

def testAddInputsTwice(ignoredValue, ignoredTable):
	return testActualButtonsEqualExpectedFunction(gc_controller.getControllerInputsForPreviousFrame(1), {"A": True, "X": True, "B": True, "Z": True})

def addInputForAllControllers(ignoreValue, ignoreTable):
	gc_controller.addInputs(1, {"A": True, "B": True})
	gc_controller.addInputs(2, {"X": True, "Y": True})
	gc_controller.addInputs(3, {"Z": True, "B": True})
	gc_controller.addInputs(4, {"A": True, "X": True})

def testAddInputsMiscTests():
    global testSuiteNumber
    global finishedFirstMiscTest
    
    if not finishedFirstMiscTest:
        if runSingleFrameInputTest("Calling addInputs() on all controllers...", 1, addInputForAllControllers, {}, testSetInputsForAllControllers):
            finishedFirstMiscTest = True
    else:
        if runSingleFrameInputTest("Calling addInputs() twice in a row to make sure that the calls add onto each other...", 1, addInputTwiceFunction, {}, testAddInputsTwice):
            finishedFirstMiscTest = False
            testSuiteNumber += 1
            
allBinaryButtonsExceptA = [ "B", "X", "Y", "Z", "L", "R", "Start", "Reset", "dPadUp", "dPadDown", "dPadLeft", "dPadRight"]
numberOfBinaryButtonsExceptA = len(allBinaryButtonsExceptA)


def addButtonFlipChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        return

addButtonFlipOutputtedA = False
indexInButtonFlipCases = 0

def convertBinaryButtonToDictionary(buttonName, buttonValue):
    myDict = {}
    myDict[buttonName] = buttonValue
    return myDict

def testAddButtonFlipChanceUnitTests():
    global addButtonFlipOutputtedA
    global allBinaryButtonsExceptA
    global numberOfBinaryButtonsExceptA
    global controllerNumberForTest
    global testSuiteNumber
    global indexInButtonFlipCases
    
    if not addButtonFlipOutputtedA:
        if controllerNumberForTest >= 5:
            controllerNumberForTest = 1
            indexInButtonFlipCases = 0
            addButtonFlipOutputtedA = True
        else:
            if runSingleFrameInputTest("Calling addButtonFlipChance() on controller " + str(controllerNumberForTest) + " with 100% probability of flipping button A...", controllerNumberForTest, addButtonFlipChanceFunction, {"A": True}, testActualButtonsEqualExpectedFunction):
                controllerNumberForTest += 1
    else:
        if indexInButtonFlipCases < numberOfBinaryButtonsExceptA:
            if runSingleFrameInputTest("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button " + str(allBinaryButtonsExceptA[indexInButtonFlipCases]) + "...", 1, addButtonFlipChanceFunction, convertBinaryButtonToDictionary(allBinaryButtonsExceptA[indexInButtonFlipCases], True), testActualButtonsEqualExpectedFunction):
                indexInButtonFlipCases += 1
        else:
            testSuiteNumber += 1
            
            

def addReversedButtonFlipChanceFunction(portNumber, buttonTable):
	gc_controller.addInputs(portNumber, {"B": True})
	gc_controller.addButtonFlipChance(portNumber, 100, "B")
    

def addButtonFlipChanceTwiceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        return

def addButtonFlipChanceThriceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        gc_controller.addButtonFlipChance(portNumber, 100, buttonName)
        return

def addZeroChanceButtonFlipChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonFlipChance(portNumber, 0, buttonName)
        return

addButtonFlipChanceMiscTestNumber = 1
def testAddButtonFlipChanceMiscTests():
    global addButtonFlipChanceMiscTestNumber
    global testSuiteNumber
    if addButtonFlipChanceMiscTestNumber == 1:
        if runSingleFrameInputTest("Calling addButtonFlipChance() on controller 1 with 100% probability of flipping button B (but after setting button B to pressed)...", 1, addReversedButtonFlipChanceFunction, {"B": False}, testActualButtonsEqualExpectedFunction):
            addButtonFlipChanceMiscTestNumber += 1
    elif addButtonFlipChanceMiscTestNumber == 2:
        if runSingleFrameInputTest("Calling addButtonFlipChance() on controller 1 twice in a row with 100% probability of flipping button X...", 1, addButtonFlipChanceTwiceFunction, {"X": False}, testActualButtonsEqualExpectedFunction):
            addButtonFlipChanceMiscTestNumber += 1
    elif addButtonFlipChanceMiscTestNumber == 3:
        if runSingleFrameInputTest("Calling addButtonFlipChance() on controller 1 three times in a row with 100% probability of flipping button Y...", 1, addButtonFlipChanceThriceFunction, {"Y": True}, testActualButtonsEqualExpectedFunction):
            addButtonFlipChanceMiscTestNumber += 1
    elif addButtonFlipChanceMiscTestNumber == 4:
        if runSingleFrameInputTest("Calling addButtonFlipChance() on controller 1 with 0% probability of flipping button A to pressed...", 1, addZeroChanceButtonFlipChanceFunction, {"A": False}, testActualButtonsEqualExpectedFunction):
            addButtonFlipChanceMiscTestNumber += 1
    else:
        testSuiteNumber += 1

def testProbabilityAddButtonFlipInputAlter(buttonName, probability):
    gc_controller.addButtonFlipChance(1, probability, buttonName)
    
def testAddButtonFlipMultiFrameTest():
    global testSuiteNumber
    if testProbabilityButton("Running through 1000 frames of calling addButtonFlipChance() on controller 1 with 13% chance of flipping button B...", testProbabilityAddButtonFlipInputAlter, "B", 13, {"B": True}, testActualButtonsEqualExpectedFunction):
        testSuiteNumber += 1
        
        
def addButtonPressChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonPressChance(portNumber, 100, buttonName)
        return
        
addButtonPressOutputtedA = False
indexInButtonPressCases = 0
def testAddButtonPressChanceUnitTests():
    global addButtonPressOutputtedA
    global allBinaryButtonsExceptA
    global numberOfBinaryButtonsExceptA
    global controllerNumberForTest
    global testSuiteNumber
    global indexInButtonPressCases
    
    if not addButtonPressOutputtedA:
        if controllerNumberForTest >= 5:
            controllerNumberForTest = 1
            indexInButtonPressCases = 0
            addButtonPressOutputtedA = True
        else:
            if runSingleFrameInputTest("Calling addButtonPressChance() on controller " + str(controllerNumberForTest) + " with 100% probability of pressing button A...", controllerNumberForTest, addButtonPressChanceFunction, {"A": True}, testActualButtonsEqualExpectedFunction):
                controllerNumberForTest += 1
    else:
        if indexInButtonPressCases < numberOfBinaryButtonsExceptA:
            if runSingleFrameInputTest("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button " + str(allBinaryButtonsExceptA[indexInButtonPressCases]) + "...", 1, addButtonPressChanceFunction, convertBinaryButtonToDictionary(allBinaryButtonsExceptA[indexInButtonPressCases], True), testActualButtonsEqualExpectedFunction):
                indexInButtonPressCases += 1
        else:
            testSuiteNumber += 1
            
            
def addReversedButtonPressChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.setInputs(portNumber, buttonTable)
        gc_controller.addButtonPressChance(portNumber, 100, buttonName)
        return

def addZeroChanceButtonPressChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonPressChance(portNumber, 0, buttonName)
        return
    

addButtonPressMiscTestsIndex = 1
def testAddButtonPressChanceMiscTests():
    global addButtonPressMiscTestsIndex
    global testSuiteNumber
    if addButtonPressMiscTestsIndex == 1:
        if runSingleFrameInputTest("Calling addButtonPressChance() on controller 1 with 100% probability of pressing button B (but after setting button B to pressed)...", 1, addReversedButtonPressChanceFunction, {"B": True}, testActualButtonsEqualExpectedFunction):
            addButtonPressMiscTestsIndex += 1
    elif addButtonPressMiscTestsIndex == 2:
        if runSingleFrameInputTest("Calling addButtonPressChance() on controller 1 with 0% probability of setting button A to pressed...", 1, addZeroChanceButtonPressChanceFunction, {"A": False}, testActualButtonsEqualExpectedFunction):
            addButtonFlipChanceMiscTestNumber = 1
            testSuiteNumber += 1

def testProbabilityAddButtonPressInputAlter(buttonName, probability):
    gc_controller.addButtonPressChance(1, probability, buttonName)
    
def testAddButtonPressMultiFrameTest():
    global testSuiteNumber
    if testProbabilityButton("Running through 1000 frames of calling addButtonPressChance() on controller 1 with 34% chance of pressing button X...", testProbabilityAddButtonPressInputAlter, "X", 34, {"X": True}, testActualButtonsEqualExpectedFunction):
        testSuiteNumber += 1


def addButtonReleaseChanceWithSetFunction(portNumber, buttonTable):
    alterDict = {}
    for buttonName in buttonTable:
        alterDict[buttonName] = True
        gc_controller.setInputs(portNumber, alterDict)
        gc_controller.addButtonReleaseChance(portNumber, 100, buttonName)
        return

addButtonReleaseOutputtedA = False
indexInButtonReleaseCases = 0
def testAddButtonReleaseChanceUnitTests():
    global addButtonReleaseOutputtedA
    global allBinaryButtonsExceptA
    global numberOfBinaryButtonsExceptA
    global controllerNumberForTest
    global testSuiteNumber
    global indexInButtonReleaseCases
    
    if not addButtonReleaseOutputtedA:
        if controllerNumberForTest >= 5:
            controllerNumberForTest = 1
            indexInButtonReleaseCases = 0
            addButtonReleaseOutputtedA = True
        else:
            if runSingleFrameInputTest("Calling addButtonReleaseChance() on controller " + str(controllerNumberForTest) + " after setting A to pressed with 100% probability of releasing button A...", controllerNumberForTest, addButtonReleaseChanceWithSetFunction, {"A": False}, testActualButtonsEqualExpectedFunction):
                controllerNumberForTest += 1
    else:
        if indexInButtonReleaseCases < numberOfBinaryButtonsExceptA:
            if runSingleFrameInputTest("Calling addButtonReleaseChance() on controller 1 after setting " + str(allBinaryButtonsExceptA[indexInButtonReleaseCases]) + " to pressed with 100% probability of releasing button " + str(allBinaryButtonsExceptA[indexInButtonReleaseCases]) + "...", 1, addButtonReleaseChanceWithSetFunction, convertBinaryButtonToDictionary(allBinaryButtonsExceptA[indexInButtonReleaseCases], False), testActualButtonsEqualExpectedFunction):
                indexInButtonReleaseCases += 1
        else:
            testSuiteNumber += 1

def addButtonReleaseChanceFunction(portNumber, buttonTable):
    for buttonName in buttonTable:
        gc_controller.addButtonReleaseChance(portNumber, 100, buttonName)
        return

def addButtonReleaseZeroChanceFunction(portNumber, buttonTable):
    alterDict = {}
    for buttonName in buttonTable:
        alterDict[buttonName] = True
        gc_controller.setInputs(portNumber, alterDict)
        gc_controller.addButtonReleaseChance(portNumber, 0, buttonName)
        return

addButtonReleaseMiscTestsIndex = 1
def testAddButtonReleaseMiscTests():
    global addButtonReleaseMiscTestsIndex
    global testSuiteNumber
    if addButtonReleaseMiscTestsIndex == 1:
        if runSingleFrameInputTest("Calling addButtonReleaseChance() on controller 1 with 100% probability of releasing button B...", 1, addButtonReleaseChanceFunction, {"B": False}, testActualButtonsEqualExpectedFunction):
            addButtonReleaseMiscTestsIndex += 1
    elif addButtonReleaseMiscTestsIndex == 2:
        if runSingleFrameInputTest("Calling addButtonRelaseChance() on controller 1 after setting A to pressed with 0% probability of releasing button A...", 1, addButtonReleaseZeroChanceFunction, {"A": True}, testActualButtonsEqualExpectedFunction):
            addButtonReleaseMiscTestsIndex = 1
            testSuiteNumber += 1
    else:
        testSuiteNumber += 1
        
def testProbabilityAddButtonReleaseInputAlter(buttonName, probability):
    alterDict = {}
    alterDict[buttonName] = True
    gc_controller.setInputs(1, alterDict)
    gc_controller.addButtonReleaseChance(1, probability, buttonName)
    
def testAddButtonReleaseMultiFrameTest():
    global testSuiteNumber
    if testProbabilityButton("Running through 1000 frames of calling addButtonReleaseChance() on controller 1 after setting B to pressed with 68% chance of releasing button B...", testProbabilityAddButtonReleaseInputAlter, "B", 68, {"B": False}, testActualButtonsEqualExpectedFunction): 
        testSuiteNumber += 1
    
def testActualButtonsInRangeFunction(actualButtonTable, tupleOfOffsets):
	for buttonName, buttonValue in actualButtonTable.items():
		if buttonValue != getDefaultValue(buttonName):
			if tupleOfOffsets[0] == buttonName:
				baseValue = getDefaultValue(buttonName)
				if tupleOfOffsets[1].get("specificValue", None) != None:
					baseValue = tupleOfOffsets[1]["specificValue"]
				
				if buttonValue < baseValue - tupleOfOffsets[1]["minusOffset"] or buttonValue > baseValue + tupleOfOffsets[1]["plusOffset"]:
					return False
			else:
				return False
	return True
  
def addOrSubtractFromCurrentAnalogValueFunction(portNumber, tupleOfOffsets):
	minusOffset = tupleOfOffsets[1]["minusOffset"]
	plusOffset = tupleOfOffsets[1]["plusOffset"]
	buttonName = tupleOfOffsets[0]
    
	if minusOffset == plusOffset:
		gc_controller.addOrSubtractFromCurrentAnalogValueChance(portNumber, 100, buttonName, plusOffset)
	else:
		gc_controller.addOrSubtractFromCurrentAnalogValueChance(portNumber, 100, buttonName, minusOffset, plusOffset)



addOrSubtractFromCurrentAnalogValueOutputtedCStickX = False
indexInAddorSubtractFromCurrentAnanalogValueCases = 0
allAnalogCasesExcepttCStickX_current = [ ("cStickY", {"minusOffset": 35, "plusOffset": 35}), ("analogStickX", {"minusOffset": 0, "plusOffset": 41}), ("analogStickY", {"minusOffset": 62, "plusOffset": 0}), ("triggerL", {"minusOffset": 13, "plusOffset": 20}), ("triggerR", {"minusOffset": 35, "plusOffset": 100})]
numberOfAnalogCasesExceptCStickX_current = len(allAnalogCasesExcepttCStickX_current)
cStickXCase_current = ("cStickX", {"minusOffset": 17, "plusOffset": 41}) 

def testAddOrSubtractFromCurrentAnalogValueChanceUnitTests():
    global addOrSubtractFromCurrentAnalogValueOutputtedCStickX
    global allAnalogCasesExcepttCStickX_current
    global numberOfAnalogCasesExceptCStickX_current
    global controllerNumberForTest
    global testSuiteNumber
    global indexInAddorSubtractFromCurrentAnanalogValueCases
    global cStickXCase_current
    
    if not addOrSubtractFromCurrentAnalogValueOutputtedCStickX:
        if controllerNumberForTest >= 5:
            controllerNumberForTest = 1
            indexInAddorSubtractFromCurrentAnanalogValueCases = 0
            addOrSubtractFromCurrentAnalogValueOutputtedCStickX = True
        else:
            if runSingleFrameInputTest("Calling addOrSubtractFromCurrentAnalogValueChance() on controller " + str(controllerNumberForTest) + " after setting a 100% probability of altering cStickX by " + str(-1 * cStickXCase_current[1]["minusOffset"]) + " to +" + str(cStickXCase_current[1]["plusOffset"]) + "...", controllerNumberForTest, addOrSubtractFromCurrentAnalogValueFunction, cStickXCase_current, testActualButtonsInRangeFunction):
                controllerNumberForTest += 1
    else:
        if indexInAddorSubtractFromCurrentAnanalogValueCases < numberOfAnalogCasesExceptCStickX_current:
            currentTuple = allAnalogCasesExcepttCStickX_current[indexInAddorSubtractFromCurrentAnanalogValueCases]
            if runSingleFrameInputTest("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 100% probability of altering " + currentTuple[0] + " by " + str(-1 * currentTuple[1]["minusOffset"]) + " to +" + str(currentTuple[1]["plusOffset"]) + "...", 1, addOrSubtractFromCurrentAnalogValueFunction, currentTuple, testActualButtonsInRangeFunction):
                indexInAddorSubtractFromCurrentAnanalogValueCases += 1
        else:
            testSuiteNumber += 1

def addOrSubtractFromCurrentAnalogValueZeroChanceFunction(portNumber, buttonTable):
	gc_controller.addOrSubtractFromCurrentAnalogValueChance(1, 0, "cStickY", 20, 50)


def testAddOrSubtractFromCurrentAnalogValueChanceMiscTests():
    global testSuiteNumber
    if runSingleFrameInputTest("Calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 after setting a 0% probability of altering cStickY by -20 to +50...", 1, addOrSubtractFromCurrentAnalogValueZeroChanceFunction,  ("cStickY", {"minusOffset": 20, "plusOffset": 50}), testActualButtonsInRangeFunction):
        testSuiteNumber += 1

def testProbabilityAddOrSubtractFromCurrentAnalogValueInputAlter(buttonAlterTuple, probability):
    buttonName = buttonAlterTuple[0]
    minusOffset = buttonAlterTuple[1]["minusOffset"]
    plusOffset = buttonAlterTuple[1]["plusOffset"]
    if minusOffset == plusOffset:
        gc_controller.addOrSubtractFromCurrentAnalogValueChance(1, probability, buttonName, minusOffset)
    else:
        gc_controller.addOrSubtractFromCurrentAnalogValueChance(1, probability, buttonName, minusOffset, plusOffset)
    
def testAddOrSubtractFromCurrentAnalogValueMultiFrameTest():
    global testSuiteNumber
    if testProbabilityAnalog("Running through 1000 frames of calling addOrSubtractFromCurrentAnalogValueChance() on controller 1 with 40% chance of altering cStickX by between -31 to +85...", testProbabilityAddOrSubtractFromCurrentAnalogValueInputAlter, 40, ("cStickX", {"minusOffset": 31, "plusOffset": 85}), testActualButtonsInRangeFunction):
        testSuiteNumber += 1


def addOrSubtractFromSpecificAnalogValueFunction(portNumber, tupleOfOffsets):
    minusOffset = tupleOfOffsets[1]["minusOffset"]
    plusOffset = tupleOfOffsets[1]["plusOffset"]
    specificValue = tupleOfOffsets[1]["specificValue"]

    buttonName = tupleOfOffsets[0]
    
    if minusOffset == plusOffset:
        gc_controller.addOrSubtractFromSpecificAnalogValueChance(portNumber, 100, buttonName, specificValue, plusOffset)
    else:
        gc_controller.addOrSubtractFromSpecificAnalogValueChance(portNumber, 100, buttonName, specificValue, minusOffset, plusOffset)


addOrSubtractFromSpecificAnalogValueOutputtedCStickX = False
allAnalogCasesExceptCStickX_specific = [ ("cStickY", {"specificValue": 70, "minusOffset": 40, "plusOffset": 40}), ("analogStickX", {"specificValue": 4, "minusOffset": 20, "plusOffset": 200}), ("analogStickY", {"specificValue": 200, "minusOffset": 80, "plusOffset": 100}), ("triggerL", {"specificValue": 31, "minusOffset": 8, "plusOffset": 6}), ("triggerR", {"specificValue": 100, "minusOffset": 150, "plusOffset": 180})]
numberOfAnalogCasesExceptCStickX_specific = len(allAnalogCasesExceptCStickX_specific)
indexInAddorSubtractFromSpecificAnanalogValueCases = 0
cStickXCase_specific = ("cStickX", {"specificValue": 200, "minusOffset": 18, "plusOffset": 34})

def testAddOrSubtractFromSpecificAnalogValueChanceUnitTests():
    global addOrSubtractFromSpecificAnalogValueOutputtedCStickX
    global allAnalogCasesExceptCStickX_specific
    global numberOfAnalogCasesExceptCStickX_specific
    global controllerNumberForTest
    global testSuiteNumber
    global indexInAddorSubtractFromSpecificAnanalogValueCases
    global cStickXCase_specific
    
    if not addOrSubtractFromSpecificAnalogValueOutputtedCStickX:
        if controllerNumberForTest >= 5:
            controllerNumberForTest = 1
            indexInAddorSubtractFromSpecificAnanalogValueCases = 0
            addOrSubtractFromSpecificAnalogValueOutputtedCStickX = True
        else:
            if runSingleFrameInputTest("Calling addOrSubtractFromSpecificAnalogValueChance() on controller " + str(controllerNumberForTest) + " after setting a 100% probability of setting cStickX to " + str(cStickXCase_specific[1]["specificValue"]) + " with an offset between " + str(-1 * cStickXCase_specific[1]["minusOffset"]) + " to +" + str(cStickXCase_specific[1]["plusOffset"]) + "...", controllerNumberForTest, addOrSubtractFromSpecificAnalogValueFunction, cStickXCase_specific, testActualButtonsInRangeFunction):
                controllerNumberForTest += 1
    else:
        if indexInAddorSubtractFromSpecificAnanalogValueCases < numberOfAnalogCasesExceptCStickX_specific:
            currentTuple = allAnalogCasesExceptCStickX_specific[indexInAddorSubtractFromSpecificAnanalogValueCases]
            if runSingleFrameInputTest("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 100% probability of setting " + currentTuple[0] + " to " + str(currentTuple[1]["specificValue"]) + " with an offset between " + str(-1 * currentTuple[1]["minusOffset"]) + " to +" + str(currentTuple[1]["plusOffset"]) + "...", 1, addOrSubtractFromSpecificAnalogValueFunction, currentTuple, testActualButtonsInRangeFunction):
                indexInAddorSubtractFromSpecificAnanalogValueCases += 1
        else:
            testSuiteNumber += 1
   
def addOrSubtractFromSpecifcAnalogValueZeroChanceFunction(portNumber, tupleOfOffsets):
    gc_controller.addOrSubtractFromSpecificAnalogValueChance(portNumber, 0, "analogStickX", 170, 30, 45)
   
def testAddOrSubtractFromSpecificAnalogValueChanceMiscTests():
    global testSuiteNumber
    if runSingleFrameInputTest("Calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 after setting a 0% probability of setting analogStickX to a value between 170 - 30 and 170 + 45...", 1, addOrSubtractFromSpecifcAnalogValueZeroChanceFunction, ("analogStickX", {"specificValue": 170, "minusOffset": 30, "plusOffset": 45}), testActualButtonsInRangeFunction):
        testSuiteNumber += 1
        
def testProbabilityAddOrSubtractFromSpecificAnalogValueInputAlter(buttonAlterTuple, probability):
    buttonName = buttonAlterTuple[0]
    minusOffset = buttonAlterTuple[1]["minusOffset"]
    plusOffset = buttonAlterTuple[1]["plusOffset"]
    specificValue = buttonAlterTuple[1]["specificValue"]
    if minusOffset == plusOffset:
        gc_controller.addOrSubtractFromSpecificAnalogValueChance(1, probability, buttonName, specificValue, minusOffset)
    else:
        gc_controller.addOrSubtractFromSpecificAnalogValueChance(1, probability, buttonName, specificValue, minusOffset, plusOffset)

def testAddOrSubtractFromSpecificAnalogValueMultiFrameTest():
    global testSuiteNumber
    if testProbabilityAnalog("Running through 1000 frames of calling addOrSubtractFromSpecificAnalogValueChance() on controller 1 with a 65% chance of setting cStickX to a random value between 50 - 30 and 50 + 10...", testProbabilityAddOrSubtractFromSpecificAnalogValueInputAlter, 65, ("cStickX", {"specificValue": 50, "minusOffset": 30, "plusOffset": 10}), testActualButtonsInRangeFunction):
        testSuiteNumber += 1


def addButtonComboNormal(portNumber, buttonTable):
    gc_controller.addButtonComboChance(portNumber, 100, buttonTable, True)

def testAddButtonComboChanceUnitTests():
    global controllerNumberForTest
    global indexInListOfAllCases
    global listOfExampleCasesForMultiButtonFunctions
    global numberOfCases
    global testSuiteNumber

    if indexInListOfAllCases >= numberOfCases:
            indexInListOfAllCases = 0
            controllerNumberForTest += 1
            
    if controllerNumberForTest >= 5:
        controllerNumberForTest = 1
        indexInListOfAllCases = 0
        testSuiteNumber += 1
    elif runSingleFrameInputTest("Calling addButtonComboChance() on controller " + str(controllerNumberForTest) + " with 100% chance of " + getListOfButtonsPressed(listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases]) + "...", controllerNumberForTest, addButtonComboNormal, listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases], testActualButtonsEqualExpectedFunction):
        indexInListOfAllCases += 1

def addZeroChanceButtonComboFunction(portNumber, buttonTable):
    gc_controller.addButtonComboChance(portNumber, 0, {"A": True}, True)

def addButtonComboChanceForAllControllers(portNumber, buttonTable):
	gc_controller.addButtonComboChance(1, 100, {"A": True, "B": True}, True)
	gc_controller.addButtonComboChance(2, 100, {"X": True, "Y": True}, True)
	gc_controller.addButtonComboChance(3, 100, {"Z": True, "B": True}, True)
	gc_controller.addButtonComboChance(4, 100, {"A": True, "X": True}, True)

def addOverwritingButtonComboChancesFunction(portNumber, buttonTable):
    gc_controller.addButtonComboChance(portNumber, 100, {"B": True, "X": True},  True)
    gc_controller.addButtonComboChance(portNumber, 100, {"A": True, "Z": True}, True)
      
def addNonOverwritingButtonComboChancesFunction(portNumber, buttonTable):
	gc_controller.addButtonComboChance(portNumber, 100, {"B": True, "X": True}, False)
	gc_controller.addButtonComboChance(portNumber, 100, {"A": True, "Z": True}, False)
    
indexInAddButtonComboMiscTests = 1
def testAddButtonComboMiscTests():
    global indexInAddButtonComboMiscTests
    global testSuiteNumber
    if indexInAddButtonComboMiscTests == 1:
        if runSingleFrameInputTest("Calling addButtonComboChance() on controller 1 with 0% probability of setting button A to pressed...", 1, addZeroChanceButtonComboFunction, {"A": False}, testActualButtonsEqualExpectedFunction):
            indexInAddButtonComboMiscTests += 1
    elif indexInAddButtonComboMiscTests == 2:
        if runSingleFrameInputTest("Calling addButtonComboChance() on all controllers...", 1, addButtonComboChanceForAllControllers, {}, testSetInputsForAllControllers):
            indexInAddButtonComboMiscTests += 1
    elif indexInAddButtonComboMiscTests == 3:
        if runSingleFrameInputTest("Calling addButtonComboChance() with overwrite-non-specified-values set to true. First call has 100% probability of setting B and X to pressed, and 2nd call has 100% probability of setting A and Z to pressed. Final input should be just A and Z are pressed...", 1, addOverwritingButtonComboChancesFunction, {"A": True, "Z": True}, testActualButtonsEqualExpectedFunction):
            indexInAddButtonComboMiscTests += 1
    elif indexInAddButtonComboMiscTests == 4:
        if runSingleFrameInputTest("Calling addButtonComboChance() with overwrite-non-specified-values set to false. First call has 100% probability of setting B and X to pressed, and 2nd call has 100% probability of setting A and Z to pressed. Final input should be A, Z, X and B are pressed...", 1, addNonOverwritingButtonComboChancesFunction, {"A": True, "B": True, "X": True, "Z": True}, testActualButtonsEqualExpectedFunction):
            indexInAddButtonComboMiscTests = 1
            testSuiteNumber += 1

def testProbabilityAddButtonComboInputAlter(buttonName, probability):
    inputsDict = {}
    inputsDict[buttonName] = True
    gc_controller.addButtonComboChance(1, probability, inputsDict, True)
    
def testAddButtonComboMultiFrameTest():
    global testSuiteNumber
    if testProbabilityButton("Running through 1000 frames of calling addButtonComboChance() on controller 1 with 80% chance of pressing button B...", testProbabilityAddButtonComboInputAlter, "B", 80, {"B": True}, testActualButtonsEqualExpectedFunction):
        testSuiteNumber += 1

allButtonsSetDict = {"A": True, "B": True, "X": True, "Y": True, "Z": True, "L": True, "R": True, "dPadUp": True, "dPadDown":True, "dPadLeft":True, "dPadRight":True, "Start":True, "Reset":True, "analogStickX": 255, "analogStickY":255, "cStickX":255, "cStickY":255, "triggerL":255, "triggerR":255}
controllerClearTestNum = 1  

def addControllerClearChanceFunction(portNumber, buttonTable):
    global allButtonsSetDict
    gc_controller.setInputs(portNumber, allButtonsSetDict)
    gc_controller.addControllerClearChance(portNumber, 100)

def addControllerClearZeroChanceFunction(portNumber, buttonTable):
    global allButtonsSetDict
    gc_controller.setInputs(portNumber, allButtonsSetDict)
    gc_controller.addControllerClearChance(portNumber, 0)

def testAddControllerClearChanceUnitTests():
    global controllerClearTestNum
    global controllerNumberForTest
    global testSuiteNumber
    global allButtonsSetDict
    if controllerNumberForTest >= 5:
        testSuiteNumber += 1
        return
    if controllerClearTestNum == 1:
        if runSingleFrameInputTest("Calling addControllerClearChance() on controller " + str(controllerNumberForTest) + " after setting all inputs to max values with 100% chance of clearing inputs...", controllerNumberForTest, addControllerClearChanceFunction, {}, testActualButtonsEqualExpectedFunction):
            controllerClearTestNum += 1
    elif controllerClearTestNum == 2:
        if runSingleFrameInputTest("Calling addControllerClearChance() on controller " + str(controllerNumberForTest) + " after setting all inputs to max values with 0% chance of clearing inputs...", controllerNumberForTest, addControllerClearZeroChanceFunction, allButtonsSetDict, testActualButtonsEqualExpectedFunction):
            controllerClearTestNum = 1
            controllerNumberForTest += 1
    else:
        testSuiteNumber += 1

def testProbabilityAddControllerClearInputAlter(buttonName, probability):
    global allButtonsSetDict
    gc_controller.setInputs(1, allButtonsSetDict)
    gc_controller.addControllerClearChance(1, probability)

def testAddControllerClearMultiFrameTest():
    global testSuiteNumber
    if testProbabilityButton("Running through 1000 frames of calling addControllerClearChance() on controller 1 with 75% chance of clearing the controller inputs to their default values...", testProbabilityAddControllerClearInputAlter, "", 75, {}, testActualButtonsEqualExpectedFunction):
        testSuiteNumber += 1

def setAndThenAddInputs(portNumber, buttonTable):
	gc_controller.setInputs(1, {"A": True, "B": True, "L": True, "dPadUp": True})
	gc_controller.setInputs(1, {"X": True, "B": False})
	gc_controller.addInputs(1, {"Z": True, "Y": True})
	gc_controller.addInputs(1, {"Y": True, "dPadUp": False})

def setAndThenAddAndThenProbability(portNumber, buttonTable):
	setAndThenAddInputs(portNumber, buttonTable)
	gc_controller.addButtonFlipChance(1, 100, "dPadUp")
	gc_controller.addButtonPressChance(1, 100, "Y")
   
integrationTestNumber = 1
def integrationTests():
    global integrationTestNumber
    global testSuiteNumber
    if integrationTestNumber == 1:
        if runSingleFrameInputTest("Calling setInputs() and addInputs() multiple times to perform integration testing. Final results should be only X, Y and Z pressed for controller 1...", 1, setAndThenAddInputs, {"X": True, "Y": True, "Z": True}, testActualButtonsEqualExpectedFunction):
            integrationTestNumber += 1
    else:
        if runSingleFrameInputTest("Calling setInputs(), addInputs() and multiple probability functions in order to perform integration testing. Final results should be only X, Y, Z, and dPadUp pressed for controller 1...", 1, setAndThenAddAndThenProbability, {"X": True, "Y": True, "Z": True, "dPadUp": True}, testActualButtonsEqualExpectedFunction):
            testSuiteNumber += 1
    

funcRef = OnFrameStart.register(MainFunction)