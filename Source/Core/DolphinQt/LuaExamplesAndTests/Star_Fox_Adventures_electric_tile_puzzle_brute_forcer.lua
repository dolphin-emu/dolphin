require ("emu")
require ("gc_controller")
require ("memory")
require ("statistics")

funcRef = 0
basePasswordAddress = 0x80329849
earlySaveStatePath= "saveStateName.sav"
movieNamePath = "movieName.dtm"

function isPasswordSet()
	if memory:readFrom(basePasswordAddress + 10, "u8") ~= 0 then
		return true
	else
		return false
	end
end

function getPassword()
	returnTable = {}
	returnTable[1] = memory:readFrom(basePasswordAddress, "u8")
	returnTable[2] = memory:readFrom(basePasswordAddress + 2, "u8")
	returnTable[3] = memory:readFrom(basePasswordAddress + 4, "u8")
	returnTable[4] = memory:readFrom(basePasswordAddress + 6, "u8")
	returnTable[5] = memory:readFrom(basePasswordAddress + 8, "u8")
	returnTable[6] = memory:readFrom(basePasswordAddress + 10, "u8")
	return returnTable
end



emu:playMovie(movieNamePath)
totalMovieLength = statistics:getMovieLength()

function mainScriptBody()
	emu:loadState(earlySaveStatePath)

	if statistics:getCurrentFrame() <= totalMovieLength - 10 and not isPasswordSet() then
		gcController:addButtonPressChance(1, 15, "dPadLeft")
		gcController:addButtonPressChance(1, 10, "dPadRight")
		gcController:addButtonPressChance(1, 0.005, "Y")
		gcController:addButtonComboChance(1, 10, false, {cStickX = 255})
		gcController:addButtonComboChance(1, 10, false, {cStickX = 0})
		gcController:addButtonComboChance(1, 10, false, {cStickY = 0})
		gcController:addButtonComboChance(1, 10, false, {cStickY = 255})
	end
	
	if statistics:getCurrentFrame() <= totalMovieLength - 10 then -- This if statement is true when we broke out of the loop because the password was set.
		emu:frameAdvance()
		passwordValue = getPassword()
		print("bruteForcePattern_" .. tostring(passwordValue[1]) .. tostring(passwordValue[2]) .. tostring(passwordValue[3]) .. "-" .. tostring(passwordValue[4]) .. tostring(passwordValue[5]) .. tostring(passwordValue[6]))
		if passwordValue[1] ~= 0 and passwordValue[1] <= 2 and passwordValue[2] <= 2 and passwordValue[3] <= 2 and passwordValue[4] <= 2 and passwordValue[5] <= 2 and passwordValue[6] <= 2 then
			generatedMoviePath = "bruteForcePattern_" .. tostring(passwordValue[1]) .. tostring(passwordValue[2]) .. tostring(passwordValue[3]) .. "-" .. tostring(passwordValue[4]) .. tostring(passwordValue[5]) .. tostring(passwordValue[6]) .. ".dtm"
			print("Writing to: " .. generatedMoviePath)
			emu:saveMovie(generatedMoviePath)
		end
		
		if passwordValue[1] == 1 and passwordValue[2] == 1 and passwordValue[3] == 1 and passwordValue[4] == 1 and passwordValue[5] == 1 and passwordValue[6] == 1 then
			OnFrameStart:unregister(funcRef)
			print("Finished generating a movie with a 111 111 electric tile puzzle!")
		end
	end
end

funcRef = OnFrameStart:register(mainScriptBody)