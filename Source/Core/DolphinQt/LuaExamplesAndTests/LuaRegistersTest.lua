require ("registers")

funcRef = 0
testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

unsignedByte = 230
signedByte = -99
unsignedShort = 60000
signedShort = -24128
unsignedInt = 4000000000
signedInt = -2000000009
signedLongLong = -9000000004567
floatValue = 3.00000375e5
doubleValue = 6.022e+23

generalRegistersTable = {}
floatRegistersTable = {}
pcRegister = "PC"
lrRegister = "LR"

indexToTypeAndValuesTable = {}

generalRegistersInitialStateBackup = {}
floatRegistersInitialStateBackup = {}
pcRegisterInitialStateBackup = 0
lrRegisterInitialStateBackup = 0

unsignedByteArray = {}
signedByteArray = {}

unsignedByteArray[1] = 200
unsignedByteArray[2] = 100
unsignedByteArray[3] = 31
unsignedByteArray[4] = 128
unsignedByteArray[5] = 1
unsignedByteArray[6] = 0
unsignedByteArray[7] = 2
unsignedByteArray[8] = 89
unsignedByteArray[9] = 110
unsignedByteArray[10] = 255
unsignedByteArray[11] = 9
unsignedByteArray[12] = 90
unsignedByteArray[13] = 200
unsignedByteArray[14] = 21
unsignedByteArray[15] = 78
unsignedByteArray[16] = 2

signedByteArray[1] = -45
signedByteArray[2] = -128
signedByteArray[3] = 0
signedByteArray[4] = 31
signedByteArray[5] = 2
signedByteArray[6] = 127
signedByteArray[7] = -90
signedByteArray[8] = 90
signedByteArray[9] = -8
signedByteArray[10] = 17
signedByteArray[11] = 100
signedByteArray[12] = 51
signedByteArray[13] = -100
signedByteArray[14] = 82
signedByteArray[15] = -10
signedByteArray[16] = 63


function clearRegister(registerName)
	if registerName[0] == 'f' or registerName[0] == 'F' then
		registers:setRegister(registerName "s64", 0, 0)
		registers:setRegister(registerName, "s64", 0, 8)
	else
		registers:setRegister(registerName, "u32", 0, 0)
	end
end

function getSizeOfType(valueType)
	if valueType == "u8" then
		return 1
	elseif valueType == "s8" then
		return 1
	elseif valueType == "u16" then
		return 2
	elseif valueType == "s16" then
		return 2
	elseif valueType == "u32" then
		return 4
	elseif valueType == "s32" then
		return 4
	elseif valueType == "float" then
		return 4
	elseif valueType == "s64" then
		return 8
	elseif valueType == "double" then
		return 8
	else
		io.write("ERROR: Unknown type encountered in getSizeOfType() function!\n\n")
		io.flush()
		return nill
	end
end

