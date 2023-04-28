import config

outputFile = open("PythonExamplesAndTests/TestResults/PythonConfigTestsResults.txt", "w")

funcRef = -1
testsRun = 0
testsPassed = 0
testsFailed = 0
testsSkipped = 0

sectionEnding = "----------------------------------------------------------------------------------------------------\n"
allEnumTypes = [
    ["CPUCore", ["CACHEDINTERPRETER", "INTERPRETER", "JIT64", "JITARM64"], ["MAIN", "Core", "CPUCore"]],
    
    ["DPL2Quality", ["HIGH", "HIGHEST", "LOW", "LOWEST"], ["MAIN", "Core", "DPL2Decoder"]],
    
    ["EXIDeviceType", ["AD16", "AGP", "AMBASEBOARD", "DUMMY", "ETHERNET", "ETHERNETBUILTIN", "ETHERNETTAPSERVER", "ETHERNETXLINK", "GECKO", "MASKROM", "MEMORYCARD", "MEMORYCARDFOLDER", "MICROPHONE", "NONE"], ["MAIN", "Core", "SlotA"]],
    
    ["SIDeviceType", ["AM_BASEBOARD", "DANCEMAT", "GC_CONTROLLER", "GC_GBA", "GC_GBA_EMULATED", "GC_KEYBOARD", "GC_STEERING", "GC_TARUKONGA", "N64_CONTROLLER", "N64_KEYBOARD", "N64_MIC", "N64_MOUSE", "NONE", "WIIU_ADAPTER"], ["MAIN", "Core", "SIDevice0"]],
    
    ["HSPDeviceType", ["ARAMEXPANSION", "NONE"], ["MAIN", "Core", "HSPDevice"]],
    
    ["Region", ["NTSC_J", "NTSC_K", "NTSC_U", "PAL", "UNKNOWN"], ["MAIN", "Core", "FallbackRegion"]],
    
    ["ShowCursor", ["CONSTANTLY", "NEVER", "ONMOVEMENT"], ["MAIN", "Interface", "CursorVisibility"]],
    
    ["LogLevel", ["LDEBUG", "LERROR", "LINFO", "LNOTICE", "LWARNING"], ["LOGGER", "Options", "Verbosity"]],
    
    ["FreeLookControl", ["FPS", "ORBITAL", "SIXAXIS"], ["FreeLook", "Camera1", "ControlType"]],
    
    ["AspectMode", ["ANALOG", "ANALOGWIDE", "AUTO", "STRETCH"], ["GFX", "Settings", "AspectRatio"]],
    
    ["ShaderCompilationMode", ["ASYNCHRONOUSSKIPRENDERING", "ASYNCHRONOUSUBERSHADERS", "SYNCHRONOUS", "SYNCHRONOUSUBERSHADERS"], ["GFX", "Settings", "ShaderCompilationMode"]],
    
    ["TriState", ["AUTO", "OFF", "ON"], ["GFX",  "Settings",  "ManuallyUploadeBuffers"]],
    
    ["TextureFilteringMode", ["DEFAULT", "LINEAR", "NEAREST"], ["GFX", "Enhancements", "ForceTextureFiltering"]],
    
    ["StereoMode", ["ANAGLYPH", "OFF", "PASSIVE", "QUADBUFFER", "SBS", "TAB"], ["GFX", "Stereoscopy", "StereoMode"]],
    
    ["WiimoteSource", ["EMULATED", "NONE", "REAL"], ["WiiPad", "Wiimote1", "Source"]]
]

