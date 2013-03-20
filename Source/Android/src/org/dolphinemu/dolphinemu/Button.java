package org.dolphinemu.dolphinemu;

public class Button {
	private String ButtonID;
	private float _x;
	private float _y;
	private float _ex;
	private float _ey;
	public Button(String Button, float[] Coords)
	{
		ButtonID = Button;
		_x = Coords[0];
		_y = Coords[1];
		_ex = Coords[4];
		_ey = Coords[5];
	}
	public float X()
	{
		return _x;
	}
	public float Y()
	{
		return _y;
	}
	public float EX()
	{
		return _ex;
	}
	public float EY()
	{
		return _ey;
	}
}
