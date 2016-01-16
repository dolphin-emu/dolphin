// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cctype>
#include <list>
#include <string>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoBackends/D3D12/D3DUtil.h"

#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"

namespace DX12
{

extern D3D12_BLEND_DESC g_reset_blend_desc;
extern D3D12_DEPTH_STENCIL_DESC g_reset_depth_desc;
extern D3D12_RASTERIZER_DESC g_reset_rast_desc;

namespace D3D
{

unsigned int AlignValue(unsigned int value, unsigned int alignment)
{
	return (value + (alignment - 1)) & ~(alignment - 1);
}

void ResourceBarrier(ID3D12GraphicsCommandList* command_list, ID3D12Resource* resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, UINT subresource)
{
	if (state_before == state_after)
		return;

	CHECK(resource, "NULL resource passed to ResourceBarrier.");

	D3D12_RESOURCE_BARRIER resourceBarrierDesc = {
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, // D3D12_RESOURCE_TRANSITION_BARRIER_DESC Transition
		D3D12_RESOURCE_BARRIER_FLAG_NONE,       // D3D12_RESOURCE_BARRIER_FLAGS Flags

		// D3D12_RESOURCE_TRANSITION_BARRIER_DESC Transition
		{
			resource,    // ID3D12Resource *pResource;
			subresource, // UINT Subresource;
			state_before, // UINT StateBefore;
			state_after   // UINT StateAfter;
		}
	};

	command_list->ResourceBarrier(1, &resourceBarrierDesc);
}

// Ring buffer class, shared between the draw* functions
class UtilVertexBuffer
{
public:
	explicit UtilVertexBuffer(int size) : m_max_size(size)
	{
		CheckHR(
			device12->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(m_max_size),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_buf12)
				)
			);

		// Map buffer for CPU access upon creation.
		// On D3D12, CPU-visible resources are persistently mapped.

		CheckHR(m_buf12->Map(0, nullptr, &m_buf12_data));
	}
	~UtilVertexBuffer()
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_buf12);
	}

	int GetSize() const { return m_max_size; }

	int GetRoomLeftInBuffer() const { return m_max_size - m_offset; }

	// returns vertex offset to the new data
	int AppendData(void* data, int size, int vertex_size)
	{
		if (m_offset + size >= m_max_size)
		{
			// Wrap buffer around.
			m_offset = 0;
		}

		m_offset = ((m_offset +vertex_size-1)/vertex_size)*vertex_size; // align offset to vertex_size bytes

		memcpy(static_cast<u8*>(m_buf12_data) + m_offset, data, size);

		m_offset += size;
		return (m_offset - size) / vertex_size;
	}

	int BeginAppendData(void** write_ptr, int size, int vertex_size)
	{
		_dbg_assert_(VIDEO, size < m_max_size);

		int aligned_offset = ((m_offset + vertex_size - 1) / vertex_size) * vertex_size; // align offset to vertex_size bytes
		if (aligned_offset + size > m_max_size)
		{
			// wrap buffer around and notify observers
			m_offset = 0;
			aligned_offset = 0;
		}

		*write_ptr = reinterpret_cast<byte*>(m_buf12_data) + aligned_offset;
		m_offset = aligned_offset + size;
		return aligned_offset / vertex_size;
	}

	void EndAppendData()
	{
		// No-op on DX12.
	}

	ID3D12Resource* GetBuffer12()
	{
		return m_buf12;
	}

private:
	ID3D12Resource* m_buf12 = nullptr;
	void* m_buf12_data = nullptr;

	int m_offset = 0;
	int m_max_size = 0;
};

CD3DFont font;
UtilVertexBuffer* util_vbuf_stq = nullptr;
UtilVertexBuffer* util_vbuf_cq = nullptr;
UtilVertexBuffer* util_vbuf_clearq = nullptr;
UtilVertexBuffer* util_vbuf_efbpokequads = nullptr;

static const unsigned int s_max_num_vertices = 8000 * 6;

struct FONT2DVERTEX
{
	float x, y, z;
	float col[4];
	float tu, tv;
};

