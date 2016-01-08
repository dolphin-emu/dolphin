// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
#pragma once

#include <array>
#include <d3d12.h>

#include "VideoCommon/NativeVertexFormat.h"

namespace DX12
{

class D3DVertexFormat final : public NativeVertexFormat
{
	std::array<D3D12_INPUT_ELEMENT_DESC, 15> m_elems;
	UINT m_num_elems;

	D3D12_INPUT_LAYOUT_DESC m_layout12;

public:
	D3DVertexFormat(const PortableVertexDeclaration& vtx_decl);
	~D3DVertexFormat();

	void SetupVertexPointers();

	D3D12_INPUT_LAYOUT_DESC GetActiveInputLayout12() const;

private:
	void AddInputElementDescFromAttributeFormatIfValid(const AttributeFormat* format, const char* semantic_name, unsigned int semantic_index);
};
}
