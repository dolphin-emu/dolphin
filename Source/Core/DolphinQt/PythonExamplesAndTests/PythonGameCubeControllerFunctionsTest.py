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

def MainFunction():
    global testSuiteNumber
    global funcRef
    
    if testSuiteNumber == 1:
        testSetInputsUnitTests()
    elif testSuiteNumber == 2:
        testAddInputsUnitTests()
    elif testSuiteNumber == 3:
        testAddButtonFlipChanceUnitTests()
    elif testSuiteNumber == 4:
        testAddButtonPressChanceUnitTests()
    elif testSuiteNumber == 5:
        testAddButtonReleaseChanceUnitTests()
    elif testSuiteNumber == 6:
        testAddOrSubtractactFromCurrentAnalogValueChanceUnitTests()
    elif testSuiteNumber == 7:
        testAddOrSubtractFromSpecificAnalogValueChanceUnitTests()
    elif testSuiteNumber == 8:
        testAddButtonComboChanceUnitTests()
    elif testSuiteNumber == 9:
        testAddControllerClearChanceUnitTests()
    elif testSuiteNumber == 10:
        integrationTests()
    elif testSuiteNumber == 11:
        printSummary()
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
    if buttonName == "A" or buttonName == "B" or buttonName == "X" or buttonName == "Y" or buttonName == "Z" or buttonName == "L" or buttonName == "R" or buttonName == "Start" or buttonName == "Reset" or buttonName == "dPadUp" or buttonName == "dPadDown" or buttonName == "dPadLeft" or buttonName == "dPadRight":
        return False
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

def testSetInputsUnitTests():
    global controllerNumberForTest
    global indexInListOfAllCases
    global listOfExampleCasesForMultiButtonFunctions
    global numberOfCases
    global inMiscTestsForAllControllers
    global inMiscTestsForController
    global testSuiteNumber

    if not inMiscTestsForAllControllers and controllerNumberForTest >= 5:
        inMiscTestsForAllControllers = True
        controllerNumberForTest = 1
    if not inMiscTestsForAllControllers:
        if indexInListOfAllCases >= numberOfCases:
            indexInListOfAllCases = 0
            controllerNumberForTest += 1
        if controllerNumberForTest >= 5:
            inSetInputsMiscTests = True
            controllerNumberForTest = 1
        else:
            if runSingleFrameInputTest("Calling setInput() on controller " + str(controllerNumberForTest) + " " + getListOfButtonsPressed(listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases]) + "...", controllerNumberForTest, 5, gc_controller.setInputs, listOfExampleCasesForMultiButtonFunctions[indexInListOfAllCases], testActualButtonsEqualExpectedFunction):
                indexInListOfAllCases += 1
    else:
        testSuiteNumber += 1

#returns True when test has finished running, and False  otherwise
def runSingleFrameInputTest(testDescription, portNumber, numberOfFramesBeforeAlteringInput, inputAlterFunction, buttonTable, checkFunction):
    global inMiddleOfTest
    global testNum
    global baseRecording
    global originalSaveState
    global newMovieName
    global frameStateLoadedOn
    global inFirstIteration
    if not inMiddleOfTest:
        outputFile.write("Test " + str(testNum) + "\n")
        outputFile.write("\t" + testDescription + "\n")
        testNum = testNum + 1
        emu.playMovie(baseRecording)
        emu.loadState(originalSaveState)
        frameStateLoadedOn = dolphin_statistics.getCurrentFrame()
        inMiddleOfTest = True
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
                    print("SUCCESS!")
                    resultsTable["PASS"] = resultsTable["PASS"] + 1
                    outputFile.write("PASS!\n\n")
                    outputFile.flush()
                inMiddleOfTest = False
                inFirstIteration = True
                return True
            else:
                return False
    return False
    



funcRef = OnFrameStart.register(MainFunction)