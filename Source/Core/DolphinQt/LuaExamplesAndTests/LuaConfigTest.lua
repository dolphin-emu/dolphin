require ("config")

funcRef = -1
testsRun = 0
testsPassed = 0
testsFailed = 0

function testGetAndSetForEachLayer()
	list_of_layers = {["Base"] = 1, ["CommandLine"] = 2, ["GlobalGame"] = 3, ["LocalGame"] = 4, ["Movie"] = 5, ["Netplay"] = 6, ["CurrentRun"] = 7}
	io.write("Testing writing to each layer...\n")
	for key, value in pairs(list_of_layers) do
		io.write("Test " .. tostring(testsRun + 0) .. ":\n")
		io.write("Testing writing to layer " .. key .. " for System - Main, Section - Interface, SettingName - debugModeEnabled\n")
		if not config:doesLayerExist(key) then
			io.write("Layer " .. tostring(key) .. " did not exist - skipping test...\n")
		else
			originalVal = config:getConfigSettingForLayer("bool", key, "MAIN", "INTERFACE", "debugModeEnabled")
			config:setConfigSettingForLayer("bool", key, "MAIN", "INTERFACE", "debugModeEnabled", not originalVal)
			if config:getConfigSettingForLayer("bool", key, "MAIN", "INTERFACE", "debugModeEnabled") == not originalVal then
				io.write("Test passed!\n")
				testsPassed = testsPassed + 1
				config:setConfigSettingForLayer("bool", key, "MAIN", "INTERFACE", "debugModeEnabled", originalVal)
			else
				io.write("Test failed!\n")
				testsFailed = testsFailed + 1
			end
		end
		testsRun = testsRun + 1
	end
end

function mainTestFunc()
	testGetAndSetForEachLayer()
	OnFrameStart:unregister(funcRef)
	config:setConfigSettingForLayer("bool", "Base", "MAIN", "INTERFACE", "debugModeEnabled", false)
	config:saveSettings()
	io.flush()
	io.close()
end

file = io.open("LuaExamplesAndTests/TestResults/LuaConfigApiTestsResults.txt", "w")
io.output(file)
funcRef = OnFrameStart:register(mainTestFunc)