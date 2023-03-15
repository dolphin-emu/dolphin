dolphin:importModule("GraphicsAPI", "1.0")
require ("emu")

startPos = 0;
while true do
	emu:frameAdvance()
	--GraphicsAPI:drawLine(45.7, 300.0, 1000.1, 300.0, 8.0, "0x50903fff")
	--GraphicsAPI:drawFilledRectangle(400.0, 400.0, 800.0, 800.0, "red")
	--GraphicsAPI:drawEmptyRectangle(400.0, 400.0, 800.0, 800.0, 10.0, "green")
	--GraphicsAPI:drawEmptyTriangle(50.0, 700.0, 700.0, 700.0, 375.0, 200.0, 6.0, "green")
	--GraphicsAPI:drawFilledTriangle(50.7, 700.0, 700.0, 700.0, 375.0, 200.0, "green")
	--GraphicsAPI:drawEmptyCircle(200.0, 200.0, 150.0, "red", 5.0)
	--GraphicsAPI:drawFilledCircle(200.0, 200.0, 150.0, "yellow")
	--GraphicsAPI:drawEmptyPolygon({ {400.0, 400.0}, {500.0, 500.0}, {700.0, 500.0}, {800.0, 400.0}, {700.0, 300.0}, {500.0, 300.0}}, 12.3, "red")
	--GraphicsAPI:drawFilledPolygon({ {400.0, 400.0}, {500.0, 500.0}, {700.0, 500.0}, {800.0, 400.0}, {700.0, 300.0}, {500.0, 300.0}}, "green")
	--GraphicsAPI:drawText(100.0, 150.0, "green", "Hello World!")
	--GraphicsAPI:drawEmptyArc(700.0, 600.0, 800.0, 200.0, 1200.0, 200.0, 1100.0, 600.0, 0)
	--GraphicsAPI:drawEmptyArc(700.0, 600.0, 600.0, 1000.0, 1000.0, 1000.0, 1100.0, 600.0, 0)

	
	--GraphicsAPI:drawEmptyPolygon({ {400.0, 400.0}, {500.0, 500.0}, {700.0, 500.0}, {800.0, 400.0}, {700.0, 300.0}, {500.0, 300.0}}, 12.3, "red")
	--GraphicsAPI:drawEmptyTriangle(300.0, 0.0, 200.0, 200.0, 100.0, 100.0, 4.0, "blue")
	--GraphicsAPI:drawEmptyCircle(300.0, 300.0, 240.0, "green", 3.0)
	--GraphicsAPI:beginWindow("OuterWindow")

	--GraphicsAPI:addRadioButtonGroup(30)
	--GraphicsAPI:addRadioButton("Apples", 30, 0)
	--GraphicsAPI:addRadioButton("Bananas", 30, 1)
	--GraphicsAPI:addRadioButton("Carrots", 30, 2)
	
	--GraphicsAPI:addRadioButtonGroup(20)
	--GraphicsAPI:addRadioButton("Ice Cream", 20, 0)
	--GraphicsAPI:addRadioButton("Sherbert", 20, 1)
	
	--GraphicsAPI:setRadioButtonGroupValue(20, 1)
	--temp = GraphicsAPI:getRadioButtonGroupValue(20)
	--GraphicsAPI:setRadioButtonGroupValue(30, temp)
	--GraphicsAPI:endWindow()
	--GraphicsAPI:drawEmptyCircle(100.0, 100.0, 100.0, "red", 4.0)
	
	--GraphicsAPI:drawFilledRectangle(400.0, 400.0, 800.0, 800.0, "red")
	
	GraphicsAPI:drawEmptyCircle(startPos, 400, 20, "yellow", 5.0)
	startPos = startPos + 30
	
	if (startPos >= 2400) then
		startPos = 0
	end
	
end