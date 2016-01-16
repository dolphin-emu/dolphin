// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

/**
 * Common interface for classes that need to go through the shader generation path (GenerateVertexShader, GenerateGeometryShader, GeneratePixelShader)
 * In particular, this includes the shader code generator (ShaderCode).
 * A different class (ShaderUid) can be used to uniquely identify each ShaderCode object.
 * More interesting things can be done with this, e.g. ShaderConstantProfile checks what shader constants are being used. This can be used to optimize buffer management.
 * If the class does not use one or more of these methods (e.g. Uid class does not need code), the method will be defined as a no-op by the base class, and the call
 * should be optimized out. The reason for this implementation is so that shader selection/generation can be done in two passes, with only a cache lookup being
 * required if the shader has already been generated.
 */
class ShaderGeneratorInterface
{
public:
	/*
	 * Used when the shader generator would write a piece of ShaderCode.
	 * Can be used like printf.
	 * @note In the ShaderCode implementation, this does indeed write the parameter string to an internal buffer. However, you're free to do whatever you like with the parameter.
	 */
	void Write(const char*, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	{
	}

	/*
	 * Tells us that a specific constant range (including last_index) is being used by the shader
	 */
	void SetConstantsUsed(unsigned int first_index, unsigned int last_index) {}

	/*
	 * Returns a pointer to an internally stored object of the uid_data type.
	 * @warning since most child classes use the default implementation you shouldn't access this directly without adding precautions against nullptr access (e.g. via adding a dummy structure, cf. the vertex/pixel shader generators)
	 */
	template<class uid_data>
	uid_data* GetUidData() { return nullptr; }
};

/*
 * Shader UID class used to uniquely identify the ShaderCode output written in the shader generator.
 * uid_data can be any struct of parameters that uniquely identify each shader code output.
 * Unless performance is not an issue, uid_data should be tightly packed to reduce memory footprint.
 * Shader generators will write to specific uid_data fields; ShaderUid methods will only read raw u32 values from a union.
 * NOTE: Because LinearDiskCache reads and writes the storage associated with a ShaderUid instance, ShaderUid must be trivially copyable.
 */
template<class uid_data>
class ShaderUid : public ShaderGeneratorInterface
{
public:
	bool operator == (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, data.NumValues() * sizeof(*values)) == 0;
	}

	bool operator != (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, data.NumValues() * sizeof(*values)) != 0;
	}

	// determines the storage order inside STL containers
	bool operator < (const ShaderUid& obj) const
	{
		return memcmp(this->values, obj.values, data.NumValues() * sizeof(*values)) < 0;
	}

	template<class uid_data2>
	uid_data2* GetUidData() { return &data; }
	const uid_data* GetUidData() const { return &data; }
	const u8* GetUidDataRaw() const { return &values[0]; }

	size_t GetUidDataSize() const { return sizeof(values); }

private:
	union
	{
		uid_data data;
		u8 values[sizeof(uid_data)];
	};
};

class ShaderCode : public ShaderGeneratorInterface
{
public:
	ShaderCode()
	{
		m_buffer.reserve(16384);
	}

	const std::string& GetBuffer() const { return m_buffer; }

	void Write(const char* fmt, ...)
#ifdef __GNUC__
	__attribute__((format(printf, 2, 3)))
#endif
	{
		va_list arglist;
		va_start(arglist, fmt);
		m_buffer += StringFromFormatV(fmt, arglist);
		va_end(arglist);
	}

protected:
	std::string m_buffer;
};

/**
 * Generates a shader constant profile which can be used to query which constants are used in a shader
 */
class ShaderConstantProfile : public ShaderGeneratorInterface
{
public:
	ShaderConstantProfile(int num_constants) { constant_usage.resize(num_constants); }

	void SetConstantsUsed(unsigned int first_index, unsigned int last_index)
	{
		for (unsigned int i = first_index; i < last_index + 1; ++i)
			constant_usage[i] = true;
	}

	bool ConstantIsUsed(unsigned int index) const
	{
		// TODO: Not ready for usage yet
		return true;
		//return constant_usage[index];
	}

private:
	std::vector<bool> constant_usage; // TODO: Is vector<bool> appropriate here?
};

template<class T>
inline void DefineOutputMember(T& object, API_TYPE api_type, const char* qualifier, const char* type, const char* name, int var_index, const char* semantic = "", int semantic_index = -1)
{
	if (qualifier != nullptr)
		object.Write("\t%s %s %s", qualifier, type, name);
	else
		object.Write("\t%s %s", type, name);

	if (var_index != -1)
		object.Write("%d", var_index);

	if (api_type == API_D3D && strlen(semantic) > 0)
	{
		if (semantic_index != -1)
			object.Write(" : %s%d", semantic, semantic_index);
		else
			object.Write(" : %s", semantic);
	}

	object.Write(";\n");
}

