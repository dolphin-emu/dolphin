 ////----------------//
 ///**SuperDepth3D**///
 //----------------////

 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 //* Depth Map Based 3D post-process shader v1.9.4																																	*//
 //* For Ishiiruka ported by Tino																																					*//
 //* --------------------------																																						*//
 //* This work is licensed under a Creative Commons Attribution 3.0 Unported License.																								*//
 //* So you are free to share, modify and adapt it for your needs, and even use it for commercial use.																				*//
 //* I would also love to hear about a project you are using it with.																												*//
 //* https://creativecommons.org/licenses/by/3.0/us/																																*//
 //*																																												*//
 //* Have fun,																																										*//
 //* Jose Negrete AKA BlueSkyDefender																																				*//
 //*																																												*//
 //* http://reshade.me/forum/shader-presentation/2128-sidebyside-3d-depth-map-based-stereoscopic-shader																				*//	
 //* ---------------------------------																																				*//
 //*																																												*//
 //* Original work was based on Shader Based on CryTech 3 Dev work http://www.slideshare.net/TiagoAlexSousa/secrets-of-cryengine-3-graphics-technology								*//
 //*																																												*//
 //* 																																												*//
 //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 /*

[configuration]
[OptionRangeInteger]
GUIName = Depth
OptionName = A_Depth
MinValue = 0
MaxValue = 50
StepAmount = 1
DefaultValue = 15
GUIDescription = Determines the amount of Image Warping and Separation between both eyes. You can Override this setting.

[OptionRangeFloat]
GUIName = Perspective
OptionName = B_Perspective
MinValue = -100
MaxValue = 100
StepAmount = 0.1
DefaultValue = 0
GUIDescription = Determines the perspective point. Default is 0.

[OptionRangeFloat]
GUIName = Depth Limit
OptionName = C_Depth_Limit
MinValue = 0.75
MaxValue = 1.0
StepAmount = 0.001
DefaultValue = 1.0
GUIDescription = Limit how far Depth Image Warping is done. Default is One.

[OptionRangeInteger]
GUIName = Disocclusion Type
OptionName = D_Disocclusion_Type
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 1
GUIDescription = Pick the type of disocclusion you want.

[OptionRangeFloat]
GUIName = Disocclusion Power
OptionName = E_Disocclusion_Power
MinValue = 0
MaxValue = 0.5
StepAmount = 0.001
DefaultValue = 0.025
GUIDescription = Determines the disocclusion effect on the Depth Map. Default is 0.025.

[OptionBool]
GUIName = Depth Map View
OptionName = F_Depth_Map_View
DefaultValue = false
GUIDescription = Display the Depth Map. Use This to Work on your Own Depth Map for your game.

[OptionBool]
GUIName = Depth Map Enhancement
OptionName = G_Depth_Map_Enhancement
DefaultValue = false
GUIDescription = Enable Or Dissable Depth Map Enhancement. Default is Off.

[OptionRangeFloat]
GUIName = Adjust
OptionName = H_Adjust
MinValue = 0
MaxValue = 1.5
StepAmount = 0.001
DefaultValue = 1.0
GUIDescription = Adjust DepthMap Enhancement, Dehancement occurs past one. Default is 1.0.

[OptionRangeInteger]
GUIName = Edge Selection
OptionName = K_Custom_Sidebars
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 1
GUIDescription = Select how you like the Edge of the screen to look like. Mirrored Edges, Black Edges, Stretched Edges.

[OptionRangeInteger]
GUIName = 3D Display Mode
OptionName = L_Stereoscopic_Mode
MinValue = 0
MaxValue = 3
StepAmount = 1
DefaultValue = 0
GUIDescription = Side by Side/Top and Bottom/Line Interlaced/Checkerboard 3D display output.

[OptionRangeInteger]
GUIName = Downscaling Support
OptionName = M_Downscaling_Support
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 0
GUIDescription = Dynamic Super Resolution & Virtual Super Resolution downscaling support for Line Interlaced & Checkerboard 3D displays.

[OptionBool]
GUIName = Eye Swap
OptionName = N_Eye_Swap
DefaultValue = false
GUIDescription = Left right image change.

