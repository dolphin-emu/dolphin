import registers

outputFile = open("PythonExamplesAndTests/TestResults/PythonRegistersTestsResults.txt",  "w")
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
unsignedLongLong = 9812345678901

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

def clearRegister(registerName):
    if registerName[0] == 'f' or registerName[0] == 'F':
        registers.setRegister(registerName, "s64", 0, 0)
        registers.setRegister(registerName, "s64", 0, 8)
    else:
        registers.setRegister(registerName, "u32", 0, 0)

def getSizeOfType(valueType):
    if valueType == "u8":
        return 1
    elif valueType == "s8":
        return 1
    elif valueType == "u16":
        return 2
    elif valueType == "s16":
        return 2
    elif valueType == "u32":
        return 4
    elif valueType == "s32":
        return 4
    elif valueType == "float":
        return 4
    elif valueType == "s64":
        return 8
    elif valueType == "double":
        return 8
    elif valueType == "u64":
        return 8
    else:
        raise("ERROR: Unknown type encountered in getSizeOfType() function!\n\n")
        return 0

def setRegisterTestCase(registerName, valueType, valueToWrite):
    global testNum
    global resultsTable
    clearRegister(registerName)
    outputFile.write("Test " + str(testNum) + ":\n\t")
    outputFile.write("Testing writing " + str(valueToWrite) + " to " + registerName + " as a " + valueType + " and making sure that the value read back out after writing is the same value...\n\t")
    testNum = testNum + 1
    registers.setRegister(registerName, valueType, valueToWrite)
    valueReadOut = registers.getRegister(registerName, valueType)
    if valueReadOut == valueToWrite:
        outputFile.write("Value read out of " + str(valueReadOut) + " == value written, which was " + str(valueToWrite) + "\nPASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("Value read out of " + str(valueReadOut) + " != value written, which was " + str(valueToWrite) + "\nFAILURE!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1


def setRegisterWithOffsetTestCase(registerName, valueType, valueToWrite, offset):
    global testNum
    global resultsTable
    clearRegister(registerName)
    outputFile.write("Test " + str(testNum) + ":\n\t")
    outputFile.write("Testing writing " + str(valueToWrite) + " to " + registerName + " as a " + valueType + " with an offset of " + str(offset) + " and making sure that the value read back out after writing is the same value...\n\t")
    testNum = testNum + 1
    registers.setRegister(registerName, valueType, valueToWrite, offset)
    valueReadOut = registers.getRegister(registerName, valueType, offset)
    if valueReadOut == valueToWrite:
        outputFile.write("Value read out of " + str(valueReadOut) + " == value written, which was " + str(valueToWrite) + "\nPASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("Value read out of " + str(valueReadOut) + " != value written, which was " + str(valueToWrite) + "\nFAILURE!\n\n")
        resultsTabe["FAIL"] = resultsTable["FAIL"] + 1


def byteArrayTestCase(registerName, byteArray, arraySize, offset, isUnsigned):
    global testNum
    global resultsTable
    
    clearRegister(registerName)
    typeName = "signed"
    if isUnsigned:
        typeName = "unsigned"
    truncatedByteArray = {}
    i = 0
    for key, value in byteArray.items():
        if i < arraySize:
            truncatedByteArray[key] = value
        i = i + 1
    outputFile.write("Test " + str(testNum) + ":\n\t")
    outputFile.write("Testing writing the following bytes to register " + registerName + " with an offset of " + str(offset) + " and making sure that the value read back as a " + typeName + " byte array is the same as the value written:\n\tValue Written:           ")
    firstVal = True
    originalValuesString = ""
    for key, value in truncatedByteArray.items():
        if firstVal == False:
            originalValuesString = originalValuesString + ", "
        originalValuesString = originalValuesString + str(value)
        firstVal = False
        
    outputFile.write(originalValuesString + "\n")
    testNum = testNum + 1
    valueReadOut = None
    registers.setRegisterFromByteArray(registerName, truncatedByteArray, offset)
    if isUnsigned:
        valueReadOut = registers.getRegisterAsUnsignedByteArray(registerName, arraySize, offset)
    else:
        valueReadOut = registers.getRegisterAsSignedByteArray(registerName, arraySize, offset)
    finalValuesString = ""
    firstVal = True
    for key, value in valueReadOut.items():
        if firstVal == False:
            finalValuesString = finalValuesString + ", "
        finalValuesString = finalValuesString + str(value)
        firstVal = False
    outputFile.write("\tValue read back out was: " + str(finalValuesString) + "\n")
    if originalValuesString == finalValuesString:
        outputFile.write("\tResults were equal.\nPASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("\tResults were NOT equal.\nFAILURE!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1

def setRegisterUnitTests():
    global generalRegistersTable
    global indexToTypeAndValuesTable
    outputFile.write("Testing writing to/reading from all of the integer registers:\n\n")
    for registerNumber in range(32):
        for i in range(1, 8):
            registerName = generalRegistersTable[registerNumber]
            valueType = indexToTypeAndValuesTable[i]["typeName"]
            valueToWrite = indexToTypeAndValuesTable[i]["value"]
            setRegisterTestCase(registerName, valueType, valueToWrite)

    for i in range(1, 8):
        valueType = indexToTypeAndValuesTable[i]["typeName"]
        valueToWrite = indexToTypeAndValuesTable[i]["value"]
        setRegisterTestCase("PC", valueType, valueToWrite)

    for i in range(1, 8):
        valueType = indexToTypeAndValuesTable[i]["typeName"]
        valueToWrite = indexToTypeAndValuesTable[i]["value"]
        setRegisterTestCase("LR", valueType, valueToWrite)
	
    outputFile.write("--------------------------------------\n\nTesting writing to/reading from all of the floating point registers:\n\n")
    for registerNumber in range(32):
        for i in range(1, 11):
            registerName = floatRegistersTable[registerNumber]
            valueType = indexToTypeAndValuesTable[i]["typeName"]
            valueToWrite = indexToTypeAndValuesTable[i]["value"]
            setRegisterTestCase(registerName, valueType, valueToWrite)
	
    outputFile.write("--------------------------------------\n\nTesting writing to integer registers with an offset:\n\n")
    for i in range(1, 8):
        valueType = indexToTypeAndValuesTable[i]["typeName"]
        size = getSizeOfType(valueType)
        valueToWrite = indexToTypeAndValuesTable[i]["value"]
        offsetValue = 0
        while offsetValue + size <= 4:
            setRegisterWithOffsetTestCase("r31", valueType, valueToWrite, offsetValue)
            offsetValue = offsetValue + 1
	
    outputFile.write("--------------------------------------\n\nTesting writing to floating point registers with an offset:\n\n")
    for i in range(1, 11):
        valueType = indexToTypeAndValuesTable[i]["typeName"]
        size = getSizeOfType(valueType)
        valueToWrite = indexToTypeAndValuesTable[i]["value"]
        offsetValue = 0
        while offsetValue + size <= 16:
            setRegisterWithOffsetTestCase("f31", valueType, valueToWrite, offsetValue)
            offsetValue = offsetValue + 1


def setRegisterFromByteArrayUnitTests():
    global unsignedByteArray
    global signedByteArray
    outputFile.write("--------------------------------------\n\nTesting writing to/reading from registers as an unsigned byte array:\n\n")
    for arraySize in range(1, 5):
        offsetValue = 0
        while offsetValue + arraySize <= 4:
            byteArrayTestCase("r31", unsignedByteArray, arraySize, offsetValue, True)
            offsetValue = offsetValue + 1

    for arraySize in range(1, 17):
        offsetValue = 0
        while offsetValue + arraySize <= 16:
            byteArrayTestCase("f31", unsignedByteArray, arraySize, offsetValue, True)
            offsetValue = offsetValue + 1
	
    outputFile.write("--------------------------------------\n\nTesting writing to/reading from registers as a signed byte array:\n\n")
    for arraySize in range(1, 5):
        offsetValue = 0
        while offsetValue + arraySize <= 4:
            byteArrayTestCase("r31", signedByteArray, arraySize, offsetValue, False)
            offsetValue = offsetValue + 1
	
    for arraySize in range(1, 17):
        offsetValue = 0
        while offsetValue + arraySize <= 16:
            byteArrayTestCase("f31", signedByteArray, arraySize, offsetValue, False)
            offsetValue = offsetValue + 1


def registersTests():
    global generalRegistersTable
    global floatRegistersTable
    global generalRegistersInitialStateBackup
    global floatRegistersInitialStateBackup
    global pcRegisterInitialStateBackup
    global lrRegisterInitialStateBackup
    global indexToTypeAndValuesTable
    global unsignedByte
    global unsignedShort
    global unsignedInt
    global signedByte
    global signedShort
    global signedInt
    global floatValue
    global signedLongLong
    global doubleValue
    global unsignedLongLong
    global resultsTable
    global testNum

    #Getting the names of the registers, and storing the initial values of the registers
    #so that they can be restored after all of the tests are done...
    for i in range(32):
        generalRegistersTable[i] = "R" + str(i)
        floatRegistersTable[i] = "F" + str(i)
        generalRegistersInitialStateBackup[i] = registers.getRegister(generalRegistersTable[i], "u32", 0)
        floatRegistersInitialStateBackup[i] = registers.getRegister(floatRegistersTable[i], "double", 0)

    pcRegisterInitialStateBackup = registers.getRegister("PC", "u32", 0)
    lrRegisterInitialStateBackup = registers.getRegister("LR", "u32", 0)
    
    indexToTypeAndValuesTable[1] = {"typeName":"u8", "value": unsignedByte}
    indexToTypeAndValuesTable[2] = {"typeName":"u16", "value": unsignedShort}
    indexToTypeAndValuesTable[3] = {"typeName": "u32", "value": unsignedInt}
    indexToTypeAndValuesTable[4] = {"typeName": "s8", "value": signedByte}
    indexToTypeAndValuesTable[5] = {"typeName": "s16", "value": signedShort}
    indexToTypeAndValuesTable[6] = {"typeName": "s32", "value": signedInt}
    indexToTypeAndValuesTable[7] = {"typeName": "float", "value": floatValue}
    indexToTypeAndValuesTable[8] = {"typeName": "s64", "value": signedLongLong}
    indexToTypeAndValuesTable[9] = {"typeName": "double", "value": doubleValue}
    indexToTypeAndValuesTable[10] = {"typeName": "u64", "value": unsignedLongLong}
    
    setRegisterUnitTests()
    setRegisterFromByteArrayUnitTests()
    
    #Restoring the contents of the registers
    for i in range(32):
        registers.setRegister(generalRegistersTable[i], "u32", generalRegistersInitialStateBackup[i], 0)
        registers.setRegister(floatRegistersTable[i], "double", floatRegistersInitialStateBackup[i], 0)
        
    registers.setRegister("PC", "u32", pcRegisterInitialStateBackup, 0)
    registers.setRegister("LR", "u32", lrRegisterInitialStateBackup, 0)

    outputFile.write("Total Tests: " + str(testNum - 1) + "\n\t")
    outputFile.write("Tests Passed: " + str(resultsTable["PASS"]) + "\n\t")
    outputFile.write("Tests Failed: " + str(resultsTable["FAIL"]) + "\n")
    outputFile.flush()
    outputFile.close()
    print("Total Tests: " + str(testNum - 1) + "\n\t")
    print("Tests Passed: " + str(resultsTable["PASS"]) + "\n\t")
    print("Tests Failed: " + str(resultsTable["FAIL"]) + "\n")

registersTests()