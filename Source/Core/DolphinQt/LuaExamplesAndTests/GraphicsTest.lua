dolphin:importModule("GraphicsAPI", "1.0")
require ("emu")

while true do
	emu:frameAdvance()
	GraphicsAPI:drawLine(45.7, 300.0, 1000.1, 300.0, 8.0, "0x50903fff")
	GraphicsAPI:drawFilledRectangle(400.0, 400.0, 800.0, 800.0, "red")
	GraphicsAPI:drawEmptyRectangle(400.0, 400.0, 800.0, 800.0, 10.0, "green")


end