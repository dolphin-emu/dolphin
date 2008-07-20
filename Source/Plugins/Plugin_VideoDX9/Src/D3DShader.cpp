#include <d3dx9.h>
#include <string>

#include "Globals.h"
#include "D3DShader.h"


namespace D3D
{
	LPDIRECT3DVERTEXSHADER9 CompileVShader(const char *code, int len)
	{
		//try to compile
		LPD3DXBUFFER shaderBuffer = 0, errorBuffer = 0;
		LPDIRECT3DVERTEXSHADER9 vShader = 0;
	 	HRESULT hr = D3DXCompileShader(code,len,0,0,"main","vs_1_1",0,&shaderBuffer,&errorBuffer,0);
		if (FAILED(hr))
		{
			//let's try 2.0
			hr = D3DXCompileShader(code,len,0,0,"main","vs_2_0",0,&shaderBuffer,&errorBuffer,0);
		}

		if (FAILED(hr))
		{
			//compilation error
			std::string hello = (char*)errorBuffer->GetBufferPointer();
			hello += "\n\n";
			hello += code;
			if (g_Config.bShowShaderErrors)
				MessageBox(0, hello.c_str(), "Error compiling vertex shader", MB_ICONERROR);
			vShader = 0;
		}
		else if (SUCCEEDED(hr))
		{
			//create it
			HRESULT hr = E_FAIL;
			if (shaderBuffer)
				hr = D3D::dev->CreateVertexShader((DWORD *)shaderBuffer->GetBufferPointer(), &vShader);
			if (FAILED(hr) || vShader == 0)
			{
				if (g_Config.bShowShaderErrors)
					MessageBox(0,code,(char*)errorBuffer->GetBufferPointer(),MB_ICONERROR);
			}
		}

		//cleanup
		if (shaderBuffer)
			shaderBuffer->Release();
		if (errorBuffer)
			errorBuffer->Release();

		return vShader;
	}

	LPDIRECT3DPIXELSHADER9 CompilePShader(const char *code, int len)
	{
		LPD3DXBUFFER shaderBuffer = 0, errorBuffer = 0;
		LPDIRECT3DPIXELSHADER9 pShader = 0;
		static char *versions[6] = {"ERROR","ps_1_1","ps_1_4","ps_2_0","ps_3_0","ps_4_0"};
		HRESULT hr = D3DXCompileShader(code,len,0,0,
			"main","ps_2_0", // Pixel Shader 2.0 is enough for all we do
			0,&shaderBuffer,&errorBuffer,0);

		if (FAILED(hr))
		{
			std::string hello = (char*)errorBuffer->GetBufferPointer();
			hello += "\n\n";
			hello += code;
			if (g_Config.bShowShaderErrors)
				MessageBox(0, hello.c_str(), "Error compiling pixel shader", MB_ICONERROR);
			pShader = 0;
		}
		else 
		{
			//create it
			HRESULT hr = D3D::dev->CreatePixelShader((DWORD *)shaderBuffer->GetBufferPointer(), &pShader);
			if (FAILED(hr) || pShader == 0)
			{
				if (g_Config.bShowShaderErrors)
					MessageBox(0,"damn","error creating pixelshader",MB_ICONERROR);
			}
		}

		//cleanup
		if (shaderBuffer)
			shaderBuffer->Release();
		if (errorBuffer)
			errorBuffer->Release();

		return pShader;
	}
}
