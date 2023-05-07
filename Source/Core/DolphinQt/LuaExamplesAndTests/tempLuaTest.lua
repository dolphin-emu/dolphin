dolphin:importModule("EmuAPI", "1.0")
dolphin:importModule("MemoryAPI", "1.0")
dolphin:importModule("StatisticsAPI", "1.0")

function mainFunc()
	while true do
	if (StatisticsAPI:getCurrentFrame() == 1100) then
		EmuAPI:saveState("newEarlySaveState.sav")
		return
	end
	EmuAPI:frameAdvance()
	end
end

mainFunc()