FONT2DVERTEX InitFont2DVertex(float x, float y, u32 color, float tu, float tv)
{
	FONT2DVERTEX v;   v.x=x; v.y=y; v.z=0;  v.tu = tu; v.tv = tv;
	v.col[0] = ((float)((color >> 16) & 0xFF)) / 255.f;
	v.col[1] = ((float)((color >>  8) & 0xFF)) / 255.f;
	v.col[2] = ((float)((color >>  0) & 0xFF)) / 255.f;
	v.col[3] = ((float)((color >> 24) & 0xFF)) / 255.f;
	return v;
}

CD3DFont::CD3DFont()
{
}

constexpr const char fontpixshader[] = {
	"Texture2D tex2D;\n"
	"SamplerState linearSampler\n"
	"{\n"
	"	Filter = MIN_MAG_MIP_LINEAR;\n"
	"	AddressU = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	AddressV = D3D11_TEXTURE_ADDRESS_BORDER;\n"
	"	BorderColor = float4(0.f, 0.f, 0.f, 0.f);\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"float4 main( PS_INPUT input ) : SV_Target\n"
	"{\n"
	"	return tex2D.Sample( linearSampler, input.tex ) * input.col;\n"
	"};\n"
};

constexpr const char fontvertshader[] = {
	"struct VS_INPUT\n"
	"{\n"
	"	float4 pos : POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"struct PS_INPUT\n"
	"{\n"
	"	float4 pos : SV_POSITION;\n"
	"	float4 col : COLOR;\n"
	"	float2 tex : TEXCOORD;\n"
	"};\n"
	"PS_INPUT main( VS_INPUT input )\n"
	"{\n"
	"	PS_INPUT output;\n"
	"	output.pos = input.pos;\n"
	"	output.col = input.col;\n"
	"	output.tex = input.tex;\n"
	"	return output;\n"
	"};\n"
};

