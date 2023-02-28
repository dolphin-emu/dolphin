dolphin:importModule("EmuAPI", "1.0.0")

EmuClass = {}

function EmuClass:new(obj)
	obj = obj or {}
	self.__index = self
	return setmetatable(obj, self)
end

emu = EmuClass:new(nil)

function EmuClass:frameAdvance()
	EmuAPI:frameAdvance()
end

function EmuClass:loadState(savestateFilename)
	EmuAPI:loadState(savestateFilename)
end

function EmuClass:saveState(savestateFilename)
	EmuAPI:saveState(savestateFilename)
end

function EmuClass:playMovie(movieFilename)
	EmuAPI:playMovie(movieFilename)
end

function EmuClass:saveMovie(movieFilename)
	EmuAPI:saveMovie(movieFilename)
end