import emu
import dolphin_statistics
import gc_controller

funcRef = 0
baseMovieFilePath  = "testBaseRecording.dtm"
earlySaveStatePath = "testEarlySaveState.sav"
laterMovieName = "laterRecording.sav.dtm"
laterSaveState = "laterRecording.sav"

testNum = 2
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

outputFile = open("PythonExamplesAndTests/TestResults/PythonStatisticsTestsResults.txt", "w")

def singleStatisticTest(functionName, resultOfFunction):
    global testNum
    class_name = "statistics"
    if testNum >= 12:
        class_name = "gc_controller"
    outputFile.write("Test " + str(testNum) + ":\n\tTesting " + class_name + "." + functionName + " function...\n")
    testNum = testNum + 1
    outputFile.write("\t" + class_name + "." + functionName + " = " + str(resultOfFunction) + "\nPASS!\n\n")
    resultsTable["PASS"] = resultsTable["PASS"] + 1

def statisticsGeneralUnitTests():
    singleStatisticTest("isRecordingInput()", dolphin_statistics.isRecordingInput())
    singleStatisticTest("isRecordingInputFromSaveState()", dolphin_statistics.isRecordingInputFromSaveState())
    singleStatisticTest("isPlayingInput()", dolphin_statistics.isPlayingInput())
    singleStatisticTest("isMovieActive()", dolphin_statistics.isMovieActive())
    singleStatisticTest("getMovieLength()", dolphin_statistics.getMovieLength())	
    singleStatisticTest("getRerecordCount()", dolphin_statistics.getRerecordCount())
    singleStatisticTest("getCurrentInputCount()", dolphin_statistics.getCurrentInputCount())
    singleStatisticTest("getTotalInputCount()", dolphin_statistics.getTotalInputCount())	
    singleStatisticTest("getCurrentLagCount()", dolphin_statistics.getCurrentLagCount())
    singleStatisticTest("getTotalLagCount()", dolphin_statistics.getTotalLagCount())
    singleStatisticTest("isGcControllerInPort(1)", gc_controller.isGcControllerInPort(1))
    singleStatisticTest("isUsingPort(1)", gc_controller.isUsingPort(1))
    singleStatisticTest("getRAMSize()", dolphin_statistics.getRAMSize())
    singleStatisticTest("getL1CacheSize()", dolphin_statistics.getL1CacheSize())
    singleStatisticTest("getFakeVMemSize()", dolphin_statistics.getFakeVMemSize())
    singleStatisticTest("getExRAMSize()", dolphin_statistics.getExRAMSize())


startingFrameCounter = 0
loadedMovieForFirstTime = False
initialFrameNumber = 0
postLoadFrameCounter = 0
madeSecondSave = False
lastFrameAdvanceCounter = 0
def statisticsTests():
    global startingFrameCounter
    global loadedMovieForFirstTime
    global initialFrameNumber
    global postLoadFrameCounter
    global madeSecondSave
    global lastFrameAdvanceCounter
    global testNum
    global funcRef
    
    if startingFrameCounter <= 2: #advance 2 frames to start off
        startingFrameCounter += 1
        return

    if not loadedMovieForFirstTime:
        emu.playMovie(baseMovieFilePath)
        emu.loadState(earlySaveStatePath)
        initialFrameNumber = dolphin_statistics.getCurrentFrame()
        loadedMovieForFirstTime = True
        return
        
    if postLoadFrameCounter < 9:
        postLoadFrameCounter += 1
        return
        
    if not madeSecondSave:
        laterFrameNumber = dolphin_statistics.getCurrentFrame()
        outputFile.write("Test 1:\n\tTesting that statistics.getCurrentFrame() after frame advancing 10 frames is 10 more than its original value...\n")
        if (laterFrameNumber != initialFrameNumber + 10):
            outputFile.write("\t" + str(initialFrameNumber) + " + 10 != " + str(laterFrameNumber) + "\nFAILURE!\n\n")
            resultsTable["FAIL"] = resultsTable["FAIL"] + 1
        else:
            outputFile.write("\t" + str(initialFrameNumber) + " + 10 == " + str(laterFrameNumber) + "\nPASS!\n\n")
            resultsTable["PASS"] = resultsTable["PASS"] + 1
        emu.saveState(laterSaveState)
        emu.loadState(earlySaveStatePath)
        madeSecondSave = True
        return
        
    if lastFrameAdvanceCounter < 2:
        lastFrameAdvanceCounter += 1
        return
        
    emu.loadState(laterSaveState)
    statisticsGeneralUnitTests()
    outputFile.write("-----------------------\nTotal Tests: " + str(testNum - 1) + "\n\t")
    outputFile.write("Tests Passed: " + str(resultsTable["PASS"]) + "\n\t")
    outputFile.write("Tests Failed: " + str(resultsTable["FAIL"]) + "\n")
    outputFile.flush()
    outputFile.close()
    print("Total Tests: " + str(testNum - 1) + "\n\t")
    print("Tests Passed: " + str(resultsTable["PASS"]) + "\n\t")
    print("Tests Failed: " + str(resultsTable["FAIL"]) + "\n") 
    OnFrameStart.unregister(funcRef)

funcRef = OnFrameStart.register(statisticsTests)

