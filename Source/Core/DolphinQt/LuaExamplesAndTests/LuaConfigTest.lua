require ("config")

funcRef = -1
testsRun = 0
testsPassed = 0
testsFailed = 0
testsSkipped = 0

allEnumTypes = {

{"CPUCore", {"CACHEDINTERPRETER", "INTERPRETER", "JIT64", "JITARM64"}, {"MAIN", "Core", "CPUCore"}},

{"DPL2Quality", {"HIGH", "HIGHEST", "LOW", "LOWEST"}, {"MAIN", "Core", "DPL2Decoder"}},

{"EXIDeviceType", {"AD16", "AGP", "AMBASEBOARD", "DUMMY", "ETHERNET", "ETHERNETBUILTIN", "ETHERNETTAPSERVER", "ETHERNETXLINK", "GECKO", "MASKROM", "MEMORYCARD", "MEMORYCARDFOLDER", "MICROPHONE", "NONE"}, {"MAIN", "Core", "SlotA"}},

{"SIDeviceType", {"AM_BASEBOARD", "DANCEMAT", "GC_CONTROLLER", "GC_GBA", "GC_GBA_EMULATED", "GC_KEYBOARD", "GC_STEERING", "GC_TARUKONGA", "N64_CONTROLLER", "N64_KEYBOARD", "N64_MIC", "N64_MOUSE", "NONE", "WIIU_ADAPTER"}, {"MAIN", "Core", "SIDevice0"}},

{"HSPDeviceType", {"ARAMEXPANSION", "NONE"}, {"MAIN", "Core", "HSPDevice"}},

{"Region", {"NTSC_J", "NTSC_K", "NTSC_U", "PAL", "UNKNOWN"}, {"MAIN", "Core", "FallbackRegion"}},

{"ShowCursor", {"CONSTANTLY", "NEVER", "ONMOVEMENT"}, {"MAIN", "Interface", "CursorVisibility"}},

{"LogLevel", {"LDEBUG", "LERROR", "LINFO", "LNOTICE", "LWARNING"}, {"LOGGER", "Options", "Verbosity"}},

{"FreeLookControl", {"FPS", "ORBITAL", "SIXAXIS"}, {"FreeLook", "Camera1", "ControlType"}},

{"AspectMode", {"ANALOG", "ANALOGWIDE", "AUTO", "STRETCH"}, {"GFX", "Settings", "AspectRatio"}},

{"ShaderCompilationMode", {"ASYNCHRONOUSSKIPRENDERING", "ASYNCHRONOUSUBERSHADERS", "SYNCHRONOUS", "SYNCHRONOUSUBERSHADERS"}, {"GFX", "Settings", "ShaderCompilationMode"}},

{"TriState", {"AUTO", "OFF", "ON"}, {"GFX",  "Settings",  "ManuallyUploadeBuffers"}},

{"TextureFilteringMode", {"DEFAULT", "LINEAR", "NEAREST"}, {"GFX", "Enhancements", "ForceTextureFiltering"}},

{"StereoMode", {"ANAGLYPH", "OFF", "PASSIVE", "QUADBUFFER", "SBS", "TAB"}, {"GFX", "Stereoscopy", "StereoMode"}},

{"WiimoteSource", {"EMULATED", "NONE", "REAL"}, {"WiiPad", "Wiimote1", "Source"}}

}

function testGetLayerNames()
	expectedReturnValue = "Base, CommandLine, GlobalGame, LocalGame, Movie, Netplay, CurrentRun"
	io.write("Test " .. tostring(testsRun + 1) .. ":\n")
	io.write("Testing calling the getLayerNames_mostGlobalFirst() function. Should have a return value of: " .. expectedReturnValue .. "\n")
	actualReturnValue = config:getLayerNames_mostGlobalFirst()
	if actualReturnValue == expectedReturnValue then
		io.write("Actual value equaled the expected value...  Test passed!\n")
		testsPassed = testsPassed + 1
	else
		io.write("Actual value returned was " .. actualReturnValue .. "... Test failed!\n")
		testsFailed = testsFailed + 1
	end
	testsRun = testsRun + 1
	io.write("\n\n")
end

function testGetListOfSystems()
	expectedReturnValue = "Main, Sysconf, GCPad, WiiPad, GCKeyboard, GFX, Logger, Debugger, DualShockUDPClient, FreeLook, Session, GameSettingsOnly, Achievements"
	io.write("Test " .. tostring(testsRun + 1) .. ":\n")
	io.write("Testing calling the getListOfSystems() function. Should have a return value of: " .. expectedReturnValue .. "\n")
	actualReturnValue = config:getListOfSystems()
	if actualReturnValue == expectedReturnValue then
		io.write("Actual value equaled the expected value... Test passed!\n")
		testsPassed = testsPassed + 1
	else
		io.write("Actual value returned was " .. actualReturnValue .. "... Test failed!\n")
		testsFailed = testsFailed + 1
	end
	testsRun = testsRun + 1
	io.write("\n\n")
