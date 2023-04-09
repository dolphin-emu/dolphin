require ("emu")
require ("memory")

funcRef = 0
testNum = 1

function clearAddress(address)
	memory:writeTo(address, "s64", 0)
end
function readFromAndWriteToTest()
	resultsTable = {}
	resultsTable["TOTAL"] = 0
	resultsTable["PASS"] = 0
	resultsTable["FAIL"] = 0
	address = 0X80000123
	unsignedByteToWrite = 230
	signedByteToWrite = -54
	unsignedShortToWrite = 60000
	signedShortToWrite = -16128
	unsignedIntToWrite = 4000000000
	signedIntToWrite = -2000000009
	unsignedlongLongToWrite = 10123456789123456789
	signedLongLongToWrite = -9000000004567
	floatToWrite = 3.00000375e5
	doubleToWrite = 6.022e+23

	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()
	emu:frameAdvance()

	typeTable = {}
	
	typeTable["u8"] = unsignedByteToWrite

	typeTable["unsigned_byte"] = unsignedByteToWrite
	typeTable["unsigned byte"] = unsignedByteToWrite
	typeTable["UnsignedByte"] = unsignedByteToWrite

	typeTable["unsigned_8"] = unsignedByteToWrite
	typeTable["UNSIGNED 8"] = unsignedByteToWrite
	typeTable["unsigned8"] = unsignedByteToWrite



	typeTable["u16"] = unsignedShortToWrite
	typeTable["Unsigned_16"] = unsignedShortToWrite
	typeTable["UNSIGNED 16"] = unsignedShortToWrite
	typeTable["Unsigned16"] = unsignedShortToWrite



	typeTable["u32"] = unsignedIntToWrite
	typeTable["UNSIGNED_INT"] = unsignedIntToWrite
	typeTable["unsigned int"] = unsignedIntToWrite
	typeTable["UnsignedInt"] = unsignedIntToWrite
	typeTable["unsigned_32"] = unsignedIntToWrite
	typeTable["Unsigned 32"] = unsignedIntToWrite
	typeTable["unsigned32"] = unsignedIntToWrite



	typeTable["s8"] = signedByteToWrite
	typeTable["Signed_Byte"] = signedByteToWrite
	typeTable["signed ByTE"] = signedByteToWrite
	typeTable["SignedByte"] = signedByteToWrite
	typeTable["SIGNED_8"] = signedByteToWrite
	typeTable["signed 8"] = signedByteToWrite
	typeTable["signed8"] = signedByteToWrite


	typeTable["S16"] = signedShortToWrite
	typeTable["SIGNED_16"] = signedShortToWrite
	typeTable["signed 16"] = signedShortToWrite
	typeTable["signed16"] = signedShortToWrite


	typeTable["s32"] = signedIntToWrite
	typeTable["signed_int"] = signedIntToWrite
	typeTable["signed int"] = signedIntToWrite
	typeTable["SignedInt"] = signedIntToWrite
	typeTable["signed_32"] = signedIntToWrite
	typeTable["signed 32"] = signedIntToWrite
	typeTable["signed32"] = signedIntToWrite
	typeTable["signed32"] = signedShortToWrite


	typeTable["s64"] = signedLongLongToWrite
	typeTable["Signed_Long_Long"] = signedLongLongToWrite
	typeTable["signed long long"] = signedLongLongToWrite
	typeTable["SignedLongLong"] = signedLongLongToWrite
	typeTable["signed_64"] = signedLongLongToWrite
	typeTable["signed 64"] = signedLongLongToWrite
	typeTable["signed64"] = signedLongLongToWrite


	typeTable["float"] = floatToWrite

	typeTable["DoUBle"] = doubleToWrite
	
	successCount = 0
	failureCount = 0
	for key, value in pairs(typeTable) do
		io.write("Test " .. tostring(testNum) .. ":\n")
		clearAddress(address)
		memory:writeTo(address, key, value)
		io.write("\tWrote value of " .. tostring(value) .. " to " .. tostring(address) .. " as type " .. key .. "\n")
		io.write("\tValue at address " .. tostring(address) .. " as type " .. key .. " is now: " .. tostring(memory:readFrom(address, key)) .. "\n")
		if (value == memory:readFrom(address, key)) then
			io.write("PASS!\n\n")
			successCount = successCount + 1
		else
			io.write("FAILURE! Value read was not the same as the value written...\n\n")
			failureCount = failureCount + 1
		end
		testNum = testNum + 1
	end

	resultsTable["PASS"] = successCount
	resultsTable["FAIL"] = failureCount
	io.flush()
	return resultsTable
end