int CD3DFont::Init()
{
	// Create vertex buffer for the letters

	// Prepare to create a bitmap
	unsigned int* pBitmapBits;
	BITMAPINFO bmi;
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       =  (int)m_tex_width;
	bmi.bmiHeader.biHeight      = -(int)m_tex_height;
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount    = 32;

	// Create a DC and a bitmap for the font
	HDC hDC = CreateCompatibleDC(nullptr);
	HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBitmapBits), nullptr, 0);
	SetMapMode(hDC, MM_TEXT);

	// create a GDI font
	HFONT hFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE,
							FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
							VARIABLE_PITCH, _T("Tahoma"));

	if (nullptr == hFont)
		return E_FAIL;

	HGDIOBJ hOldbmBitmap = SelectObject(hDC, hbmBitmap);
	HGDIOBJ hOldFont = SelectObject(hDC, hFont);

	// Set text properties
	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor  (hDC, 0);
	SetTextAlign(hDC, TA_TOP);

	TEXTMETRICW tm;
	GetTextMetricsW(hDC, &tm);
	m_line_height = tm.tmHeight;

	// Loop through all printable characters and output them to the bitmap
	// Meanwhile, keep track of the corresponding tex coords for each character.
	int x = 0, y = 0;
	char str[2] = "\0";
	for (int c = 0; c < 127 - 32; c++)
	{
		str[0] = c + 32;
		SIZE size;
		GetTextExtentPoint32A(hDC, str, 1, &size);
		if ((int)(x + size.cx + 1) > m_tex_width)
		{
			x  = 0;
			y += m_line_height;
		}

		ExtTextOutA(hDC, x+1, y+0, ETO_OPAQUE | ETO_CLIPPED, nullptr, str, 1, nullptr);
		m_tex_coords[c][0] = ((float)(x + 0))/m_tex_width;
		m_tex_coords[c][1] = ((float)(y + 0))/m_tex_height;
		m_tex_coords[c][2] = ((float)(x + 0 + size.cx))/m_tex_width;
		m_tex_coords[c][3] = ((float)(y + 0 + size.cy))/m_tex_height;

		x += size.cx + 3;  // 3 to work around annoying ij conflict (part of the j ends up with the i)
	}

	// Create a new texture for the font
	// possible optimization: store the converted data in a buffer and fill the texture on creation.
	// That way, we can use a static texture
	std::unique_ptr<byte[]> texInitialData(new byte[m_tex_width * m_tex_height * 4]);

	for (y = 0; y < m_tex_height; y++)
	{
		u32* pDst32_12 = reinterpret_cast<u32*>(static_cast<u8*>(texInitialData.get()) + y * m_tex_width * 4);
		for (x = 0; x < m_tex_width; x++)
		{
			const u8 bAlpha = (pBitmapBits[m_tex_width * y + x] & 0xff);

			*pDst32_12++ = (((bAlpha << 4) | bAlpha) << 24) | 0xFFFFFF;
		}
	}

	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_tex_width, m_tex_height, 1, 1),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_texture12)
		)
		);

	D3D::SetDebugObjectName12(m_texture12, "texture of a CD3DFont object");

	ID3D12Resource* temporaryFontTextureUploadBuffer;
	CheckHR(
		D3D::device12->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(AlignValue(m_tex_width * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * m_tex_height),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&temporaryFontTextureUploadBuffer)
		)
		);

	D3D12_SUBRESOURCE_DATA subresourceDataDesc12 = {
		texInitialData.get(), // const void *pData;
		m_tex_width * 4,      // LONG_PTR RowPitch;
		0                     // LONG_PTR SlicePitch;
	};

	D3D::ResourceBarrier(D3D::current_command_list, m_texture12, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	CHECK(0 != UpdateSubresources(D3D::current_command_list, m_texture12, temporaryFontTextureUploadBuffer, 0, 0, 1, &subresourceDataDesc12), "UpdateSubresources call failed.");

	command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(temporaryFontTextureUploadBuffer);

	texInitialData.release();

	D3D::gpu_descriptor_heap_mgr->Allocate(&m_texture12_cpu, &m_texture12_gpu);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = -1;

	D3D::device12->CreateShaderResourceView(m_texture12, &srvDesc, m_texture12_cpu);

	D3D::ResourceBarrier(D3D::current_command_list, m_texture12, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	SelectObject(hDC, hOldbmBitmap);
	DeleteObject(hbmBitmap);

	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	// setup device objects for drawing
	D3DBlob* psbytecode = nullptr;
	D3D::CompilePixelShader(fontpixshader, &psbytecode);
	if (psbytecode == nullptr)
		PanicAlert("Failed to compile pixel shader, %s %d\n", __FILE__, __LINE__);

	m_pshader12.pShaderBytecode = psbytecode->Data();
	m_pshader12.BytecodeLength = psbytecode->Size();

	D3DBlob* vsbytecode = nullptr;
	D3D::CompileVertexShader(fontvertshader, &vsbytecode);
	if (vsbytecode == nullptr)
		PanicAlert("Failed to compile vertex shader, %s %d\n", __FILE__, __LINE__);

	m_vshader12.pShaderBytecode = vsbytecode->Data();
	m_vshader12.BytecodeLength = vsbytecode->Size();

	const D3D12_INPUT_ELEMENT_DESC desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	m_input_layout12.NumElements = ARRAYSIZE(desc);
	m_input_layout12.pInputElementDescs = desc;

	D3D12_BLEND_DESC blenddesc = {};
	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = TRUE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blenddesc.RenderTarget[0].LogicOpEnable = FALSE;
	m_blendstate12 = blenddesc;

	D3D12_RASTERIZER_DESC rastdesc = { D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false, 0, 0.f, 0.f, false, false, false, false };
	m_raststate12 = rastdesc;

	UINT textVBSize = s_max_num_vertices * sizeof(FONT2DVERTEX);

	CheckHR(
		device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(textVBSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vb12)
			)
		);

	SetDebugObjectName12(m_vb12, "vertex buffer of a CD3DFont object");

	m_vb12_view.BufferLocation = m_vb12->GetGPUVirtualAddress();
	m_vb12_view.SizeInBytes = textVBSize;
	m_vb12_view.StrideInBytes = sizeof(FONT2DVERTEX);

	CheckHR(m_vb12->Map(0, nullptr, &m_vb12_data));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC textPsoDesc = {
		default_root_signature,                           // ID3D12RootSignature *pRootSignature;
		{ vsbytecode->Data(), vsbytecode->Size() },       // D3D12_SHADER_BYTECODE VS;
		{ psbytecode->Data(), psbytecode->Size() },       // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		{},                                               // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		blenddesc,                                        // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		rastdesc,                                         // D3D12_RASTERIZER_DESC RasterizerState
		CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),        // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		m_input_layout12,                                 // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_UNKNOWN,                              // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(&textPsoDesc, &m_pso));

	SAFE_RELEASE(psbytecode);
	SAFE_RELEASE(vsbytecode);

	return S_OK;
}

