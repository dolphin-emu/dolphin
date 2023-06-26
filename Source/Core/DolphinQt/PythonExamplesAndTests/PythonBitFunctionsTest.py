import bit

testNum = 1
resultsTable = {}
resultsTable["PASS"] = 0
resultsTable["FAIL"] = 0
outputFile = open("PythonExamplesAndTests/TestResults/PythonBitTestsResults.txt", "w")


def doTest2Args(actualValue, expectedValue, firstInteger, secondInteger, functionName, operatorValue):
    global testNum
    global outputFile
    global resultsTable
    outputFile.write("Test " + str(testNum) + ":\n")
    outputFile.flush()
    testNum = testNum + 1
    actualExpression = "bit." + functionName + "(" + str(firstInteger) + ", " + str(secondInteger) + ")"
    baseExpression =  str(firstInteger) + " " + operatorValue + " " + str(secondInteger)
    outputFile.write("\tTesting if " + actualExpression + " == " + baseExpression + ":\n")
    if actualValue == expectedValue:
        outputFile.write("\t" + str(actualValue) + " == " + str(expectedValue) + "\nPASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("\t" + str(actualValue) + " != " + str(expectedValue) + "\nFAILURE! Actual value did not equal expected value!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1


def doTest1Arg(actualValue, expectedValue, numberValue, functionName, operatorValue):
    global testNum
    global outputFile
    global resultsTable
    outputFile.write("Test " + str(testNum) + ":\n")
    testNum = testNum + 1
    actualExpression = "bit." + str(functionName) + "(" + str(numberValue) + ")"
    baseExpression = operatorValue + str(numberValue)
    outputFile.write("\tTesting if " + actualExpression + " == " + baseExpression + ":\n")
    if (actualValue == expectedValue):
        outputFile.write("\t" + str(actualValue) + " == " + str(expectedValue) + "\nPASS!\n\n")
        resultsTable["PASS"] = resultsTable["PASS"] + 1
    else:
        outputFile.write("\t" + str(actualValue) + " != " + str(expectedValue) + "\nFAILURE! Actual value did not equal expected value!\n\n")
        resultsTable["FAIL"] = resultsTable["FAIL"] + 1


def PythonBitTests():
    global testNum
    global outputFile
    global resultsTable
    firstMainInteger = 113
    secondMainInteger = 546

    firstBitShiftArg = 13
    secondBitShiftArg = 2
    doTest2Args(bit.bitwise_and(firstMainInteger, secondMainInteger), firstMainInteger & secondMainInteger, firstMainInteger, secondMainInteger, "bitwise_and", "&")
    doTest2Args(bit.bitwise_or(firstMainInteger, secondMainInteger), firstMainInteger | secondMainInteger, firstMainInteger, secondMainInteger, "bitwise_or", "|")
    doTest2Args(bit.bitwise_xor(firstMainInteger, secondMainInteger), firstMainInteger ^ secondMainInteger, firstMainInteger, secondMainInteger, "bitwise_xor", "^")
    doTest2Args(bit.logical_and(True, False), True and False, True, False, "logical_and", "&&")
    doTest2Args(bit.logical_or(True, False), True or False, True, False, "logical_or", "||")
    doTest2Args(bit.logical_xor(True, True), False, True, False, "logical_xor", "XOR")
    doTest2Args(bit.bit_shift_left(firstBitShiftArg, secondBitShiftArg), firstBitShiftArg << secondBitShiftArg, firstBitShiftArg, secondBitShiftArg, "bit_shift_left", "<<")
    doTest2Args(bit.bit_shift_right(firstBitShiftArg, secondBitShiftArg), firstBitShiftArg >> secondBitShiftArg, firstBitShiftArg, secondBitShiftArg, "bit_shift_right", ">>")	
	

    doTest1Arg(bit.bitwise_not(firstMainInteger), ~firstMainInteger, firstMainInteger, "bitwise_not", "~")
    doTest1Arg(bit.logical_not(firstMainInteger), not firstMainInteger, firstMainInteger, "logical_not", "!")
    outputFile.write("Total Tests: " + str(testNum - 1) + "\n")
    outputFile.write("\tTests Passed: " + str(resultsTable["PASS"]) + "\n")
    outputFile.write("\tTests Failed: " + str(resultsTable["FAIL"]) + "\n")
    print("Total Tests: " + str(testNum - 1) + "\n")
    print("\tTests Passed: " + str(resultsTable["PASS"]) + "\n")
    print("\tTests Failed: " + str(resultsTable["FAIL"]) + "\n")



PythonBitTests()
outputFile.flush()
outputFile.close()