end

function testGetConfigEnumTypes()
	expectedReturnValue = "CPUCore, DPL2Quality, EXIDeviceType, SIDeviceType, HSPDeviceType, Region, ShowCursor, LogLevel, FreeLookControl, AspectMode, ShaderCompilationMode, TriState, TextureFilteringMode, StereoMode, WiimoteSource"
	io.write("Test " .. tostring(testsRun + 1) .. ":\n")
	io.write("Testing calling the getConfigEnumTypes() function. Should have a return value of: " .. expectedReturnValue .. "\n")
	actualReturnValue = config:getConfigEnumTypes()
	if actualReturnValue == expectedReturnValue then
		io.write("Actual value equaled the expected value... Test passed!\n")
		testsPassed = testsPassed + 1
	else
		io.write("Actual value returned was " .. actualReturnValue .. "...Test failed!\n")
		testsFailed = testsFailed + 1
	end
	testsRun = testsRun + 1
	io.write("\n\n")
end

function testGetListOfValidValuesForEnumType()
	for outerKey, outerValue in ipairs(allEnumTypes) do
		io.write("Test " .. tostring(testsRun + 1) .. ":\n")
		expectedValue = ""
		for innerKey, innerValue in ipairs(outerValue[2]) do
			if expectedValue ~= "" then
				expectedValue = expectedValue .. ", "
			end
			expectedValue = expectedValue .. innerValue
		end
		
		io.write("Testing " .. tostring(outerValue[1]) .. " enum type range. Expected list of values returned is: " .. tostring(expectedValue) .. "\n")
		actualValue = config:getListOfValidValuesForEnumType(outerValue[1])
		if actualValue == expectedValue then
			io.write("Actual value equaled the expected value... Test passed!\n")
			testsPassed = testsPassed + 1
		else
			io.write("Actual value returned was " .. actualValue .. "...Test failed!\n")
			testsFailed = testsFailed + 1
		end
		testsRun = testsRun + 1
	end
	io.write("\n\n")
end


function testGetAndSetForEachLayer()
	list_of_layers = { "Base", "CommandLine", "GlobalGame", "LocalGame", "Movie", "Netplay", "CurrentRun" }
	io.write("Testing writing to each layer...\n")
	for key, value in ipairs(list_of_layers) do
		io.write("Test " .. tostring(testsRun + 1) .. ":\n")
		io.write("Testing writing to layer " .. value .. " for System - Main, Section - Interface, SettingName - debugModeEnabled\n")
		if not config:doesLayerExist(value) then
			io.write("Layer " .. tostring(value) .. " did not exist - skipping test...\n")
			testsSkipped = testsSkipped + 1
		else
			originalVal = config:getConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled")
			config:setConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled", not originalVal)
			if config:getConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled") == not originalVal then
				io.write("Test passed!\n")
				testsPassed = testsPassed + 1
				config:setConfigSettingForLayer("bool", value, "MAIN", "Interface", "debugModeEnabled", originalVal)
			else
				io.write("Test failed!\n")
				testsFailed = testsFailed + 1
			end
		end
		testsRun = testsRun + 1
		io.write("\n\n")
	end
end

function testPrecedence()
	localValue = 54
	baseValue = 35
	io.write("Test " .. tostring(testsRun + 1) .. ":\n")
	io.write("Testing writing integer value to CurrentRun and then to Base, and making sure that value from CurrentRun is the one that was used...\n")
	config:setConfigSettingForLayer("s32", "CurrentRun", "MAIN", "Interface", "testingDebugValue", localValue)
	config:setConfigSettingForLayer("s32", "Base", "MAIN", "Interface", "testingDebugValue", baseValue)
	actualValue = config:getConfigSetting("s32", "MAIN", "Interface", "testingDebugValue")
	if actualValue == localValue then
		io.write("Test passed!\n")
		testsPassed = testsPassed + 1
	else
		io.write("Test failed!\n")
		testsFailed = testsFailed + 1
	end
	testsRun = testsRun + 1
	io.write("\n\n")
	config:deleteConfigSettingFromLayer("s32", "CurrentRun", "MAIN", "Interface", "testingDebugValue")
	config:deleteConfigSettingFromLayer("s32", "Base", "MAIN", "Interface", "testingDebugValue")
end