int CD3DFont::Shutdown()
{
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_vb12);
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_texture12);

	return S_OK;
}

int CD3DFont::DrawTextScaled(float x, float y, float size, float spacing, u32 dwColor, const std::string& text)
{
	if (!m_vb12)
		return 0;

	UINT stride = sizeof(FONT2DVERTEX);
	UINT bufoffset = 0;

	float scale_x = 1 / (float)D3D::GetBackBufferWidth() * 2.f;
	float scale_y = 1 / (float)D3D::GetBackBufferHeight() * 2.f;
	float sizeratio = size / (float)m_line_height;

	// translate starting positions
	float sx = x * scale_x - 1.f;
	float sy = 1.f - y * scale_y;

	// Fill vertex buffer
	FONT2DVERTEX* vertices12 = static_cast<FONT2DVERTEX*>(m_vb12_data) + m_vb12_offset / sizeof(FONT2DVERTEX);
	int num_triangles = 0L;

	// set general pipeline state
	D3D::current_command_list->SetPipelineState(m_pso);
	D3D::command_list_mgr->m_dirty_pso = true;

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	D3D::command_list_mgr->m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, m_texture12_gpu);

	// If we are close to running off edge of vertex buffer, jump back to beginning.
	if (m_vb12_offset + text.length() * 6 * sizeof(FONT2DVERTEX) >= s_max_num_vertices * sizeof(FONT2DVERTEX))
	{
		m_vb12_offset = 0;
		vertices12 = static_cast<FONT2DVERTEX*>(m_vb12_data);
	}

	float start_x = sx;
	for (char c : text)
	{
		if (c == '\n')
		{
			sx  = start_x;
			sy -= scale_y * size;
		}
		if (!std::isprint(c))
			continue;

		c -= 32;
		float tx1 = m_tex_coords[c][0];
		float ty1 = m_tex_coords[c][1];
		float tx2 = m_tex_coords[c][2];
		float ty2 = m_tex_coords[c][3];

		float w = (float)(tx2-tx1) * m_tex_width * scale_x * sizeratio;
		float h = (float)(ty1-ty2) * m_tex_height * scale_y * sizeratio;

		FONT2DVERTEX v[6];
		v[0] = InitFont2DVertex(sx,   sy+h, dwColor, tx1, ty2);
		v[1] = InitFont2DVertex(sx,   sy,   dwColor, tx1, ty1);
		v[2] = InitFont2DVertex(sx+w, sy+h, dwColor, tx2, ty2);
		v[3] = InitFont2DVertex(sx+w, sy,   dwColor, tx2, ty1);
		v[4] = v[2];
		v[5] = v[1];

		memcpy(vertices12, v, 6 * sizeof(FONT2DVERTEX));
		vertices12 += 6;

		num_triangles += 2;

		sx += w + spacing * scale_x * size;
	}

	// Render the vertex buffer
	if (num_triangles > 0)
	{
		D3D::current_command_list->IASetVertexBuffers(0, 1, &m_vb12_view);

		D3D::current_command_list->DrawInstanced(3 * num_triangles, 1, m_vb12_offset / sizeof(FONT2DVERTEX), 0);
	}

	m_vb12_offset += 3 * num_triangles * sizeof(FONT2DVERTEX);

	return S_OK;
}

D3D12_CPU_DESCRIPTOR_HANDLE linear_copy_sampler12CPU;
D3D12_GPU_DESCRIPTOR_HANDLE linear_copy_sampler12GPU;
D3D12_CPU_DESCRIPTOR_HANDLE point_copy_sampler12CPU;
D3D12_GPU_DESCRIPTOR_HANDLE point_copy_sampler12GPU;

struct STQVertex
{
	float x, y, z, u, v, w, g;
};
struct ClearVertex
{
	float x, y, z;
	u32 col;
};

struct ColVertex
{
	float x, y, z;
	u32 col;
};

struct
{
	float u1, v1, u2, v2, S, G;
} tex_quad_data;

struct
{
	float x1, y1, x2, y2, z;
	u32 col;
} draw_quad_data;

struct
{
	u32 col;
	float z;
} clear_quad_data;

// ring buffer offsets
int stq_offset;
int cq_offset;
int clearq_offset;

