dolphin:importModule("StatisticsAPI", "1.0.0")

StatisticsClass = {}

function StatisticsClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

statistics = StatisticsClass:new(nil)

function StatisticsClass:isRecordingInput()
	return StatisticsAPI:isRecordingInput()
end

function StatisticsClass:isRecordingInputFromSaveState()
	return StatisticsAPI:isRecordingInputFromSaveState()
end

function StatisticsClass:isPlayingInput()
	return StatisticsAPI:isPlayingInput()
end

function StatisticsClass:isMovieActive()
	return StatisticsAPI:isMovieActive()
end

function StatisticsClass:getCurrentFrame()
	return StatisticsAPI:getCurrentFrame()
end

function StatisticsClass:getMovieLength()
	return StatisticsAPI:getMovieLength()
end

function StatisticsClass:getRerecordCount()
	return StatisticsAPI:getRerecordCount()
end

function StatisticsClass:getCurrentInputCount()
	return StatisticsAPI:getCurrentInputCount()
end

function StatisticsClass:getTotalInputCount()
	return StatisticsAPI:getTotalInputCount()
end

function StatisticsClass:getCurrentLagCount()
	return StatisticsAPI:getCurrentLagCount()
end

function StatisticsClass:getTotalLagCount()
	return StatisticsAPI:getTotalLagCount()
end

function StatisticsClass:getRAMSize()
	return StatisticsAPI:getRAMSize()
end

function StatisticsClass:getL1CacheSize()
	return StatisticsAPI:getL1CacheSize()
end

function StatisticsClass:getFakeVMemSize()
	return StatisticsAPI:getFakeVMemSize()
end

function StatisticsClass:getExRAMSize()
	return StatisticsAPI:getExRAMSize()
end