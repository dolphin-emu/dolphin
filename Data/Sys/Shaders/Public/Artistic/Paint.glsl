/**
 * Basic kuwahara filtering by Jan Eric Kyprianidis <www.kyprianidis.com>
 * https://code.google.com/p/gpuakf/source/browse/glsl/kuwahara.glsl
 * 
 * Copyright (C) 2009-2011 Computer Graphics Systems Group at the
 * Hasso-Plattner-Institut, Potsdam, Germany <www.hpi3d.de>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * Paint effect shader for ENB by kingeric1992
 * http://enbseries.enbdev.com/forum/viewtopic.php?f=7&t=3244#p53168
 * 
 * Modified and optimized for ReShade by JPulowski
 * https://reshade.me/forum/shader-presentation/261
 * 
 * Do not distribute without giving credit to the original author(s).
 * 
 * Ported to Dolphin by iwubcode (based off of 1.3)
 */

void Kuwahara()
{
	int paint_radius = GetOption(_PaintRadius);
	float n = pow(paint_radius, -2.0);
	float4 col = float4(1.0, 1.0, 1.0, 1.0);
	
	// Using vectors instead of arrays, otherwise temp register index gets exceeded very quickly
	float4 m0 = float4(0.0, 0.0, 0.0, 0.0);
	float4 m1 = float4(0.0, 0.0, 0.0, 0.0);
	float4 m2 = float4(0.0, 0.0, 0.0, 0.0);
	float4 m3 = float4(0.0, 0.0, 0.0, 0.0);
	float3 s1 = float3(0.0, 0.0, 0.0);
	float3 s2 = float3(0.0, 0.0, 0.0);
	float3 s3 = float3(0.0, 0.0, 0.0);
	float3 s4 = float3(0.0, 0.0, 0.0);
	
	for (int i = 0; i < paint_radius; i++) {
		for (int j = 0; j < paint_radius; j++) {
			col.rgb = SampleLocation(GetCoordinates() + GetInvResolution() * float2(-i, -j)).rgb;
			m0.rgb += col.rgb;
			s1 += col.rgb * col.rgb;
			
			col.rgb = SampleLocation(GetCoordinates() + GetInvResolution() * float2( i, -j)).rgb;
			m1.rgb += col.rgb;
			s2 += col.rgb * col.rgb;
			
			col.rgb = SampleLocation(GetCoordinates() + GetInvResolution() * float2( i,  j)).rgb;
			m2.rgb += col.rgb;
			s3 += col.rgb * col.rgb;
			
			col.rgb = SampleLocation(GetCoordinates() + GetInvResolution() * float2(-i,  j)).rgb;
			m3.rgb += col.rgb;
			s4 += col.rgb * col.rgb;
		}
	}
	
	m0.rgb *= n; m1.rgb *= n;
	m2.rgb *= n; m3.rgb *= n;
	
	// Sigma2
	m0.a = dot(distance(s1.rgb * n, m0.rgb * m0.rgb), 1.0);
	m1.a = dot(distance(s2.rgb * n, m1.rgb * m1.rgb), 1.0);
	m2.a = dot(distance(s3.rgb * n, m2.rgb * m2.rgb), 1.0);
	m3.a = dot(distance(s4.rgb * n, m3.rgb * m3.rgb), 1.0);
	
	col = m0.a < col.a ? m0 : col;
	col = m1.a < col.a ? m1 : col;
	col = m2.a < col.a ? m2 : col;
	col = m3.a < col.a ? m3 : col;
	
	SetOutput(float4(col.rgb, 1.0));
}

void Paint()
{
	float4 col;
	float3 lumcoeff = float3(0.2126, 0.7152, 0.0722) * 9.0; // Multiplied by total number of available intensity levels
	
	float4 col0 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col1 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col2 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col3 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col4 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col5 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col6 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col7 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col8 = float4(0.0, 0.0f, 0.0f, 0.0f);
	float4 col9 = float4(0.0, 0.0f, 0.0f, 0.0f);
	
	int paint_radius = GetOption(_PaintRadius);
	
	for (int i = -paint_radius; i <= paint_radius; i++) {
		for (int j = -paint_radius; j <= paint_radius; j++) {
			col.rgb = SampleLocation(GetCoordinates() + GetInvResolution() * float2(i, j)).rgb;
			col.a   = round(dot(col.rgb, lumcoeff)) + 1.0;	// Store intensity in alpha channel and increase it by 1, so we can count
															// values between 0.0 - 1.0
			col0 += col.a ==  1.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col1 += col.a ==  2.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col2 += col.a ==  3.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col3 += col.a ==  4.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col4 += col.a ==  5.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col5 += col.a ==  6.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col6 += col.a ==  7.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col7 += col.a ==  8.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col8 += col.a ==  9.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
			col9 += col.a == 10.0 ? col : float4(0.0, 0.0f, 0.0f, 0.0f);
		}
	}

	// Calculate intensity count
	col1.a /=  2.0;
	col2.a /=  3.0;
	col3.a /=  4.0;
	col4.a /=  5.0;
	col5.a /=  6.0;
	col6.a /=  7.0;
	col7.a /=  8.0;
	col8.a /=  9.0;
	col9.a /= 10.0;
	 
	col.a  =  0.0;
	
	// Calculate mode
	col = col0.a > col.a ? col0 : col;
	col = col1.a > col.a ? col1 : col;
	col = col2.a > col.a ? col2 : col;
	col = col3.a > col.a ? col3 : col;
	col = col4.a > col.a ? col4 : col;
	col = col5.a > col.a ? col5 : col;
	col = col6.a > col.a ? col6 : col;
	col = col7.a > col.a ? col7 : col;
	col = col8.a > col.a ? col8 : col;
	col = col9.a > col.a ? col9 : col;
	
	SetOutput(float4(col.rgb / col.a, 1.0f));
}