void InitUtils()
{
	util_vbuf_stq = new UtilVertexBuffer(0x10000);
	util_vbuf_cq = new UtilVertexBuffer(0x10000);
	util_vbuf_clearq = new UtilVertexBuffer(0x10000);
	util_vbuf_efbpokequads = new UtilVertexBuffer(0x100000);

	D3D12_SAMPLER_DESC point_sampler_desc = {
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.f,
		1,
		D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.f, 0.f, 0.f, 0.f },
		0.f,
		0.f
	};

	D3D::sampler_descriptor_heap_mgr->Allocate(&point_copy_sampler12CPU, &point_copy_sampler12GPU);
	D3D::device12->CreateSampler(&point_sampler_desc, point_copy_sampler12CPU);

	D3D12_SAMPLER_DESC linear_sampler_desc = {
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.f,
		1,
		D3D12_COMPARISON_FUNC_ALWAYS,
		{ 0.f, 0.f, 0.f, 0.f },
		0.f,
		0.f
	};

	D3D::sampler_descriptor_heap_mgr->Allocate(&linear_copy_sampler12CPU, &linear_copy_sampler12GPU);
	D3D::device12->CreateSampler(&linear_sampler_desc, linear_copy_sampler12CPU);

	// cached data used to avoid unnecessarily reloading the vertex buffers
	memset(&tex_quad_data, 0, sizeof(tex_quad_data));
	memset(&draw_quad_data, 0, sizeof(draw_quad_data));
	memset(&clear_quad_data, 0, sizeof(clear_quad_data));

	font.Init();
}

void ShutdownUtils()
{
	font.Shutdown();

	SAFE_DELETE(util_vbuf_stq);
	SAFE_DELETE(util_vbuf_cq);
	SAFE_DELETE(util_vbuf_clearq);
	SAFE_DELETE(util_vbuf_efbpokequads);
}

void SetPointCopySampler()
{
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, point_copy_sampler12GPU);
	D3D::command_list_mgr->m_dirty_samplers = true;
}

void SetLinearCopySampler()
{
	D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SAMPLER, linear_copy_sampler12GPU);
	D3D::command_list_mgr->m_dirty_samplers = true;
}

void DrawShadedTexQuad(D3DTexture2D* texture,
	const D3D12_RECT* rSource,
	int source_width,
	int source_height,
	D3D12_SHADER_BYTECODE pshader12,
	D3D12_SHADER_BYTECODE vshader12,
	D3D12_INPUT_LAYOUT_DESC layout12,
	D3D12_SHADER_BYTECODE gshader12,
	float gamma,
	u32 slice,
	DXGI_FORMAT rt_format,
	bool inherit_srv_binding,
	bool rt_multisampled,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc_override
	)
{
	float sw = 1.0f / (float)source_width;
	float sh = 1.0f / (float)source_height;
	float u1 = ((float)rSource->left) * sw;
	float u2 = ((float)rSource->right) * sw;
	float v1 = ((float)rSource->top) * sh;
	float v2 = ((float)rSource->bottom) * sh;
	float S = (float)slice;
	float G = 1.0f / gamma;

	STQVertex coords[4] = {
		{ -1.0f, 1.0f, 0.0f, u1, v1, S, G },
		{ 1.0f, 1.0f, 0.0f, u2, v1, S, G },
		{ -1.0f, -1.0f, 0.0f, u1, v2, S, G },
		{ 1.0f, -1.0f, 0.0f, u2, v2, S, G },
	};

	// only upload the data to VRAM if it changed
	if (tex_quad_data.u1 != u1 || tex_quad_data.v1 != v1 ||
		tex_quad_data.u2 != u2 || tex_quad_data.v2 != v2 ||
		tex_quad_data.S != S || tex_quad_data.G != G)
	{
		stq_offset = util_vbuf_stq->AppendData(coords, sizeof(coords), sizeof(STQVertex));

		tex_quad_data.u1 = u1;
		tex_quad_data.v1 = v1;
		tex_quad_data.u2 = u2;
		tex_quad_data.v2 = v2;
		tex_quad_data.S = S;
		tex_quad_data.G = G;
	}

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::command_list_mgr->m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_stq->GetBuffer12()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		0x10000,                                              // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(STQVertex)                                     // UINT StrideInBytes;
	};

	D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
	D3D::command_list_mgr->m_dirty_vertex_buffer = true;

	if (!inherit_srv_binding)
	{
		texture->TransitionToResourceState(D3D::current_command_list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV, texture->GetSRV12GPU());
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		default_root_signature,                           // ID3D12RootSignature *pRootSignature;
		vshader12,                                        // D3D12_SHADER_BYTECODE VS;
		pshader12,                                        // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		gshader12,                                        // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		g_reset_blend_desc,                               // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		g_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		depth_stencil_desc_override ?
			*depth_stencil_desc_override :
			g_reset_depth_desc,                           // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		layout12,                                         // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ rt_format },                                    // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                            // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(&pso_desc, &pso));

	D3D::current_command_list->SetPipelineState(pso);
	D3D::command_list_mgr->m_dirty_pso = true;

	// In D3D11, the 'resetraststate' has ScissorEnable disabled. In D3D12, scissor testing is always enabled.
	// Thus, set the scissor rect to the max texture size, then reset it to the current scissor rect to avoid
	// dirtying state.

	// 2 ^ D3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP = 131072
	D3D::current_command_list->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, 131072, 131072));

	D3D::current_command_list->DrawInstanced(4, 1, stq_offset, 0);

	g_renderer->RestoreAPIState();
}

