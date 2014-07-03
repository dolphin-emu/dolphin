uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 c_center = texture(samp9, uv0);

	float4 bloom_sum = float4(0.0, 0.0, 0.0, 0.0);
	vec2 pos = uv0 + float2(0.3, 0.3) * resolution.zw;
	float2 radius1 = 1.3 * resolution.zw;
	bloom_sum += texture(samp9, pos + float2(-1.5, -1.5) * radius1);
	bloom_sum += texture(samp9, pos + float2(-2.5, 0.0)  * radius1);
	bloom_sum += texture(samp9, pos + float2(-1.5, 1.5) * radius1);
	bloom_sum += texture(samp9, pos + float2(0.0, 2.5) * radius1);
	bloom_sum += texture(samp9, pos + float2(1.5, 1.5) * radius1);
	bloom_sum += texture(samp9, pos + float2(2.5, 0.0) * radius1);
	bloom_sum += texture(samp9, pos + float2(1.5, -1.5) * radius1);
	bloom_sum += texture(samp9, pos + float2(0.0, -2.5) * radius1);

	float2 radius2 = 4.6 * resolution.zw;
	bloom_sum += texture(samp9, pos + float2(-1.5, -1.5) * radius2);
	bloom_sum += texture(samp9, pos + float2(-2.5, 0.0)  * radius2);
	bloom_sum += texture(samp9, pos + float2(-1.5, 1.5)  * radius2);
	bloom_sum += texture(samp9, pos + float2(0.0, 2.5)  * radius2);
	bloom_sum += texture(samp9, pos + float2(1.5, 1.5)  * radius2);
	bloom_sum += texture(samp9, pos + float2(2.5, 0.0)  * radius2);
	bloom_sum += texture(samp9, pos + float2(1.5, -1.5)  * radius2);
	bloom_sum += texture(samp9, pos + float2(0.0, -2.5)  * radius2);

	bloom_sum *= 0.07;
	bloom_sum -= float4(0.3, 0.3, 0.3, 0.3);
	bloom_sum = max(bloom_sum, float4(0.0, 0.0, 0.0, 0.0));

	float2 vpos = (uv0 - float2(0.5, 0.5)) * 2.0;
	float dist = (dot(vpos, vpos));
	dist = 1.0 - 0.4*dist;

	ocol0 = (c_center * 0.7 + bloom_sum) * dist;
}
