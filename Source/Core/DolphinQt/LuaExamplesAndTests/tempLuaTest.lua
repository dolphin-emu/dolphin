require("emu")
require ("graphics")

while true do
	graphics:beginWindow("Main Window")
	graphics:beginWindow("Inner Window")
	graphics:addCheckbox("Ice Cream", 42)
	graphics:endWindow()
		graphics:setCheckboxValue(42, true)
	val = graphics:getCheckboxValue(42)
	print("Val should be true. Val was: " .. tostring(val))
	graphics:endWindow()
	
	emu:frameAdvance()
end