template<class T>
inline void GenerateVSOutputMembers(T& object, API_TYPE api_type, u32 texgens, bool per_pixel_lighting, const char* qualifier = nullptr)
{
	DefineOutputMember(object, api_type, qualifier, "float4", "pos", -1, "POSITION");
	DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 0, "COLOR", 0);
	DefineOutputMember(object, api_type, qualifier, "float4", "colors_", 1, "COLOR", 1);

	for (unsigned int i = 0; i < texgens; ++i)
		DefineOutputMember(object, api_type, qualifier, "float3", "tex", i, "TEXCOORD", i);

	DefineOutputMember(object, api_type, qualifier, "float4", "clipPos", -1, "TEXCOORD", texgens);

	if (per_pixel_lighting)
	{
		DefineOutputMember(object, api_type, qualifier, "float3", "Normal", -1, "TEXCOORD", texgens + 1);
		DefineOutputMember(object, api_type, qualifier, "float3", "WorldPos", -1, "TEXCOORD", texgens + 2);
	}
}

template<class T>
inline void AssignVSOutputMembers(T& object, const char* a, const char* b, u32 texgens, bool per_pixel_lighting)
{
	object.Write("\t%s.pos = %s.pos;\n", a, b);
	object.Write("\t%s.colors_0 = %s.colors_0;\n", a, b);
	object.Write("\t%s.colors_1 = %s.colors_1;\n", a, b);

	for (unsigned int i = 0; i < texgens; ++i)
		object.Write("\t%s.tex%d = %s.tex%d;\n", a, i, b, i);

	object.Write("\t%s.clipPos = %s.clipPos;\n", a, b);

	if (per_pixel_lighting)
	{
		object.Write("\t%s.Normal = %s.Normal;\n", a, b);
		object.Write("\t%s.WorldPos = %s.WorldPos;\n", a, b);
	}
}

// We use the flag "centroid" to fix some MSAA rendering bugs. With MSAA, the
// pixel shader will be executed for each pixel which has at least one passed sample.
// So there may be rendered pixels where the center of the pixel isn't in the primitive.
// As the pixel shader usually renders at the center of the pixel, this position may be
// outside the primitive. This will lead to sampling outside the texture, sign changes, ...
// As a workaround, we interpolate at the centroid of the coveraged pixel, which
// is always inside the primitive.
// Without MSAA, this flag is defined to have no effect.
inline const char* GetInterpolationQualifier(API_TYPE api_type, bool msaa, bool ssaa, bool in = true, bool in_out = false)
{
	if (!msaa)
		return "";

	if (!ssaa)
	{
		if (in_out && api_type == API_OPENGL && !g_ActiveConfig.backend_info.bSupportsBindingLayout)
			return in ? "centroid in" : "centroid out";
		return "centroid";
	}

	return "sample";
}

// Constant variable names
#define I_COLORS        "color"
#define I_KCOLORS       "k"
#define I_ALPHA         "alphaRef"
#define I_TEXDIMS       "texdim"
#define I_ZBIAS         "czbias"
#define I_INDTEXSCALE   "cindscale"
#define I_INDTEXMTX     "cindmtx"
#define I_FOGCOLOR      "cfogcolor"
#define I_FOGI          "cfogi"
#define I_FOGF          "cfogf"
#define I_ZSLOPE        "czslope"
#define I_EFBSCALE      "cefbscale"

#define I_POSNORMALMATRIX       "cpnmtx"
#define I_PROJECTION            "cproj"
#define I_MATERIALS             "cmtrl"
#define I_LIGHTS                "clights"
#define I_TEXMATRICES           "ctexmtx"
#define I_TRANSFORMMATRICES     "ctrmtx"
#define I_NORMALMATRICES        "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_PIXELCENTERCORRECTION "cpixelcenter"

#define I_STEREOPARAMS  "cstereo"
#define I_LINEPTPARAMS  "clinept"
#define I_TEXOFFSET     "ctexoffset"

static const char s_shader_uniforms[] =
	"\tfloat4 " I_POSNORMALMATRIX"[6];\n"
	"\tfloat4 " I_PROJECTION"[4];\n"
	"\tint4 " I_MATERIALS"[4];\n"
	"\tLight " I_LIGHTS"[8];\n"
	"\tfloat4 " I_TEXMATRICES"[24];\n"
	"\tfloat4 " I_TRANSFORMMATRICES"[64];\n"
	"\tfloat4 " I_NORMALMATRICES"[32];\n"
	"\tfloat4 " I_POSTTRANSFORMMATRICES"[64];\n"
	"\tfloat4 " I_PIXELCENTERCORRECTION";\n";
