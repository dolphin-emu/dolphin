//			DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//					Version 2, December 2004

// Copyright (C) 2013 mudlord

// Everyone is permitted to copy and distribute verbatim or modified
// copies of this license document, and changing it is allowed as long
// as the name is changed.

//			DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
//	TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

// 0. You just DO WHAT THE FUCK YOU WANT TO.

uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;
#define FXAA_REDUCE_MIN		(1.0/ 128.0)
#define FXAA_REDUCE_MUL		(1.0 / 8.0)
#define FXAA_SPAN_MAX		8.0
 
vec4 applyFXAA(vec2 fragCoord, sampler2D tex)
{
	vec4 color;
	vec2 inverseVP = resolution.zw;
	vec3 rgbNW = texture(tex, (fragCoord + vec2(-1.0, -1.0)) * inverseVP).xyz;
	vec3 rgbNE = texture(tex, (fragCoord + vec2(1.0, -1.0)) * inverseVP).xyz;
	vec3 rgbSW = texture(tex, (fragCoord + vec2(-1.0, 1.0)) * inverseVP).xyz;
	vec3 rgbSE = texture(tex, (fragCoord + vec2(1.0, 1.0)) * inverseVP).xyz;
	vec3 rgbM  = texture(tex, fragCoord  * inverseVP).xyz;
	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
 
	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
						(0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
			max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
			dir * rcpDirMin)) * inverseVP;

	vec3 rgbA = 0.5 * (
		texture(tex, fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
		texture(tex, fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		texture(tex, fragCoord * inverseVP + dir * -0.5).xyz +
		texture(tex, fragCoord * inverseVP + dir * 0.5).xyz);
 
	float lumaB = dot(rgbB, luma);
	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		color = vec4(rgbA, 1.0);
	else
		color = vec4(rgbB, 1.0);
	return color;
}

void main()
{
	ocol0 = applyFXAA(uv0 * resolution.xy, samp9);
}
