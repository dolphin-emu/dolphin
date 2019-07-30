const float Filmic_Strength = 0.60; 
const float Filmic_Contrast = 1.03; 
const float Fade = 0.0; 
const float Linearization = 1.0; 
const float Filmic_Bleach = 0.0; 
const float Saturation = -0.25; 
const float BaseCurve = 1.5; 
const float BaseGamma = 1.0; 
const float EffectGamma = 0.68;

void main() 
{
	float3 B = Sample().rgb; 
	float3 G = B; 
	float3 H = float3(0.01); 

	B = clamp(B, 0.0, 1.); 
	B = pow(float3(B), float3(Linearization)); 
	B = mix(H, B, Filmic_Contrast); 

	float3 LumCoeff = float3(0.2126, 0.7152, 0.0722); 
	float A = dot(B.rgb, LumCoeff); 
	float3 D = float3(A); 

	B = pow(B, 1.0 / float3(BaseGamma)); 

	float RedCurve = 1.0; 
	float GreenCurve = 1.0; 
	float BlueCurve = 1.0; 

	float a = RedCurve; 
	float b = GreenCurve; 
	float c = BlueCurve; 
	float d = BaseCurve; 

	float y = 1.0 / (1.0 + exp(a / 2.0)); 
	float z = 1.0 / (1.0 + exp(b / 2.0)); 
	float w = 1.0 / (1.0 + exp(c / 2.0)); 
	float v = 1.0 / (1.0 + exp(d / 2.0)); 

	float3 C = B; 

	D.r = (1.0 / (1.0 + exp(-a * (D.r - 0.5))) - y) / (1.0 - 2.0 * y); 
	D.g = (1.0 / (1.0 + exp(-b * (D.g - 0.5))) - z) / (1.0 - 2.0 * z); 
	D.b = (1.0 / (1.0 + exp(-c * (D.b - 0.5))) - w) / (1.0 - 2.0 * w); 

	D = pow(D, 1.0 / float3(EffectGamma)); 

	float3 Di = 1.0 - D; 

	D = mix(D, Di, Filmic_Bleach); 

	float EffectGammaR = 1.0; 
	float EffectGammaG = 1.0; 
	float EffectGammaB = 1.0; 

	D.r = pow(abs(D.r), 1.0 / EffectGammaR); 
	D.g = pow(abs(D.g), 1.0 / EffectGammaG); 
	D.b = pow(abs(D.b), 1.0 / EffectGammaB); 

	if (D.r < 0.5) 
		C.r = (2.0 * D.r - 1.0) * (B.r - B.r * B.r) + B.r; 
	else 
		C.r = (2.0 * D.r - 1.0) * (sqrt(B.r) - B.r) + B.r; 

	if (D.g < 0.5) 
		C.g = (2.0 * D.g - 1.0) * (B.g - B.g * B.g) + B.g; 
	else 
		C.g = (2.0 * D.g - 1.0) * (sqrt(B.g) - B.g) + B.g; 

	if (D.b < 0.5) 
		C.b = (2.0 * D.b - 1.0) * (B.b - B.b * B.b) + B.b; 
	else 
		C.b = (2.0 * D.b - 1.0) * (sqrt(B.b) - B.b) + B.b; 

	float3 F = mix(B, C, Filmic_Strength); 
	F = (1.0 / (1.0 + exp(-d * (F - 0.5))) - v) / (1.0 - 2.0 * v); 

	float r2R = 1.0 - Saturation; 
	float g2R = 0.0 + Saturation; 
	float b2R = 0.0 + Saturation; 

	float r2G = 0.0 + Saturation; 
	float g2G = (1.0 - Fade) - Saturation; 
	float b2G = (0.0 + Fade) + Saturation; 

	float r2B = 0.0 + Saturation; 
	float g2B = (0.0 + Fade) + Saturation; 
	float b2B = (1.0 - Fade) - Saturation; 

	float3 iF = F; 

	F.r = (iF.r * r2R + iF.g * g2R + iF.b * b2R); 
	F.g = (iF.r * r2G + iF.g * g2G + iF.b * b2G); 
	F.b = (iF.r * r2B + iF.g * g2B + iF.b * b2B); 

	float N = dot(F.rgb, LumCoeff); 
	float3 Cn = F; 

	if (N < 0.5) 
		Cn = (2.0 * N - 1.0) * (F - F * F) + F; 
	else 
		Cn = (2.0 * N - 1.0) * (sqrt(F) - F) + F; 

	Cn = pow(max(Cn, float3(0)), 1.0 / float3(Linearization)); 

	float3 Fn = mix(B, Cn, Filmic_Strength); 
	SetOutput(float4(Fn, 1.0)); 
}