[Pass]
EntryPoint = DisocclusionMask
OutputScale = 0.5
OutputFormat = R32_FLOAT
Input0=DepthBuffer
Input0Filter=Nearest
Input0Mode=Clamp
[Pass]
EntryPoint = PS_renderLR
Input0=PreviousPass
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
Input2=ColorBuffer
Input2Filter=Linear
Input2Mode=Mirror
Input3=ColorBuffer
Input3Filter=Linear
Input3Mode=Border
Input4=ColorBuffer
Input4Filter=Linear
Input4Mode=Clamp
[/configuration]
*/
	
void DisocclusionMask()
{
	float color = 0;
	
	if(GetOption(D_Disocclusion_Type) > 0 && GetOption(E_Disocclusion_Power) > 0) 
	{	
		const float weight[10] = 
		{ 
		-0.08, -0.05, -0.03, -0.02, -0.01,
		0.01, 0.02, 0.03, 0.05, 0.08
		};
		float2 dir;
		float B;
		float Con = 10;
		float2 texcoord = GetCoordinates();
		if(GetOption(D_Disocclusion_Type) == 1)
		{
			dir = float2(0.5,0);
			B = GetOption(E_Disocclusion_Power)*1.5;
		}
	
		if(GetOption(D_Disocclusion_Type) == 2)
		{
			dir = 0.5 - texcoord;
			B = GetOption(E_Disocclusion_Power)*2;
		}
	
		dir = normalize( dir ); 
		
		for (int i = 0; i < 10; i++)
		{
			float D = SampleDepthLocation(texcoord + dir * weight[i] * B);
			if(GetOption(G_Depth_Map_Enhancement) > 0)
			{
				float A = GetOption(H_Adjust);
				float cDF = 1.025;
				float cDN = 0;	
				D = lerp(pow(abs((exp(D * log(cDF + cDN)) - cDN) / cDF),1000),D,A);
			}
			D = D/Con;
			color += D;
		}
	}
	else
	{
		float D = SampleDepth();
		if(GetOption(G_Depth_Map_Enhancement) > 0)
		{
			float A = GetOption(H_Adjust);
			float cDF = 1.025;
			float cDN = 0;
			D = lerp(pow(abs((exp(D * log(cDF + cDN)) - cDN) / cDF),1000),D,A);
		}
		color = D;
	}	
	SetOutput(float4(color, color, color, 1.0));
} 
  