// Fills a certain area of the current render target with the specified color
// destination coordinates normalized to (-1;1)
void DrawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2, D3D12_BLEND_DESC* blend_desc, D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc, bool rt_multisampled)
{
	ColVertex coords[4] = {
		{ x1, y2, z, Color },
		{ x2, y2, z, Color },
		{ x1, y1, z, Color },
		{ x2, y1, z, Color },
	};

	if (draw_quad_data.x1 != x1 || draw_quad_data.y1 != y1 ||
	    draw_quad_data.x2 != x2 || draw_quad_data.y2 != y2 ||
	    draw_quad_data.col != Color || draw_quad_data.z != z)
	{
		cq_offset = util_vbuf_cq->AppendData(coords, sizeof(coords), sizeof(ColVertex));

		draw_quad_data.x1 = x1;
		draw_quad_data.y1 = y1;
		draw_quad_data.x2 = x2;
		draw_quad_data.y2 = y2;
		draw_quad_data.col = Color;
		draw_quad_data.z = z;
	}

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::command_list_mgr->m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_cq->GetBuffer12()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		0x10000,                                             // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(ColVertex)                                    // UINT StrideInBytes;
	};

	D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
	D3D::command_list_mgr->m_dirty_vertex_buffer = true;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		default_root_signature,                           // ID3D12RootSignature *pRootSignature;
		StaticShaderCache::GetClearVertexShader(),        // D3D12_SHADER_BYTECODE VS;
		StaticShaderCache::GetClearPixelShader(),         // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		StaticShaderCache::GetClearGeometryShader(),      // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		*blend_desc,                                      // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		g_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		*depth_stencil_desc,                              // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		StaticShaderCache::GetClearVertexShaderInputLayout(), // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                            // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(&pso_desc, &pso));

	D3D::current_command_list->SetPipelineState(pso);
	D3D::command_list_mgr->m_dirty_pso = true;

	// In D3D11, the 'resetraststate' has ScissorEnable disabled. In D3D12, scissor testing is always enabled.
	// Thus, set the scissor rect to the max texture size, then reset it to the current scissor rect to avoid
	// dirtying state.

	// 2 ^ D3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP = 131072
	D3D::current_command_list->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, 131072, 131072));

	D3D::current_command_list->DrawInstanced(4, 1, cq_offset, 0);

	g_renderer->RestoreAPIState();
}

