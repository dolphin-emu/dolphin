require ("emu")
require ("statistics")
require ("gc_controller")

funcRef = 0
baseMovieFilePath  = "testBaseRecording.dtm"
earlySaveStatePath = "testEarlySaveState.sav"
laterMovieName = "laterRecording.sav.dtm"
laterSaveState = "laterRecording.sav"

testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0


function singleStatisticTest(functionName, resultOfFunction)
	io.write("Test " .. tostring(testNum) .. ":\n\tTesting statistics:" .. functionName .. " function...\n")
	testNum = testNum + 1
	io.write("\tstatistics:" .. functionName .. " = " .. tostring(resultOfFunction) .. "\nPASS!\n\n")
	resultsTable["PASS"] = resultsTable["PASS"] + 1
end

function statisticsGeneralUnitTests()
	singleStatisticTest("isRecordingInput()", statistics:isRecordingInput())
	singleStatisticTest("isRecordingInputFromSaveState()", statistics:isRecordingInputFromSaveState())
	singleStatisticTest("isPlayingInput()", statistics:isPlayingInput())
	singleStatisticTest("isMovieActive()", statistics:isMovieActive())
	singleStatisticTest("getMovieLength()", statistics:getMovieLength())	
	singleStatisticTest("getRerecordCount()", statistics:getRerecordCount())
	singleStatisticTest("getCurrentInputCount()", statistics:getCurrentInputCount())
	singleStatisticTest("getTotalInputCount()", statistics:getTotalInputCount())	
	singleStatisticTest("getCurrentLagCount()", statistics:getCurrentLagCount())
	singleStatisticTest("getTotalLagCount()", statistics:getTotalLagCount())
	singleStatisticTest("isGcControllerInPort(1)", gc_controller:isGcControllerInPort(1))
	singleStatisticTest("isUsingPort(1)", gc_controller:isUsingPort(1))
	singleStatisticTest("getRAMSize()", statistics:getRAMSize())
	singleStatisticTest("getL1CacheSize()", statistics:getL1CacheSize())
	singleStatisticTest("getFakeVMemSize()", statistics:getFakeVMemSize())
	singleStatisticTest("getExRAMSize()", statistics:getExRAMSize())
end

file = io.open("LuaExamplesAndTests/TestResults/LuaStatisticsTestsResults.txt", "w")
io.output(file)

function statisticsTests()
emu:frameAdvance()
emu:frameAdvance()
emu:playMovie(baseMovieFilePath)
emu:loadState(earlySaveStatePath)
initialFrameNumber = statistics:getCurrentFrame()
for i = 1, 10 do 
	emu:frameAdvance()
end
laterFrameNumber = statistics:getCurrentFrame()
io.write("Test 1:\n\tTesting that statistics:getCurrentFrame() after frame advancing 10 frames is 10 more than its original value...\n")
testNum = testNum + 1
if (laterFrameNumber ~= initialFrameNumber + 10) then
	io.write("\t" .. tostring(initialFrameNumber) .. " + 10 != " .. tostring(laterFrameNumber) .. "\nFAILURE!\n\n")
	resultsTable["FAIL"] = resultsTable["FAIL"] + 1
else
	io.write("\t" .. tostring(initialFrameNumber) .. " + 10 == " .. tostring(laterFrameNumber) .. "\nPASS!\n\n")
	resultsTable["PASS"] = resultsTable["PASS"] + 1
end
emu:saveState(laterSaveState)
emu:loadState(earlySaveStatePath)
emu:frameAdvance()
emu:frameAdvance()
emu:loadState(laterSaveState)

statisticsGeneralUnitTests()
io.write("-----------------------\nTotal Tests: " .. tostring(testNum - 1) .. "\n\t")
io.write("Tests Passed: " .. tostring(resultsTable["PASS"]) .. "\n\t")
io.write("Tests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n") 
print("Total Tests: " .. tostring(testNum - 1) .. "\n\t")
print("Tests Passed: " .. tostring(resultsTable["PASS"]) .. "\n\t")
print("Tests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n") 
io.flush()
io.close()
OnFrameStart:unregister(funcRef)
end

funcRef = OnFrameStart:register(statisticsTests)