function performInnerTestCaseForGetAndSet(exampleValue)
		layerName = "CurrentRun"
		systemName = exampleValue[1]
		sectionName = exampleValue[2]
		settingName = exampleValue[3]
		settingType = exampleValue[4]
		valueToBeWritten = exampleValue[5]
		
		io.write("Test " .. tostring(testsRun + 1) .. ":\n")
		io.write("Testing writing to system " .. systemName ..  " in section " .. sectionName .. " with a variable " .. settingName .. " of type " .. settingType .. " and a value of " .. tostring(valueToBeWritten) .. ", and making sure it has the same value when read back out at the same layer...\n")
		originalValue = config:getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName)
		config:setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, valueToBeWritten)
		valueReadOutAtEndLayer = config:getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName)
		if valueReadOutAtEndLayer == valueToBeWritten then
			io.write("Value written of " .. tostring(valueToBeWritten) .. " and value read of " ..  tostring(valueReadOutAtEndLayer) .. " were equal. Test passed!\n\n")
			testsPassed = testsPassed + 1
		else
			io.write("Values were NOT equal. Value written was " .. tostring(valueToBeWritten) .. ", and value read back out was " .. tostring(valueReadOutAtEndLayer) .. ". Test failed!\n\n")
			testsFailed = testsFailed + 1
		end
		testsRun = testsRun + 1
		io.write("Test " .. tostring(testsRun + 1) .. ":\n")
		io.write("Testing writing to system " .. systemName ..  " in section " .. sectionName .. " with a variable " .. settingName .. " of type " .. settingType .. " and a value of " .. tostring(valueToBeWritten) .. ", and making sure it has the same value when the absolute value of the setting is read out (when calling getConfigSetting() without specifying a layer)...\n")
		valueReadOutTotal = config:getConfigSetting(settingType, systemName, sectionName, settingName)
		if valueReadOutTotal == valueToBeWritten then
			io.write("Values written of " .. tostring(valueToBeWritten) .. " and value read of " .. tostring(valueReadOutTotal) .. " were equal. Test passed!\n\n")
			testsPassed = testsPassed + 1
		else
			io.write("Values were NOT equal. Value written was " .. tostring(valueToBeWritten) .. ", and absolute value read back out was " .. tostring(valueReadOutTotal) .. ". Test failed!\n\n")
			testsFailed = testsFailed + 1
		end
		testsRun = testsRun + 1
		if originalValue == nil then
			config:deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
		else
			config:setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, originalValue)
		end
end


function testThatAllSystemsWork()
	io.write("Testing that all systems can be written to...\n\n")
	test_cases = {
		{"MAIN", "Interface", "debugModeEnabled", "boolean", true},
		{"SYSCONF", "IPL", "SADR", "u32", 2},
		{"GCPad", "Buttons", "A", "boolean", true},
		{"WiiPad", "Buttons", "B", "boolean", false},
		{"GCKeyboard", "Keys", "Enter", "boolean", true},
		{"GFX", "Settings", "ShaderCompilerThreads", "s32", 5},
		{"Logger", "Options", "WriteToConsole", "boolean", false},
		{"Debugger", "Settings", "enabledTracing", "boolean", true},
		{"DualShockUDPClient", "Server", "IPAddress", "string", "customIP"},
		{"FreeLook", "General", "Enabled", "boolean", true},
		{"Session", "Core", "LoadIPLDump", "boolean", false},
		{"GameSettingsOnly", "General", "useBackgroundInputs", "boolean", true},
		{"Achievements", "Achievements", "Enabled", "boolean", true}
	}

	layerName = "Base"
	for key, value in ipairs(test_cases) do
		performInnerTestCaseForGetAndSet(value)
	end
	io.write("\n\n")
end

function testGetAndSetForAllRegularTypes()
	io.write("Testing that all non-enum types can have their get/set methods called...\n\n")
	test_cases = {
		{"MAIN", "Interface", "debugModeEnabled", "boolean", true},
		{"MAIN", "Core", "TimingVariance", "s32", 312},
		{"MAIN", "Core", "MEM1Size", "u32", 564},
		{"MAIN", "Core", "EmulationSpeed", "float", 54.25},
		{"MAIN", "Core", "GFXBackend", "string", "newBackendName"}
	}
	for key, value in ipairs(test_cases) do
		performInnerTestCaseForGetAndSet(value)
	end
	io.write("\n\n")
end

