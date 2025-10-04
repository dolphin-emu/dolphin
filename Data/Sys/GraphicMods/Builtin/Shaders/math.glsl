#define PI 3.1415926

// Chris­t­ian Schüler http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv )
{
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	//float invmax = 1.0 / sqrt(max(dot(T,T), dot(B,B)));
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
	return mat3( normalize(T * invmax), normalize(B * invmax), N );
}

vec3 perturb_normal( vec3 N, vec3 P, vec2 texcoord, vec3 map)
{
	mat3 TBN = cotangent_frame( N, -P, texcoord);
	return normalize( TBN * map );
}

float saturate(float val)
{
	return clamp(val, 0.0, 1.0);
}

vec3 saturate(vec3 val)
{
	return clamp(val, 0.0, 1.0);
}