function readAndWriteBytesTest()
	baseAddress = 0X80000123
	resultsTable = {}
	resultsTable["PASS"] = 0
	resultsTable["FAIL"] = 0
	unsignedBytesTable = {}
	unsignedBytesTable[baseAddress] = 58
	unsignedBytesTable[baseAddress + 1] = 180
	unsignedBytesTable[baseAddress + 2] = 243
	clearAddress(baseAddress)
	memory:writeBytes(unsignedBytesTable)
	outputBytesTable = memory:readUnsignedBytes(baseAddress, 3)
	io.write("Test: " .. tostring(testNum) .. "\n")
	isFailure = false
	for i = 0, 2 do
		io.write("\tWrote unsigned byte array value of " .. tostring(unsignedBytesTable[baseAddress + i]) .. " to address " .. tostring(baseAddress + i) .. "\n")
		io.write("\tValue in resulting spot in memory is: " .. tostring(outputBytesTable[baseAddress + i]) .. "\n\n")
		if (unsignedBytesTable[baseAddress + i] ~= outputBytesTable[baseAddress + i]) then
			isFailure = true
		end
	end
	if (isFailure) then
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		io.write("FAILURE! Value read was not the same as the value written!\n\n")
	else
		resultsTable["PASS"] = resultsTable["PASS"] + 1
		io.write("PASS!\n\n")
	end
	io.flush()
	testNum = testNum + 1
	isFailure = false
	signedBytesTable = {}
	signedBytesTable[baseAddress] = 32
	signedBytesTable[baseAddress + 1] = -43
	signedBytesTable[baseAddress + 2] = 119
	clearAddress(baseAddress)
	memory:writeBytes(signedBytesTable)
	outputBytesTable = memory:readSignedBytes(baseAddress, 3)
	io.write("Test " .. tostring(testNum) .. ":\n")
	for i = 0, 2 do
		io.write("\tWrote signed byte array value of " .. tostring(signedBytesTable[baseAddress + i]) .. " to address " .. tostring(baseAddress + i) .. "\n")
		io.write("\tValue in resulting spot in memory is: " .. tostring(outputBytesTable[baseAddress + i]) .. "\n\n")
		if (signedBytesTable[baseAddress + i] ~= outputBytesTable[baseAddress + i]) then
			isFailure = true
		end
	end
	if (isFailure) then
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
		io.write("FAILURE! Value read was not the same as the value written!\n\n")
	else
		resultsTable["PASS"] = resultsTable["PASS"] + 1
		io.write("PASS!\n\n")
	end
	testNum = testNum + 1
	io.flush()
	return resultsTable
end

function stringFunctionsTest()
	baseAddress = 0X80000123
	resultsTable = {}
	resultsTable["PASS"] = 0
	resultsTable["FAIL"] = 0
	sampleString = "Star Fox"
	isFailure = false
	clearAddress(baseAddress)
	memory:writeString(baseAddress, sampleString)
	
	resultString = memory:readNullTerminatedString(baseAddress)
	io.write("Test: " .. tostring(testNum) .. ":\n")
	io.write("\tWrote " .. "\"" .. tostring(sampleString) .. "\" to memory. Null terminated string read back from that spot in memory was: \"" .. tostring(resultString) .. "\"\n")
	if (resultString == sampleString) then
		io.write("PASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("FAILURE! Value read from memory was not the same as the value written!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
	testNum = testNum + 1

	clearAddress(baseAddress)
	memory:writeString(baseAddress, sampleString)
	resultString = memory:readFixedLengthString(baseAddress, 3)
	io.write("Test: " .. tostring(testNum) .. ":\n")
	io.write("\tWrote: \"" .. sampleString .. "\" to memory. Reading back the first 3 characters of the sample string... Resulting string was: \"" .. resultString .. "\"\n")
	if (string.sub(sampleString, 1, 3) == resultString) then
		io.write("PASS!\n\n")
		resultsTable["PASS"]  = resultsTable["PASS"] + 1
	else
		io.write("FAILURE! Substring returned was not expected value!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
	testNum = testNum + 1
	return resultsTable
end

function addTestResultsToTable(finalTable, tempTable)
	finalTable["PASS"] = finalTable["PASS"] + tempTable["PASS"]
	finalTable["FAIL"] = finalTable["FAIL"] + tempTable["FAIL"]
end

function LuaMemoryApiTest()
	testResults = {}
	testResults["TOTAL"] = 0
	testResults["PASS"] = 0
	testResults["FAIL"] = 0
	file = io.open("LuaExamplesAndTests/TestResults/LuaMemoryApiTestsResults.txt", "w")
	io.output(file)
	addTestResultsToTable(testResults, readFromAndWriteToTest())
	addTestResultsToTable(testResults, readAndWriteBytesTest())
	addTestResultsToTable(testResults, stringFunctionsTest())
	testResults["TOTAL"] = testNum - 1
	io.write("Total Tests: " .. tostring(testResults["TOTAL"]) .. "\n")
	io.write("\tTests Passed: " .. tostring(testResults["PASS"]) .. "\n")
	io.write("\tTests Failed: " .. tostring(testResults["FAIL"]) .. "\n")
	print("Total Tests: " .. tostring(testResults["TOTAL"]) .. "\n")
	print("\tTests Passed: " .. tostring(testResults["PASS"]) .. "\n")
	print("\tTests Failed: " .. tostring(testResults["FAIL"]) .. "\n")
	io.flush()
	io.close()
	OnFrameStart:unregister(funcRef)
end

funcRef = OnFrameStart:register(LuaMemoryApiTest)