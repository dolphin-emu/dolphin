//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: Input Configuration and Calibration
// Description: Common SDL Input Functions
//
// Author: Falcon4ever (nJoy@falcon4ever.com, www.multigesture.net), JPeterson etc
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "SDL.h" // Local
////////////////////////////////////


namespace InputCommon
{


//////////////////////////////////////////////////////////////////////////////////////////
// Degree to radian and back
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
float Deg2Rad(float Deg)
{
	return Deg * (M_PI / 180.0);
}
float Rad2Deg(float Rad)
{
	return (Rad * 180.0) / M_PI;
}
/////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Convert stick values
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

/* Convert stick values.

   The value returned by SDL_JoystickGetAxis is a signed integer s16
   (-32768 to 32767). The value used for the gamecube controller is an unsigned
   char u8 (0 to 255) with neutral at 0x80 (128), so that it's equivalent to a signed
   -128 to 127.
*/
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
int Pad_Convert(int _val)
{
	/* If the limits on PadState[].axis[] actually is a u16 then we don't need this
	   but if it's not actually limited to that we need to apply these limits */
	if(_val > 32767) _val = 32767; // upper limit
	if(_val < -32768) _val = -32768; // lower limit
		
	// Convert the range (-0x8000 to 0x7fff) to (0 to 0xffff)
	_val = 0x8000 +_val;

	// Convert the range (-32768 to 32767) to (-128 to 127)
	_val = _val >> 8;

	//Console::Print("0x%04x  %06i\n\n", _val, _val);

	return _val;
}

//////////////////////////////////////////////////////////////////////////////////////////
/* Convert the stick raidus from a circular to a square. I don't know what input values
   the actual GC controller produce for the GC, it may be a square, a circle or something
   in between. But one thing that is certain is that PC pads differ in their output (as
   shown in the list below), so it may be beneficiary to convert whatever radius they
   produce to the radius the GC games expect. This is the first implementation of this
   that convert a square radius to a circual radius. Use the advanced settings to enable
   and calibrate it.

   Observed diagonals:
   Perfect circle: 71% = sin(45)
   Logitech Dual Action: 100%
   Dual Shock 2 (Original) with Super Dual Box Pro: 90%
   XBox 360 Wireless: 85%
   GameCube Controller (Third Party) with EMS TrioLinker Plus II: 60%
*/
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

/* Calculate the distance from the center of the current stick coordinates. The distance is defined
   as at most sqrt(2) in the corners */
float SquareDistance(float deg)
{
	// See if we have to adjust the angle
	deg = abs(deg);
	if( (deg > 45 && deg < 135) ) deg = deg - 90;

	float val = abs(cos(Deg2Rad(deg)));
	float dist = 1 / val; // Calculate distance from center

	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("Deg:%f  Val:%f  Dist:%f", deg, val, dist));

	return dist;
}
// Produce the circle from the original
std::vector<int> Square2Circle(int _x, int _y, int _pad, std::string SDiagonal, bool Circle2Square)
{
	/* Do we need this? */
	if(_x > 32767) _x = 32767; if(_y > 32767) _y = 32767; // upper limit
	if(_x < -32768) _x = -32768; if(_y > 32767) _y = 32767; // lower limit

	// ====================================
	// Convert to circle
	// -----------
	// Get the manually configured diagonal distance
	int Tmp = atoi (SDiagonal.substr(0, SDiagonal.length() - 1).c_str());
	float Diagonal = Tmp / 100.0;

	// First make a perfect square in case we don't have one already
	float OrigDist = sqrt(  pow((float)_y, 2) + pow((float)_x, 2)  ); // Get current distance
	float deg = Rad2Deg(atan2((float)_y, (float)_x)); // Get current angle

	// A diagonal of 85% means a maximum distance of 0.85 * sqrt(2) ~1.2 in the diagonals
	float corner_circle_dist = ( Diagonal / sin(Deg2Rad(45)) );
	float SquareDist = SquareDistance(deg);
	// The original-to-square distance adjustment
	float adj_ratio1;
	// The circle-to-square distance adjustment
	float adj_ratio2 = SquareDist;
	 // The resulting distance
	float result_dist;

	// Calculate the corner-to-square adjustment ratio
	if(corner_circle_dist < SquareDist) adj_ratio1 = SquareDist / corner_circle_dist;
		else adj_ratio1 = 1;

	// Calculate the resulting distance
	if(Circle2Square)
		result_dist = OrigDist * adj_ratio1;
	else
		result_dist = OrigDist * adj_ratio1 / adj_ratio2;
	
	// Calculate x and y and return it
	float x = result_dist * cos(Deg2Rad(deg));
	float y = result_dist * sin(Deg2Rad(deg));
	// Make integers
	int int_x = (int)floor(x);
	int int_y = (int)floor(y);
	// Boundaries
	if (int_x < -32767) int_x = -32767; if (int_x > 32767) int_x = 32767;
	if (int_y < -32767) int_y = -32767; if (int_y > 32767) int_y = 32767;
	// Return it
	std::vector<int> vec;
	vec.push_back(int_x);
	vec.push_back(int_y);

	// Debugging
	//m_frame->m_pStatusBar2->SetLabel(wxString::Format("%f  %f  %i", corner_circle_dist, Diagonal, Tmp));

	return vec;
}
/////////////////////////////////////////////////////////////////////


}