////////////////////////////////////////////////Left/Right Eye////////////////////////////////////////////////////////
void PS_renderLR()
{	
	const float samples[3] = {0.50, 0.66, 1.0};
	float DepthL = GetOption(C_Depth_Limit), DepthR = GetOption(C_Depth_Limit);
	float2 uv = float2(0.0,0.0);
	float D;
	float P;
	float2 pix = GetInvResolution().xy;
	float4 color = float4(0.0,0.0,0.0,0.0);
	if(OptionEnabled(N_Eye_Swap))
	{
		P = -GetOption(B_Perspective) * pix.x;
		D = -GetOption(A_Depth) * pix.x;
	}
	else
	{	
		P = GetOption(B_Perspective) * pix.x;
		D = GetOption(A_Depth) * pix.x;
	}
	
	float2 texcoord = GetCoordinates();
	for (int j = 0; j < 3; ++j) 
	{	
		uv.x = samples[j] * D;
			
		if(GetOption(L_Stereoscopic_Mode) == 0)
		{	
			DepthL =  min(DepthL,SamplePrevLocation(float2((texcoord.x*2 + P)+uv.x, texcoord.y)).r);
			DepthR =  min(DepthR,SamplePrevLocation(float2((texcoord.x*2-1 - P)-uv.x, texcoord.y)).r);
		}
		else if(GetOption(L_Stereoscopic_Mode) == 1)
		{
			DepthL =  min(DepthL,SamplePrevLocation(float2((texcoord.x + P)+uv.x, texcoord.y*2)).r);
			DepthR =  min(DepthR,SamplePrevLocation(float2((texcoord.x - P)-uv.x, texcoord.y*2-1)).r);
		}
		else
		{
			DepthL =  min(DepthL,SamplePrevLocation(float2((texcoord.x + P)+uv.x, texcoord.y)).r);
			DepthR =  min(DepthR,SamplePrevLocation(float2((texcoord.x - P)-uv.x, texcoord.y)).r);
		}
	}
	
	if(OptionEnabled(F_Depth_Map_View))
	{
		color = SamplePrev().rrrr;
	}
	else
	{
		if(GetOption(L_Stereoscopic_Mode) == 0)
		{
			if(GetOption(K_Custom_Sidebars) == 0)
			{
				color = SampleInputLocation(2, texcoord.x < 0.5 ? float2((texcoord.x*2 + P) + DepthL * D , texcoord.y) : float2((texcoord.x*2-1 - P) - DepthR * D , texcoord.y));
			}
			else if(GetOption(K_Custom_Sidebars) == 1)
			{
				color = SampleInputLocation(3, texcoord.x < 0.5 ? float2((texcoord.x*2 + P) + DepthL * D , texcoord.y) : float2((texcoord.x*2-1 - P) - DepthR * D , texcoord.y));				
			}
			else
			{
				color = SampleInputLocation(4, texcoord.x < 0.5 ? float2((texcoord.x*2 + P) + DepthL * D , texcoord.y) : float2((texcoord.x*2-1 - P) - DepthR * D , texcoord.y));
			}
		}
		else if(GetOption(L_Stereoscopic_Mode) == 1)
		{	
			if(GetOption(K_Custom_Sidebars) == 0)
			{
				color = SampleInputLocation(2, texcoord.y < 0.5 ? float2((texcoord.x+ P) + DepthL * D , texcoord.y*2) : float2((texcoord.x - P) - DepthR * D , texcoord.y*2-1));
			}
			else if(GetOption(K_Custom_Sidebars) == 1)
			{
				color = SampleInputLocation(3, texcoord.y < 0.5 ? float2((texcoord.x+ P) + DepthL * D , texcoord.y*2) : float2((texcoord.x - P) - DepthR * D , texcoord.y*2-1));				
			}
			else
			{
				color = SampleInputLocation(4, texcoord.y < 0.5 ? float2((texcoord.x+ P) + DepthL * D , texcoord.y*2) : float2((texcoord.x - P) - DepthR * D , texcoord.y*2-1));				
			}
		}
		else if(GetOption(L_Stereoscopic_Mode) == 2)
		{
			float gridL;
			
			if(GetOption(M_Downscaling_Support) == 0)
			{
				gridL = frac(texcoord.y*(GetResolution().y/2));
			}
			else if(GetOption(M_Downscaling_Support) == 1)
			{
				gridL = frac(texcoord.y*(1080.0/2));
			}
			else
			{
				gridL = frac(texcoord.y*(1081.0/2));
			}
			
			if(GetOption(K_Custom_Sidebars) == 0)
			{
				color = SampleInputLocation(2, gridL > 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));
			}
			else if(GetOption(K_Custom_Sidebars) == 1)
			{
				color = SampleInputLocation(3, gridL > 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));
			}
			else
			{
				color = SampleInputLocation(4, gridL > 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));
			}
		}
		else
		{
			float gridy;
			float gridx;
			
			if(GetOption(M_Downscaling_Support) == 0)
			{
				gridy = floor(texcoord.y*(GetResolution().y));
				gridx = floor(texcoord.x*(GetResolution().x));
			}
			else if(GetOption(M_Downscaling_Support) == 1)
			{
				gridy = floor(texcoord.y*(1080.0));
				gridx = floor(texcoord.x*(1080.0));
			}
			else
			{
				gridy = floor(texcoord.y*(1081.0));
				gridx = floor(texcoord.x*(1081.0));
			}
			
			if(GetOption(K_Custom_Sidebars) == 0)
			{
				color = SampleInputLocation(2, (int(gridy+gridx) & 1) < 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));				
			}
			else if(GetOption(K_Custom_Sidebars) == 1)
			{
				color = SampleInputLocation(3, (int(gridy+gridx) & 1) < 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));
			}
			else
			{
				color = SampleInputLocation(4, (int(gridy+gridx) & 1) < 0.5 ? float2((texcoord.x + P) + DepthL * D , texcoord.y) : float2((texcoord.x - P) - DepthR * D , texcoord.y));
			}
		}
	}
	SetOutput(color);
}

