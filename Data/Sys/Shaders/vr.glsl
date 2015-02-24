// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
[configuration]

[OptionRangeFloat]
GUIName = Blue Distortion
OptionName = k1_blue
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.66

[OptionRangeFloat]
GUIName = Green Distortion
OptionName = k1_green
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.53

[OptionRangeFloat]
GUIName = Red Distortion
OptionName = k1_red
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.45

[OptionRangeFloat]
GUIName = Vertical Center
OptionName = center_proj_y
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.55

[OptionRangeFloat]
GUIName = Horizontal Center
OptionName = center_proj_x
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.47

[/configuration]
*/

vec2 Distort(vec2 p, float k1)
{
    float r2 = p.x * p.x + p.y * p.y;
    float r = sqrt(r2);
    
    float newRadius = (1 + k1*r*r);
    p.x = p.x * newRadius;
    p.y = p.y * newRadius;

    return p;
}

void main()
{
    vec2 uv_red, uv_green, uv_blue;
    vec4 color_red, color_green, color_blue;
    vec2 sectorOrigin;
    
	if (GetLayer() > 0)
		sectorOrigin = vec2(1.0 - GetOption(center_proj_x), GetOption(center_proj_y));
	else
		sectorOrigin = vec2(GetOption(center_proj_x), GetOption(center_proj_y));

    uv_red      =  Distort(GetCoordinates() - sectorOrigin, GetOption(k1_red))      + sectorOrigin;
    uv_green    =  Distort(GetCoordinates() - sectorOrigin, GetOption(k1_green))    + sectorOrigin;
    uv_blue     =  Distort(GetCoordinates() - sectorOrigin, GetOption(k1_blue))     + sectorOrigin;
    
    color_red   = SampleLocation( uv_red   );
    color_green = SampleLocation( uv_green );
    color_blue  = SampleLocation( uv_blue  );

    if (    ((uv_red.x>0)     && (uv_red.x<1)        && (uv_red.y>0)     && (uv_red.y<1)))
    {
        SetOutput(vec4(color_red.x, color_green.y, color_blue.z, 1.0));
    }
	else
	{
        SetOutput(vec4(0,0,0,0)); //black
    }
}