function setRegisterTestCase(registerName, valueType, valueToWrite)
	clearRegister(registerName)
	io.write("Test " .. tostring(testNum) .. ":\n\t")
	io.write("Testing writing " .. tostring(valueToWrite) .. " to " .. registerName .. " as a " .. valueType .. " and making sure that the value read back out after writing is the same value...\n\t")
	testNum = testNum + 1
	registers:setRegister(registerName, valueType, valueToWrite)
	valueReadOut = registers:getRegister(registerName, valueType)
	if valueReadOut == valueToWrite then
		io.write("Value read out of " .. tostring(valueReadOut) .. " == value written, which was " .. tostring(valueToWrite) .. "\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("Value read out of " .. tostring(valueReadOut) .. " != value written, which was " .. tostring(valueToWrite) .. "\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
end

function setRegisterWithOffsetTestCase(registerName, valueType, valueToWrite, offset)
	clearRegister(registerName)
	io.write("Test " .. tostring(testNum) .. ":\n\t")
	io.write("Testing writing " .. tostring(valueToWrite) .. " to " .. registerName .. " as a " .. valueType .. " with an offset of " .. tostring(offset) .. " and making sure that the value read back out after writing is the same value...\n\t")
	testNum = testNum + 1
	registers:setRegister(registerName, valueType, valueToWrite, offset)
	valueReadOut = registers:getRegister(registerName, valueType, offset)
	if valueReadOut == valueToWrite then
		io.write("Value read out of " .. tostring(valueReadOut) .. " == value written, which was " .. tostring(valueToWrite) .. "\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("Value read out of " .. tostring(valueReadOut) .. " != value written, which was " .. tostring(valueToWrite) .. "\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
end

function byteArrayTestCase(registerName, byteArray, arraySize, offset, isUnsigned)
	clearRegister(registerName)
	typeName = "signed"
	if isUnsigned then
		typeName = "unsigned"
	end
	truncatedByteArray = {}
	i = 0
	for key, value in ipairs(byteArray) do
		if i < arraySize then
			truncatedByteArray[key] = value
		end
		i = i + 1
	end
	io.write("Test " .. tostring(testNum) .. ":\n\t")
	io.write("Testing writing the following bytes to register " .. registerName .. " with an offset of " .. tostring(offset) .. " and making sure that the value read back as a " .. typeName .. " array is the same as the value written:\n\tValue Written:           ")
	firstVal = true
	originalValuesString = ""
	for key, value in ipairs(truncatedByteArray) do
		if firstVal == false then
			originalValuesString = originalValuesString .. ", "
		end
		originalValuesString = originalValuesString .. tostring(value)
		firstVal = false
	end
	io.write(originalValuesString .. "\n")
	testNum = testNum + 1
	valueReadOut = nill
	registers:setRegisterFromByteArray(registerName, truncatedByteArray, offset)
	if isUnsigned then
		valueReadOut = registers:getRegisterAsUnsignedByteArray(registerName, arraySize, offset)
	else
		valueReadOut = registers:getRegisterAsSignedByteArray(registerName, arraySize, offset)
	end
	finalValuesString = ""
	firstVal = true
	for key, value in ipairs(valueReadOut) do
		if firstVal == false then
			finalValuesString = finalValuesString .. ", "
		end
		finalValuesString = finalValuesString .. tostring(value)
		firstVal = false
	end
	io.write("\tValue read back out was: " .. tostring(finalValuesString) .. "\n")
	if originalValuesString == finalValuesString then
		io.write("\tResults were equal.\nPASS!\n\n")
		resultsTable["PASS"] = resultsTable["PASS"] + 1
	else
		io.write("\tResults were NOT equal.\nFAILURE!\n\n")
		resultsTable["FAIL"] = resultsTable["FAIL"] + 1
	end
end

function setRegisterUnitTests()
	io.write("Testing writing to/reading from all of the integer registers:\n\n")
	for registerNumber = 0, 31 do
		for i = 1, 7 do
			registerName = generalRegistersTable[registerNumber]
			valueType = indexToTypeAndValuesTable[i]["typeName"]
			valueToWrite = indexToTypeAndValuesTable[i]["value"]
			setRegisterTestCase(registerName, valueType, valueToWrite)
		end
	end


	for i = 1, 7 do
		valueType = indexToTypeAndValuesTable[i]["typeName"]
		valueToWrite = indexToTypeAndValuesTable[i]["value"]
		setRegisterTestCase("PC", valueType, valueToWrite)
	end


	for i = 1, 7 do
		valueType = indexToTypeAndValuesTable[i]["typeName"]
		valueToWrite = indexToTypeAndValuesTable[i]["value"]
		setRegisterTestCase("LR", valueType, valueToWrite)
	end
	
	io.write("--------------------------------------\n\nTesting writing to/reading from all of the floating point registers:\n\n")
	for registerNumber = 0, 31 do
		for i = 1, 9 do
			registerName = floatRegistersTable[registerNumber]
			valueType = indexToTypeAndValuesTable[i]["typeName"]
			valueToWrite = indexToTypeAndValuesTable[i]["value"]
			setRegisterTestCase(registerName, valueType, valueToWrite)
		end
	end
	
	io.write("--------------------------------------\n\nTesting writing to integer registers with an offset:\n\n")
	for i = 1, 7 do
		valueType = indexToTypeAndValuesTable[i]["typeName"]
		size = getSizeOfType(valueType)
		valueToWrite = indexToTypeAndValuesTable[i]["value"]
		offsetValue = 0
		while offsetValue + size <= 4 do
			setRegisterWithOffsetTestCase("r31", valueType, valueToWrite, offsetValue)
			offsetValue = offsetValue + 1
		end
	end
	
	io.write("--------------------------------------\n\nTesting writing to floating point registers with an offset:\n\n")
	for i = 1, 9 do
		valueType = indexToTypeAndValuesTable[i]["typeName"]
		size = getSizeOfType(valueType)
		valueToWrite = indexToTypeAndValuesTable[i]["value"]
		offsetValue = 0
		while offsetValue + size <= 16 do
			setRegisterWithOffsetTestCase("f31", valueType, valueToWrite, offsetValue)
			offsetValue = offsetValue + 1
		end
	end
end




function setRegisterFromByteArrayUnitTests()
	io.write("--------------------------------------\n\nTesting writing to/reading from registers as an unsigned byte array:\n\n")
	for arraySize = 1, 4 do
		offsetValue = 0
		while offsetValue + arraySize <= 4 do
			byteArrayTestCase("r31", unsignedByteArray, arraySize, offsetValue, true)
			offsetValue = offsetValue + 1
		end
	end
	
	for arraySize = 1, 16 do
		offsetValue = 0
		while offsetValue + arraySize <= 16 do
			byteArrayTestCase("f31", unsignedByteArray, arraySize, offsetValue, true)
			offsetValue = offsetValue + 1
			end
	end
	
	io.write("--------------------------------------\n\nTesting writing to/reading from registers as a signed byte array:\n\n")
	for arraySize = 1, 4 do
		offsetValue = 0
		while offsetValue + arraySize <= 4 do
			byteArrayTestCase("r31", signedByteArray, arraySize, offsetValue, false)
			offsetValue = offsetValue + 1
		end
	end
	
	for arraySize = 1, 16 do
		offsetValue = 0
		while offsetValue + arraySize <= 16 do
			byteArrayTestCase("f31", signedByteArray, arraySize, offsetValue, false)
			offsetValue = offsetValue + 1
		end
	end
end


function registersTests()
file = io.open("LuaExamplesAndTests/TestResults/LuaRegistersTestsResults.txt", "w")
io.output(file)

-- Getting the names of the registers, and storing the initial values of the registers
-- so that they can be restored after all of the tests are done...
for i = 0, 31 do
	generalRegistersTable[i] = "R" .. tostring(i)
	floatRegistersTable[i] = "F" .. tostring(i)
	generalRegistersInitialStateBackup[i] = registers:getRegister(generalRegistersTable[i], "u32", 0)
	floatRegistersInitialStateBackup[i] = registers:getRegister(floatRegistersTable[i], "double", 0)
end

pcRegisterInitialStateBackup = registers:getRegister("PC", "u32", 0)
lrRegisterInitialStateBackup = registers:getRegister("LR", "u32", 0)

indexToTypeAndValuesTable[1] = {typeName="u8", value=unsignedByte}
indexToTypeAndValuesTable[2] = {typeName="u16", value=unsignedShort}
indexToTypeAndValuesTable[3] = {typeName="u32", value=unsignedInt}
indexToTypeAndValuesTable[4] = {typeName="s8", value=signedByte}
indexToTypeAndValuesTable[5] = {typeName="s16", value=signedShort}
indexToTypeAndValuesTable[6] = {typeName="s32", value=signedInt}
indexToTypeAndValuesTable[7] = {typeName="float", value=floatValue}
indexToTypeAndValuesTable[8] = {typeName="s64", value=signedLongLong}
indexToTypeAndValuesTable[9] = {typeName="double", value=doubleValue}



setRegisterUnitTests()
setRegisterFromByteArrayUnitTests()

-- Restoring the contents of the registers
for i = 0, 31 do
	registers:setRegister(generalRegistersTable[i], "u32", generalRegistersInitialStateBackup[i], 0)
	registers:setRegister(floatRegistersTable[i], "double", floatRegistersInitialStateBackup[i], 0)
end

registers:setRegister("PC", "u32", pcRegisterInitialStateBackup, 0)
registers:setRegister("LR", "u32", lrRegisterInitialStateBackup, 0)

io.write("Total Tests: " .. tostring(testNum - 1) .. "\n\t")
io.write("Tests Passed: " .. tostring(resultsTable["PASS"]) .. "\n\t")
io.write("Tests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n")
print("Total Tests: " .. tostring(testNum - 1) .. "\n\t")
print("Tests Passed: " .. tostring(resultsTable["PASS"]) .. "\n\t")
print("Tests Failed: " .. tostring(resultsTable["FAIL"]) .. "\n")
io.flush()
io.close()
OnFrameStart:unregister(funcRef)
end

funcRef = OnFrameStart:register(registersTests)