def testGetLayerNames():
    global testsRun
    global testsPassed
    global testsFailed
    expectedReturnValue = "Base, CommandLine, GlobalGame, LocalGame, Movie, Netplay, CurrentRun"
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    actualReturnValue = config.getLayerNames_mostGlobalFirst()
    if actualReturnValue == expectedReturnValue:
        outputFile.write("\tActual value equaled the expected return value of: '" + expectedReturnValue + "'...\nPASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("\tActual value returned was " + str(actualReturnValue) + ", which did NOT equal the expected return value of '" + expectedReturnValue + "'...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    outputFile.write("\n")


def testGetListOfSystems():
    global testsRun
    global testsPassed
    global testsFailed
    expectedReturnValue = "Main, Sysconf, GCPad, WiiPad, GCKeyboard, GFX, Logger, Debugger, DualShockUDPClient, FreeLook, Session, GameSettingsOnly, Achievements"
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    actualReturnValue = config.getListOfSystems()
    if actualReturnValue == expectedReturnValue:
        outputFile.write("\tActual value equaled the expected return value of: '" + expectedReturnValue + "'...\nPASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("\tActual value returned was " + str(actualReturnValue) + ", which did NOT equal the expected return value of '" + expectedReturnValue + "'...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    outputFile.write("\n")

def testGetConfigEnumTypes():
    global testsRun
    global testsPassed
    global testsFailed
    expectedReturnValue = "CPUCore, DPL2Quality, EXIDeviceType, SIDeviceType, HSPDeviceType, Region, ShowCursor, LogLevel, FreeLookControl, AspectMode, ShaderCompilationMode, TriState, TextureFilteringMode, StereoMode, WiimoteSource"
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    actualReturnValue = config.getConfigEnumTypes()
    if actualReturnValue == expectedReturnValue:
        outputFile.write("\tActual value equaled the expected return value of: '" + expectedReturnValue + "'...\nPASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("\tActual value returned was " + str(actualReturnValue) + ", which did NOT equal the expected return value of '" + expectedReturnValue + "'...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    outputFile.write("\n")

def testGetListOfValidValuesForEnumType():
    global testsRun
    global testsPassed
    global testsFailed
    for outerValue in allEnumTypes:
        outputFile.write("Test " + str(testsRun + 1) + ":\n")
        expectedReturnValue = ""
        for innerValue in outerValue[1]:
            if expectedReturnValue != "":
                expectedReturnValue = expectedReturnValue + ", "
            expectedReturnValue = expectedReturnValue + innerValue
		
        outputFile.write("\tTesting " + str(outerValue[0]) + " enum type range...\n")
        actualValue = config.getListOfValidValuesForEnumType(outerValue[0])
        if actualValue == expectedReturnValue:
            outputFile.write("\tActual value equaled the expected return value of: '" + expectedReturnValue + "'...\nPASS!\n\n")
            testsPassed = testsPassed + 1
        else:
            outputFile.write("\tActual value returned was " + str(actualValue) + ", which did NOT equal the expected return value of '" + expectedReturnValue + "'...\nFAILURE!\n\n")
            testsFailed = testsFailed + 1
        testsRun = testsRun + 1
    outputFile.write("\n")

def testGetAndSetForEachLayer():
    global testsRun
    global testsPassed
    global testsFailed
    global testsSkipped
    list_of_layers = [ "Base", "CommandLine", "GlobalGame", "LocalGame", "Movie", "Netplay", "CurrentRun" ]
    for value in list_of_layers:
        outputFile.write("Test " + str(testsRun + 1) + ":\n")
        outputFile.write("\tTesting writing to layer " + value + " for System - Main, Section - Interface, SettingName - debugModeEnabled\n")
        if not config.doesLayerExist(value):
            outputFile.write("\tLayer " + str(value) + " did not exist\nSKIP\n\n")
            testsSkipped = testsSkipped + 1
        else:
            originalVal = config.getConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled")
            valueToSet = originalVal
            if originalVal is None:
                valueToSet = True
            config.setConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled", not valueToSet)
            if config.getConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled") != valueToSet:
                outputFile.write("PASS!\n\n")
                testsPassed = testsPassed + 1
                if originalVal is not None:
                    config.setConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled", originalVal)
                else:
                    config.deleteConfigSettingFromLayer("bool", value, "MAIN", "Interface", "debugModeEnabled")
            else:
                outputFile.write("FAILURE!\n\n")
                testsFailed = testsFailed + 1
        testsRun = testsRun + 1
    outputFile.write("\n")

def testPrecedence():
    global testsRun
    global testsPassed
    global testsFailed
    localValue = 54
    baseValue = 35
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    outputFile.write("\tTesting writing integer value to CurrentRun and to Base, and making sure that value from CurrentRun is the one that was used...\n")
    config.setConfigSettingForLayer("s32", "CurrentRun", "MAIN", "Interface", "testingDebugValue", localValue)
    config.setConfigSettingForLayer("s32", "Base", "MAIN", "Interface", "testingDebugValue", baseValue)
    actualValue = config.getConfigSetting("s32", "MAIN", "Interface", "testingDebugValue")
    if actualValue == localValue:
        outputFile.write("PASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("FAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    outputFile.write("\n")
    config.deleteConfigSettingFromLayer("s32", "CurrentRun", "MAIN", "Interface", "testingDebugValue")
    config.deleteConfigSettingFromLayer("s32", "Base", "MAIN", "Interface", "testingDebugValue")

def performInnerTestCaseForGetAndSet(exampleValue):
    global testsRun
    global testsPassed
    global testsFailed
    layerName = "CurrentRun"
    systemName = exampleValue[0]
    sectionName = exampleValue[1]
    settingName = exampleValue[2]
    settingType = exampleValue[3]
    valueToBeWritten = exampleValue[4]
	
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    outputFile.write("\tTesting writing to system " + systemName +  " in section " + sectionName + " with a variable " + settingName + " of type " + settingType + " and a value of " + str(valueToBeWritten) + ", and making sure it has the same value when read back out at the same layer...\n")
    originalValue = config.getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName)
    config.setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, valueToBeWritten)
    valueReadOutAtEndLayer = config.getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName)
    if valueReadOutAtEndLayer == valueToBeWritten:
        outputFile.write("\tValue written of " + str(valueToBeWritten) + " and value read of " +  str(valueReadOutAtEndLayer) + " were equal.\nPASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("\tValues were NOT equal. Value written was " + str(valueToBeWritten) + ", and value read back out was " + str(valueReadOutAtEndLayer) + ".\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    outputFile.write("\tTesting writing to system " + systemName +  " in section " + sectionName + " with a variable " + settingName + " of type " + settingType + " and a value of " + str(valueToBeWritten) + ", and making sure it has the same value when the absolute value of the setting is read out (when calling getConfigSetting() without specifying a layer)...\n")
    valueReadOutTotal = config.getConfigSetting(settingType, systemName, sectionName, settingName)
    if valueReadOutTotal == valueToBeWritten:
        outputFile.write("\tValue written of " + str(valueToBeWritten) + " and value read of " + str(valueReadOutTotal) + " were equal.\nPASS!\n\n")
        testsPassed = testsPassed + 1
    else:
        outputFile.write("\tValues were NOT equal. Value written was " + str(valueToBeWritten) + ", and absolute value read back out was " + str(valueReadOutTotal) + ".\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
    testsRun = testsRun + 1
    if originalValue is None:
        config.deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
    else:
        config.setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, originalValue)

def testThatAllSystemsWork():
	test_cases = [
		["MAIN", "Interface", "debugModeEnabled", "boolean", True],
		["SYSCONF", "IPL", "SADR", "u32", 2],
		["GCPad", "Buttons", "A", "boolean", True],
		["WiiPad", "Buttons", "B", "boolean", False],
		["GCKeyboard", "Keys", "Enter", "boolean", True],
		["GFX", "Settings", "ShaderCompilerThreads", "s32", 5],
		["Logger", "Options", "WriteToConsole", "boolean", False],
		["Debugger", "Settings", "enabledTracing", "boolean", True],
		["DualShockUDPClient", "Server", "IPAddress", "string", "customIP"],
		["FreeLook", "General", "Enabled", "boolean", True],
		["Session", "Core", "LoadIPLDump", "boolean", False],
		["GameSettingsOnly", "General", "useBackgroundInputs", "boolean", True],
		["Achievements", "Achievements", "Enabled", "boolean", True]
	]
	for value in test_cases:
		performInnerTestCaseForGetAndSet(value)
	outputFile.write("\n")

def testGetAndSetForAllRegularTypes():
	test_cases = [
		["MAIN", "Interface", "debugModeEnabled", "boolean", True],
		["MAIN", "Core", "TimingVariance", "s32", 312],
		["MAIN", "Core", "MEM1Size", "u32", 564],
		["MAIN", "Core", "EmulationSpeed", "float", 54.25],
		["MAIN", "Core", "GFXBackend", "string", "newBackendName"]
	]
	for value in test_cases:
		performInnerTestCaseForGetAndSet(value)
	outputFile.write("\n")

def testGetAndSetForAllEnumTypes():
    for value in allEnumTypes:
        outputFile.write("Testing enum type of " + value[0] + "\n")
        for valid_type_value in value[1]:
            new_test_case = {}
            new_test_case[0] = value[2][0]
            new_test_case[1] = value[2][1]
            new_test_case[2] = value[2][2]
            new_test_case[3] = value[0]
            new_test_case[4] = valid_type_value
            performInnerTestCaseForGetAndSet(new_test_case)
    outputFile.write("\n")

def performInnerTestCaseForDelete(exampleValue):
    global testsRun
    global testsPassed
    global testsFailed
    layerName = "CurrentRun"
    systemName = exampleValue[0]
    sectionName = exampleValue[1]
    settingName = exampleValue[2]
    settingType = exampleValue[3]
    valueToBeWritten = exampleValue[4]
    if config.getConfigSetting(settingType, systemName, sectionName, settingName) is not None:
        print("Value existed before start in delete function?!?!?")
        print(str(config.getConfigSetting(settingType, systemName, sectionName, settingName)))
        outputFile.write("Value existed before start in delete function?!?!?\n")
    outputFile.write("Test " + str(testsRun + 1) + ":\n")
    testsRun = testsRun + 1
    outputFile.write("\tTesting creating a new setting and deleting it.")
    outputFile.write("\tTest case has system of " + systemName + ", section name of " + sectionName + ", settingName of " + settingName + ", settingType of " + settingType + ", and value to be written of " + str(valueToBeWritten) + "\n")
    config.setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, valueToBeWritten)
    if config.getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName) is None or config.getConfigSetting(settingType, systemName, sectionName, settingName) is None:
        outputFile.write("\tFailed to create setting...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
        return
    retVal = config.deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
    if not retVal:
        outputFile.write("\tDelete function did not return True - indicating that delete attempt failed...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
        return
    if config.getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName) is not None or config.getConfigSetting(settingType, systemName, sectionName, settingName) is not None:
        outputFile.write("\tError: Setting still exists in layer after calling delete function!\nFAILURE\n\n")
        testsFailed = testsFailed + 1
        return
    retVal = config.deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
    if retVal:
        outputFile.write("\tSystem allowed for a double-delete...\nFAILURE!\n\n")
        testsFailed = testsFailed + 1
        return
    outputFile.write("\tDelete succeeded.\nPASS!\n\n")
    testsPassed = testsPassed + 1

def testDeleteForAllRegularTypes():
    test_cases = [
        ["MAIN", "Interface", "NON_EXISTANT_SETTING12", "boolean", True],
		["MAIN", "Core", "NON_EXISTANT_SETTING12", "s32", 312],
		["MAIN", "Core", "NON_EXISTANT_SETTING12", "u32", 564],
		["MAIN", "Core", "NON_EXISTANT_SETTING12", "float", 54.25],
		["MAIN", "Core", "NON_EXISTANT_SETTING12", "string", "newBackendName"]
	]
    for value in test_cases:
        performInnerTestCaseForDelete(value)
    outputFile.write("\n")

def testDeleteForAllEnumTypes():
    for value in allEnumTypes:
        outputFile.write("Testing enum type of " + value[0] + "\n")
        for valid_type_value in value[1]:
            new_test_case = {}
            new_test_case[0] = "MAIN"
            new_test_case[1] = "Core"
            new_test_case[2] = "NON_EXISTANT_SETTING45"
            new_test_case[3] = value[0]
            new_test_case[4] = valid_type_value
            performInnerTestCaseForDelete(new_test_case)
    outputFile.write("\n")

def mainTestFunc():
	outputFile.write("Testing calling the getLayerNames_mostGlobalFirst() function...\n\n")
	testGetLayerNames()	
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing calling the getListOfSystems() function...\n\n")
	testGetListOfSystems()	
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing calling the getConfigEnumTypes() function...\n\n")
	testGetConfigEnumTypes()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing getting list of valid values for each enum type()...\n\n")
	testGetListOfValidValuesForEnumType()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing writing to each layer...\n\n")
	testGetAndSetForEachLayer()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that precedence of layers works correctly...\n\n")
	testPrecedence()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that all systems can be written to...\n\n")
	testThatAllSystemsWork()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that all non-enum types can have their get/set methods called...\n\n")
	testGetAndSetForAllRegularTypes()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that all enum types can have their get/set methods called, and that all enum value types can be written to a config setting of each enum type...\n\n")
	testGetAndSetForAllEnumTypes()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that all non-enum types can have delete called on them...\n\n")
	testDeleteForAllRegularTypes()
	outputFile.write(sectionEnding)
	
	outputFile.write("Testing that all enum types can have delete called on them...\n\n")
	testDeleteForAllEnumTypes()
	outputFile.write(sectionEnding)
	
	OnFrameStart.unregister(funcRef)
	config.saveSettings()
	outputFile.write("\nTotal Tests: " + str(testsRun) + "\n\tTests Passed: " + str(testsPassed) + "\n\tTests Failed: " + str(testsFailed) + "\n\tTests Skipped: " + str(testsSkipped))
	print("Total Tests: " + str(testsRun) + "\n\tTests Passed: " + str(testsPassed) + "\n\tTests Failed: " + str(testsFailed) + "\n\tTests Skipped: " + str(testsSkipped))
	outputFile.flush()
	outputFile.close()

funcRef = OnFrameStart.register(mainTestFunc)