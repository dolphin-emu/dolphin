import memory

testNum = 1
outputFile = open("PythonExamplesAndTests/TestResults/PythonMemoryTestsResults.txt", "w")
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0

def clearAddress(address):
    memory.writeTo(address, "s64", 0)


def readFromAndWriteToTest():
    global testNum
    global resultsTable
    
    address = 0X80000123
    unsignedByteToWrite = 230
    signedByteToWrite = -54
    unsignedShortToWrite = 60000
    signedShortToWrite = -16128
    unsignedIntToWrite = 4000000000
    signedIntToWrite = -2000000009
    unsignedLongLongToWrite = 10123456789123456789
    signedLongLongToWrite = -9000000004567
    floatToWrite = 3.00000375e5
    doubleToWrite = 6.022e+23


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
    
    typeTable["u64"] = unsignedLongLongToWrite
    typeTable["Unsigned_Long_Long"] = unsignedLongLongToWrite
    typeTable["unsignedLongLong"] = unsignedLongLongToWrite
    typeTable["unsigned_64"] = unsignedLongLongToWrite
    typeTable["unsigned 64"] = unsignedLongLongToWrite
    typeTable["unsigned64"] = unsignedLongLongToWrite

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

    typeTable["s64"] = signedLongLongToWrite
    typeTable["Signed_Long_Long"] = signedLongLongToWrite
    typeTable["signed long long"] = signedLongLongToWrite
    typeTable["SignedLongLong"] = signedLongLongToWrite
    typeTable["signed_64"] = signedLongLongToWrite
    typeTable["signed 64"] = signedLongLongToWrite
    typeTable["signed64"] = signedLongLongToWrite

    typeTable["float"] = floatToWrite

    typeTable["DoUBle"] = doubleToWrite
	
    for key, value in typeTable.items():
        clearAddress(address)
        outputFile.write("Test " + str(testNum) + ":\n")
        memory.writeTo(address, key, value)
        outputFile.write("\tWrote value of " + str(value) + " to " + str(address) + " as type " + key + "\n")
        outputFile.write("\tValue at address " + str(address) + " as type " + key + " is now: " + str(memory.readFrom(address, key)) + "\n")
        outputFile.flush()
        if value == memory.readFrom(address, key):
            outputFile.write("PASS!\n\n")
            resultsTable["PASS"] = resultsTable["PASS"] + 1
        else:
            outputFile.write("FAILURE! Value read was not the same as the value written...\n\n")
            resultsTable["FAIL"] = resultsTable["FAIL"] + 1
        testNum = testNum + 1

def readAndWriteBytesTest():
    global testNum
    global resultsTable
    baseAddress = 0X80000123
    unsignedBytesTable = {}
    unsignedBytesTable[baseAddress] = 58
    unsignedBytesTable[baseAddress + 1] = 180
    unsignedBytesTable[baseAddress + 2] = 243
    clearAddress(baseAddress)
    memory.writeBytes(unsignedBytesTable)
    outputBytesTable = memory.readUnsignedBytes(baseAddress, 3)
    outputFile.write("Test: " + str(testNum) + "\n")
    isFailure = False
    for i in range(3):
        outputFile.write("\tWrote unsigned byte array value of " + str(unsignedBytesTable[baseAddress + i]) + " to address " + str(baseAddress + i) + "\n")
        outputFile.write("\tValue in resulting spot in memory is: " + str(outputBytesTable[baseAddress + i]) + "\n\n")
        if unsignedBytesTable[baseAddress + i] != outputBytesTable[baseAddress + i]:
            isFailure = True

    if isFailure:
        outputFile.write("FAILURE! Value read was not the same as the value written!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1
    else:
        outputFile.write("PASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
        
    testNum = testNum + 1
    
    isFailure = False
    signedBytesTable = {}
    signedBytesTable[baseAddress] = 32
    signedBytesTable[baseAddress + 1] = -43
    signedBytesTable[baseAddress + 2] = 119
    clearAddress(baseAddress)
    memory.writeBytes(signedBytesTable)
    outputBytesTable = memory.readSignedBytes(baseAddress, 3)
    outputFile.write("Test " + str(testNum) + ":\n")
    for i in range(3):
        outputFile.write("\tWrote signed byte array value of " + str(signedBytesTable[baseAddress + i]) + " to address "  + str(baseAddress + i) + "\n")
        outputFile.write("\tValue in resulting spot in memory is: " + str(outputBytesTable[baseAddress + i]) + "\n\n")
        if signedBytesTable[baseAddress + i] != outputBytesTable[baseAddress + i]:
            isFailure = True
            
    if isFailure:
        outputFile.write("FAILURE! Value read was not the same as the value written!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1
    else:
        outputFile.write("PASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    testNum = testNum + 1

def stringFunctionsTest():
    global testNum
    global resultsTable
    baseAddress = 0X80000123
    sampleString = "Star Fox"
    clearAddress(baseAddress)
    memory.writeString(baseAddress, sampleString)
	
    resultString = memory.readNullTerminatedString(baseAddress)
    outputFile.write("Test: " + str(testNum) + ":\n")
    outputFile.write("\tWrote " + "\"" + str(sampleString) + "\" to memory. Null terminated string read back from that spot in memory was: \"" + str(resultString) + "\"\n")
    if resultString == sampleString:
        outputFile.write("PASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("FAILURE! Value read from memory was not the same as the value written!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1
    testNum = testNum + 1

    clearAddress(baseAddress)
    memory.writeString(baseAddress, sampleString)
    resultString = memory.readFixedLengthString(baseAddress, 3)
    outputFile.write("Test: " + str(testNum) + ":\n")
    outputFile.write("\tWrote: \"" + sampleString + "\" to memory. Reading back the first 3 characters of the sample string... Resulting string was: \"" + resultString + "\"\n")
    if sampleString[0:3] == resultString:
        outputFile.write("PASS!\n\n")
        resultsTable["PASS"]  = resultsTable["PASS"] + 1
    else:
        outputFile.write("FAILURE! Substring returned was not expected value!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1
    testNum = testNum + 1

def PythonMemoryTests():
    global testNum
    global resultsTable
    readFromAndWriteToTest()
    readAndWriteBytesTest()
    stringFunctionsTest()
    outputFile.write("Total Tests: " + str(testNum - 1) + "\n")
    outputFile.write("\tTests Passed: " + str(resultsTable["PASS"]) + "\n")
    outputFile.write("\tTests Failed: " + str(resultsTable["FAIL"]) + "\n")
    outputFile.flush()
    outputFile.close()
    print("Total Tests: " + str(testNum - 1))
    print("\tTests Passed: " + str(resultsTable["PASS"]))
    print("\tTests Failed: " + str(resultsTable["FAIL"]))

PythonMemoryTests()