void DrawClearQuad(u32 Color, float z, D3D12_BLEND_DESC* blend_desc, D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc, bool rt_multisampled)
{
	ClearVertex coords[4] = {
		{-1.0f,  1.0f, z, Color},
		{ 1.0f,  1.0f, z, Color},
		{-1.0f, -1.0f, z, Color},
		{ 1.0f, -1.0f, z, Color},
	};

	if (clear_quad_data.col != Color || clear_quad_data.z != z)
	{
		clearq_offset = util_vbuf_clearq->AppendData(coords, sizeof(coords), sizeof(ClearVertex));

		clear_quad_data.col = Color;
		clear_quad_data.z = z;
	}

	D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	D3D::command_list_mgr->m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_clearq->GetBuffer12()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		0x10000,                                                 // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(ClearVertex)                                      // UINT StrideInBytes;
	};

	D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
	D3D::command_list_mgr->m_dirty_vertex_buffer = true;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		default_root_signature,                           // ID3D12RootSignature *pRootSignature;
		StaticShaderCache::GetClearVertexShader(),        // D3D12_SHADER_BYTECODE VS;
		StaticShaderCache::GetClearPixelShader(),         // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		g_ActiveConfig.iStereoMode > 0 ?
		StaticShaderCache::GetClearGeometryShader() :
		D3D12_SHADER_BYTECODE(),                          // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		*blend_desc,                                      // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		g_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		*depth_stencil_desc,                              // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		StaticShaderCache::GetClearVertexShaderInputLayout(), // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                            // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(&pso_desc, &pso));

	D3D::current_command_list->SetPipelineState(pso);
	D3D::command_list_mgr->m_dirty_pso = true;

	// In D3D11, the 'resetraststate' has ScissorEnable disabled. In D3D12, scissor testing is always enabled.
	// Thus, set the scissor rect to the max texture size, then reset it to the current scissor rect to avoid
	// dirtying state.

	// 2 ^ D3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP = 131072
	D3D::current_command_list->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, 131072, 131072));

	D3D::current_command_list->DrawInstanced(4, 1, clearq_offset, 0);

	g_renderer->RestoreAPIState();
}

static void InitColVertex(ColVertex* vert, float x, float y, float z, u32 col)
{
	vert->x = x;
	vert->y = y;
	vert->z = z;
	vert->col = col;
}

