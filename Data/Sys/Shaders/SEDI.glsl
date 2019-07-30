/*
   Simple Edge Directed Interpolation (SEDI) v1.0

   Copyright (C) 2017 SimoneT - simone1tarditi@gmail.com

   de Blur - Copyright (C) 2016 guest(r) - guest.r@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

// de Blur control
#define filterparam 5.0

float CLength(float3 c1)
{
	float rmean = c1.r * 0.5;
	c1 *= c1;
	return sqrt((2.0 + rmean) * c1.r + 4.0 * c1.g + (3.0 - rmean) * c1.b);
}

float Cdistance(float3 c1, float3 c2)
{
	float rmean = (c1.r + c2.r) * 0.5;
	c1 = pow(c1 - c2, float3(2.0));
	return sqrt((2.0 + rmean) * c1.r + 4.0 * c1.g + (3.0 - rmean) * c1.b);
}

float3 ColMin(float3 a, float3 b)
{
	float dist = step(0.01, sign(CLength(a) - CLength(b)));
	return mix(a, b, dist);
}

float3 ColMax(float3 a, float3 b)
{
	float dist = step(0.01, sign(CLength(a) - CLength(b)));
	return mix(b, a, dist);
}

float3 Blur(float2 TexCoord)
{
	float2 shift  = GetInvResolution() * 0.5;

	float3 C06 = SampleLocation(TexCoord - shift.xy).rgb;
	float3 C07 = SampleLocation(TexCoord + float2( shift.x,-shift.y)).rgb;
	float3 C11 = SampleLocation(TexCoord + float2(-shift.x, shift.y)).rgb;
	float3 C12 = SampleLocation(TexCoord + shift.xy).rgb;

	float dif1 = Cdistance(C06, C12) + 0.00001;
	float dif2 = Cdistance(C07, C11) + 0.00001;

	dif1 = pow(dif1, filterparam);
	dif2 = pow(dif2, filterparam);

	float dif3 = dif1 + dif2;
	return (dif1 * (C07 + C11) * 0.5 + dif2 * (C06 + C12) * 0.5) / dif3;
}

// de Blur code
float3 deBlur() 
{
	float2 Size = GetInvResolution();
	float2 coord = GetCoordinates();
	float2 dx = float2( Size.x,    0.0);
	float2 dy = float2(    0.0, Size.y);
	float2 g1 = float2( Size.x, Size.y);
	float2 g2 = float2(-Size.x, Size.y);

	float3 C0 = Blur(coord-g1).rgb; 
	float3 C1 = Blur(coord-dy).rgb;
	float3 C2 = Blur(coord-g2).rgb;
	float3 C3 = Blur(coord-dx).rgb;
	float3 C4 = Blur(coord   ).rgb;
	float3 C5 = Blur(coord+dx).rgb;
	float3 C6 = Blur(coord+g2).rgb;
	float3 C7 = Blur(coord+dy).rgb;
	float3 C8 = Blur(coord+g1).rgb;

	float3 mn1 = ColMin(ColMin(C0, C1), C2);
	float3 mn2 = ColMin(ColMin(C3, C4), C5);
	float3 mn3 = ColMin(ColMin(C6, C7), C8);
	     mn1 = ColMin(ColMin(mn1, mn2), mn3);

	float3 mx1 = ColMax(ColMax(C0, C1), C2);
	float3 mx2 = ColMax(ColMax(C3, C4), C5);
	float3 mx3 = ColMax(ColMax(C6, C7), C8);
 	     mx1 = ColMax(ColMax(mx1, mx2), mx3);

	float dif1 = Cdistance(C4, mn1) + 0.00001;
	float dif2 = Cdistance(C4, mx1) + 0.00001;

	dif1 = pow(dif1, filterparam);
	dif2 = pow(dif2, filterparam);

	float dif3 = dif1 + dif2;
	return (dif1 * mx1 + dif2 * mn1) / dif3;
}

void main()
{
	SetOutput(float4(deBlur(), 1.0));
} 
