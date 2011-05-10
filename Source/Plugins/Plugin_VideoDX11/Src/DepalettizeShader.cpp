// DepalettizeShader.cpp
// Nolan Check
// Created 4/29/2011

#include "DepalettizeShader.h"

#include "D3DShader.h"
#include "D3DTexture.h"
#include "FramebufferManager.h"
#include "GfxState.h"
#include "Render.h"

namespace DX11
{
	
static const char DEPALETTIZE_SHADER[] =
"// dolphin-emu depalettizing shader\n"

// If NUM_COLORS is 0, Base is assumed to have UINT type.
// Otherwise, base is assumed to have UNORM type and NUM_COLORS is the number
// of colors in the palette.
"#ifndef NUM_COLORS\n"
"#error NUM_COLORS was not defined\n"
"#endif\n"

"#if NUM_COLORS == 0\n"
"Texture2D<uint> Base : register(t0);\n"
"#else\n"
"Texture2D<float4> Base : register(t0);\n"
"#endif\n"

"Buffer<float4> Palette : register(t1);\n"

"void main(out float4 ocol0 : SV_Target, in float4 pos : SV_Position)\n"
"{\n"
"#if NUM_COLORS == 0\n"
	"uint sample = Base.Load(int3(pos.xy, 0));\n"
	"ocol0 = Palette.Load(sample);\n"
"#else\n"
	"float sample = Base.Load(int3(pos.xy, 0)).r;\n"
	"ocol0 = Palette.Load(sample * (NUM_COLORS-1));\n"
"#endif\n"
"}\n"
;

void DepalettizeShader::Depalettize(D3DTexture2D* dstTex, D3DTexture2D* baseTex,
	BaseType baseType, ID3D11ShaderResourceView* paletteSRV)
{
	SharedPtr<ID3D11PixelShader> shader = GetShader(baseType);
	if (!shader)
	{
		ERROR_LOG(VIDEO, "Failed to create depalettizer shader");
		return;
	}

	g_renderer->ResetAPIState();
	D3D::stateman->Apply();

	D3D11_TEXTURE2D_DESC dstDesc;
	dstTex->GetTex()->GetDesc(&dstDesc);

	D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(dstDesc.Width), FLOAT(dstDesc.Height));
	D3D::g_context->RSSetViewports(1, &vp);

	D3D::g_context->OMSetRenderTargets(1, &dstTex->GetRTV(), NULL);
	D3D::g_context->PSSetShaderResources(0, 1, &baseTex->GetSRV());
	D3D::g_context->PSSetShaderResources(1, 1, &paletteSRV);

	D3D::drawEncoderQuad(shader);

	ID3D11ShaderResourceView* nullDummy = NULL;
	D3D::g_context->PSSetShaderResources(0, 1, &nullDummy);
	D3D::g_context->PSSetShaderResources(1, 1, &nullDummy);

	g_renderer->RestoreAPIState();
	D3D::g_context->OMSetRenderTargets(1,
		&FramebufferManager::GetEFBColorTexture()->GetRTV(),
		FramebufferManager::GetEFBDepthTexture()->GetDSV());
}

SharedPtr<ID3D11PixelShader> DepalettizeShader::GetShader(BaseType baseType)
{
	switch (baseType)
	{

	case Uint:
		if (!m_uintShader)
		{
			D3D_SHADER_MACRO macros[] = {
				{ "NUM_COLORS", "0" },
				{ NULL, NULL }
			};
			m_uintShader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_SHADER,
				sizeof(DEPALETTIZE_SHADER), macros);
		}
		return m_uintShader;

	case Unorm4:
		if (!m_unorm4Shader)
		{
			D3D_SHADER_MACRO macros[] = {
				{ "NUM_COLORS", "16" },
				{ NULL, NULL }
			};
			m_unorm4Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_SHADER,
				sizeof(DEPALETTIZE_SHADER), macros);
		}
		return m_unorm4Shader;

	case Unorm8:
		if (!m_unorm8Shader)
		{
			D3D_SHADER_MACRO macros[] = {
				{ "NUM_COLORS", "256" },
				{ NULL, NULL }
			};
			m_unorm8Shader = D3D::CompileAndCreatePixelShader(DEPALETTIZE_SHADER,
				sizeof(DEPALETTIZE_SHADER), macros);
		}
		return m_unorm8Shader;

	default:
		_assert_msg_(VIDEO, 0, "Invalid depalettizer base type");
		return SharedPtr<ID3D11PixelShader>();

	}
}

}