void DrawEFBPokeQuads(EFBAccessType type,
	const EfbPokeData* points,
	size_t num_points,
	D3D12_BLEND_DESC* blend_desc,
	D3D12_DEPTH_STENCIL_DESC* depth_stencil_desc,
	D3D12_VIEWPORT* viewport,
	D3D12_CPU_DESCRIPTOR_HANDLE* render_target,
	D3D12_CPU_DESCRIPTOR_HANDLE* depth_buffer,
	bool rt_multisampled
	)
{
	// The viewport and RT/DB are passed in so we can reconstruct the state if we need to execute in the middle of building the vertex buffer.

	D3D::command_list_mgr->m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D12_VERTEX_BUFFER_VIEW vb_view = {
		util_vbuf_efbpokequads->GetBuffer12()->GetGPUVirtualAddress(), // D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
		0x100000,                                                      // UINT SizeInBytes; This is the size of the entire buffer, not just the size of the vertex data for one draw call, since the offsetting is done in the draw call itself.
		sizeof(ColVertex)                                              // UINT StrideInBytes;
	};

	D3D::command_list_mgr->m_dirty_vertex_buffer = true;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
		default_root_signature,                           // ID3D12RootSignature *pRootSignature;
		StaticShaderCache::GetClearVertexShader(),        // D3D12_SHADER_BYTECODE VS;
		StaticShaderCache::GetClearPixelShader(),         // D3D12_SHADER_BYTECODE PS;
		{},                                               // D3D12_SHADER_BYTECODE DS;
		{},                                               // D3D12_SHADER_BYTECODE HS;
		g_ActiveConfig.iStereoMode > 0 ?
		StaticShaderCache::GetClearGeometryShader() :
		D3D12_SHADER_BYTECODE(),                          // D3D12_SHADER_BYTECODE GS;
		{},                                               // D3D12_STREAM_OUTPUT_DESC StreamOutput
		*blend_desc,                                      // D3D12_BLEND_DESC BlendState;
		UINT_MAX,                                         // UINT SampleMask;
		g_reset_rast_desc,                                // D3D12_RASTERIZER_DESC RasterizerState
		*depth_stencil_desc,                              // D3D12_DEPTH_STENCIL_DESC DepthStencilState
		StaticShaderCache::GetClearVertexShaderInputLayout(), // D3D12_INPUT_LAYOUT_DESC InputLayout
		D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF,        // D3D12_INDEX_BUFFER_PROPERTIES IndexBufferProperties
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,           // D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType
		1,                                                // UINT NumRenderTargets
		{ DXGI_FORMAT_R8G8B8A8_UNORM },                   // DXGI_FORMAT RTVFormats[8]
		DXGI_FORMAT_D32_FLOAT,                            // DXGI_FORMAT DSVFormat
		{ 1 /* UINT Count */, 0 /* UINT Quality */ }      // DXGI_SAMPLE_DESC SampleDesc
	};

	if (rt_multisampled)
	{
		pso_desc.SampleDesc.Count = g_ActiveConfig.iMultisamples;
	}

	ID3D12PipelineState* pso = nullptr;
	CheckHR(DX12::gx_state_cache.GetPipelineStateObjectFromCache(&pso_desc, &pso));

	D3D::command_list_mgr->m_dirty_pso = true;

	// If drawing a large number of points at once, this will have to be split into multiple passes.
	const size_t COL_QUAD_SIZE = sizeof(ColVertex) * 6;
	size_t points_per_draw = util_vbuf_efbpokequads->GetSize() / COL_QUAD_SIZE;

	// Makes sure we aren't about to run out of room in our buffer. If so, wait for GPU to finish current work,
	// then the BeginAppendData function below will automatically roll over to beginning of buffer.
	if (util_vbuf_efbpokequads->GetRoomLeftInBuffer() < points_per_draw * COL_QUAD_SIZE)
	{
		D3D::command_list_mgr->ExecuteQueuedWork(true);
	}

	size_t current_point_index = 0;

	while (current_point_index < num_points)
	{
		// Corresponding dirty flags set outside loop.
		D3D::current_command_list->OMSetRenderTargets(1, render_target, FALSE, depth_buffer);
		D3D::current_command_list->RSSetViewports(1, viewport);
		D3D::current_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		D3D::current_command_list->IASetVertexBuffers(0, 1, &vb_view);
		D3D::current_command_list->SetPipelineState(pso);

		// Disable scissor testing.
		D3D::current_command_list->RSSetScissorRects(1, &CD3DX12_RECT(0, 0, 131072, 131072));

		size_t points_to_draw = std::min(num_points - current_point_index, points_per_draw);
		size_t required_bytes = COL_QUAD_SIZE * points_to_draw;

		// map and reserve enough buffer space for this draw
		void* buffer_ptr;
		int base_vertex_index = util_vbuf_efbpokequads->BeginAppendData(&buffer_ptr, (int)required_bytes, sizeof(ColVertex));

		// generate quads for each efb point
		ColVertex* base_vertex_ptr = reinterpret_cast<ColVertex*>(buffer_ptr);
		for (size_t i = 0; i < points_to_draw; i++)
		{
			// generate quad from the single point (clip-space coordinates)
			const EfbPokeData* point = &points[current_point_index];
			float x1 = float(point->x) * 2.0f / EFB_WIDTH - 1.0f;
			float y1 = -float(point->y) * 2.0f / EFB_HEIGHT + 1.0f;
			float x2 = float(point->x + 1) * 2.0f / EFB_WIDTH - 1.0f;
			float y2 = -float(point->y + 1) * 2.0f / EFB_HEIGHT + 1.0f;
			float z = (type == POKE_Z) ? (1.0f - float(point->data & 0xFFFFFF) / 16777216.0f) : 0.0f;
			u32 col = (type == POKE_Z) ? 0 : ((point->data & 0xFF00FF00) | ((point->data >> 16) & 0xFF) | ((point->data << 16) & 0xFF0000));
			current_point_index++;

			// quad -> triangles
			ColVertex* vertex = &base_vertex_ptr[i * 6];
			InitColVertex(&vertex[0], x1, y1, z, col);
			InitColVertex(&vertex[1], x2, y1, z, col);
			InitColVertex(&vertex[2], x1, y2, z, col);
			InitColVertex(&vertex[3], x1, y2, z, col);
			InitColVertex(&vertex[4], x2, y1, z, col);
			InitColVertex(&vertex[5], x2, y2, z, col);
		}

		// Issue the draw
		D3D::current_command_list->DrawInstanced(6 * (UINT)points_to_draw, 1, base_vertex_index, 0);

		if (current_point_index < num_points)
		{
			// If we're about to go through the loop again, that means we ran out of room in our efb buffer. Let's
			// tell the GPU to execute the currently-queued work, and wait on the CPU until it finishes (so we can
			// reuse the buffer). This shouldn't happen too often.

			D3D::command_list_mgr->ExecuteQueuedWork(true);
		}
	}

}

}  // namespace D3D

}  // namespace DX12
