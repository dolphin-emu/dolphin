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

{"ShaderCompilationMode", {"ASYNCHRONOUSSKIPRENDERING, ASYNCHRONOUSUBERSHADERS, SYNCHRONOUS, SYNCHRONOUSUBERSHADERS"}, {"GFX", "Settings", "ShaderCompilationMode"}},

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
			originalVal = config:getConfigSettingForLayer("bool", value, "MAIN", "INTERFACE", "debugModeEnabled")
			config:setConfigSettingForLayer("bool", value, "MAIN", "INTERFACE", "debugModeEnabled", not originalVal)
			if config:getConfigSettingForLayer("bool", value, "MAIN", "INTERFACE", "debugModeEnabled") == not originalVal then
				io.write("Test passed!\n")
				testsPassed = testsPassed + 1
				config:setConfigSettingForLayer("bool", value, "MAIN", "INTERFACE", "debugModeEnabled", originalVal)
			else
				io.write("Test failed!\n")
				testsFailed = testsFailed + 1
			end
		end
		testsRun = testsRun + 1
		io.write("\n\n")
	end
end

function mainTestFunc()
	testGetLayerNames()
	testGetListOfSystems()
	testGetConfigEnumTypes()
	testGetListOfValidValuesForEnumType()
	testGetAndSetForEachLayer()
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