function testGetAndSetForAllEnumTypes()
	io.write("Testing that all enum types can have their get/set methods called, and that all enum value types can be written to a config setting of each enum type...\n\n")
	 for key, value in ipairs(allEnumTypes) do
		for key, valid_type_value in ipairs(value[2]) do
			io.write("Testing enum type of " .. value[1] .. "\n")
			new_test_case = {}
			new_test_case[1] = value[3][1]
			new_test_case[2] = value[3][2]
			new_test_case[3] = value[3][3]
			new_test_case[4] = value[1]
			new_test_case[5] = valid_type_value
			performInnerTestCaseForGetAndSet(new_test_case)
		end
	 end
	 io.write("\n\n")
end

function performInnerTestCaseForDelete(exampleValue)
		layerName = "CurrentRun"
		systemName = exampleValue[1]
		sectionName = exampleValue[2]
		settingName = exampleValue[3]
		settingType = exampleValue[4]
		valueToBeWritten = exampleValue[5]
		if config:getConfigSetting(settingType, systemName, sectionName, settingName) ~= nil then
			print("Value existed before start?!?!?")
			print(tostring(config:getConfigSetting(settingType, systemName, sectionName, settingName)))
		end
		io.write("Test " .. tostring(testsRun + 1) .. ":\n")
		testsRun = testsRun + 1
		io.write("Testing creating a new setting and deleting it.")
		io.write("\tTest case has system of " .. systemName .. ", section name of " .. sectionName .. ", settingName of " .. settingName .. ", settingType of " .. settingType .. ", and value to be written of " .. tostring(valueToBeWritten) .. "\n")
		config:setConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName, valueToBeWritten)
		if config:getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName) == nil or config:getConfigSetting(settingType, systemName, sectionName, settingName)  == nil then
			io.write("Failed to create setting... Test failed!\n\n")
			testsFailed = testsFailed + 1
			return
		end
		retVal = config:deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
		if not retVal then
			io.write("Delete function did not return true - indicating that delete attempt failed... Test failed!\n\n")
			testsFailed = testsFailed + 1
			return
		end
		--if config:getConfigSettingForLayer(settingType, layerName, systemName, sectionName, settingName) ~= nil then or 
		if config:getConfigSetting(settingType, systemName, sectionName, settingName)  ~= nil then
			io.write("Error: Setting still exists in layer after calling delete function! Test failed!\n\n")
			print(tostring(config:getConfigSetting(settingType, systemName, sectionName, settingName)))
			testsFailed = testsFailed + 1
			return
		end
		retVal = config:deleteConfigSettingFromLayer(settingType, layerName, systemName, sectionName, settingName)
		if retVal then
			io.write("System allowed for a double-delete... Test failed!\n\n")
			testsFailed = testsFailed + 1
			return
		end
		io.write("Delete succeeded! Test passed!\n\n")
		testsPassed = testsPassed + 1
end

function testDeleteForAllRegularTypes()
	io.write("Testing that all non-enum types can have delete called on them...\n\n")
		test_cases = {
		{"MAIN", "Interface", "NON_EXISTANT_SETTING12", "boolean", true},
		{"MAIN", "Core", "NON_EXISTANT_SETTING12", "s32", 312},
		{"MAIN", "Core", "NON_EXISTANT_SETTING12", "u32", 564},
		{"MAIN", "Core", "NON_EXISTANT_SETTING12", "float", 54.25},
		{"MAIN", "Core", "NON_EXISTANT_SETTING12", "string", "newBackendName"}
	}
	for key, value in ipairs(test_cases) do
		performInnerTestCaseForDelete(value)
	end
end

function mainTestFunc()
	testGetLayerNames()
	testGetListOfSystems()
	testGetConfigEnumTypes()
	testGetListOfValidValuesForEnumType()
	testGetAndSetForEachLayer()
	testPrecedence()
	testThatAllSystemsWork()
	testGetAndSetForAllRegularTypes()
	testGetAndSetForAllEnumTypes()
	testDeleteForAllRegularTypes()
	OnFrameStart:unregister(funcRef)
	config:saveSettings()
	io.write("Results:\n\n\tTests Passed: " .. tostring(testsPassed) .. "\n\tTests Failed: " .. tostring(testsFailed) .. "\n\tTests Skipped: " .. tostring(testsSkipped) .. "\nTotal Tests: " .. tostring(testsRun))
	print("Results:\n\n\tTests Passed: " .. tostring(testsPassed) .. "\n\tTests Failed: " .. tostring(testsFailed) .. "\n\tTests Skipped: " .. tostring(testsSkipped) .. "\nTotal Tests: " .. tostring(testsRun))
	io.flush()
	io.close()
end

file = io.open("LuaExamplesAndTests/TestResults/LuaConfigApiTestsResults.txt", "w")
io.output(file)
funcRef = OnFrameStart:register(mainTestFunc)