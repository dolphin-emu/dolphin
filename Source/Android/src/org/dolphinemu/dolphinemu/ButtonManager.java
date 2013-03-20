package org.dolphinemu.dolphinemu;

public class ButtonManager {
	
	private final int NUMBUTTONS = 15;
	
	Button[] Buttons;
	float[][] ButtonCoords = 
		{	// X, Y, X, EY, EX, EY, EX, Y
			{0.75f, -1.0f, 0.75f, -0.75f, 1.0f, -0.75f, 1.0f, -1.0f},
			{0.50f, -1.0f, 0.50f, -0.75f, 0.75f, -0.75f, 0.75f, -1.0f},
		};
	public ButtonManager()
	{
		Buttons = new Button[NUMBUTTONS];
		
		Buttons[0] = new Button("A", ButtonCoords[0]);
		Buttons[1] = new Button("B", ButtonCoords[1]);
		
	}
	Button GetButton(int ID)
	{
		return Buttons[ID];
	}
	float[][] GetButtonCoords()
	{
		return ButtonCoords;
	}
	public int ButtonPressed(int action, float x, float y)
	{
		for (int a = 0; a < 2; ++a)
		{
			if (x >= Buttons[a].X() &&
				x <= Buttons[a].EX() &&
				-y >= Buttons[a].Y() && 
				-y <= Buttons[a].EY())
			{
				return a;
			}
		}
		return -1;
	}
}
