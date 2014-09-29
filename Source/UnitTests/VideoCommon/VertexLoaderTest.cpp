#include <set>

#include "Common/Common.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"

// Needs to be included later because it defines a TEST macro that conflicts
// with a TEST method definition in x64Emitter.h.
#include <gtest/gtest.h>  // NOLINT

TEST(VertexLoaderUID, UniqueEnough)
{
	std::set<VertexLoaderUID> uids;

	TVtxDesc vtx_desc;
	memset(&vtx_desc, 0, sizeof (vtx_desc));
	VAT vat;
	memset(&vat, 0, sizeof (vat));
	uids.insert(VertexLoaderUID(vtx_desc, vat));

	vtx_desc.Hex = 0xFEDCBA9876543210ull;
	EXPECT_EQ(uids.end(), uids.find(VertexLoaderUID(vtx_desc, vat)));
	uids.insert(VertexLoaderUID(vtx_desc, vat));

	vat.g0.Hex = 0xFFFFFFFF;
	vat.g1.Hex = 0xFFFFFFFF;
	vat.g2.Hex = 0xFFFFFFFF;
	EXPECT_EQ(uids.end(), uids.find(VertexLoaderUID(vtx_desc, vat)));
	uids.insert(VertexLoaderUID(vtx_desc, vat));
}

static u8 input_memory[16 * 1024 * 1024];
static u8 output_memory[16 * 1024 * 1024];

class VertexLoaderTest : public testing::Test
{
protected:

	void SetUp() override
	{
		memset(&input_memory[0], 0, sizeof (input_memory));
		memset(&output_memory[0], 0, sizeof (input_memory));

		memset(&m_vtx_desc, 0, sizeof (m_vtx_desc));
		memset(&m_vtx_attr, 0, sizeof (m_vtx_attr));

		ResetPointers();
	}

	// Pushes a value to the input stream.
	template <typename T>
	void Input(T val)
	{
		// Converts *to* big endian, not from.
		*(T*)(&input_memory[m_input_pos]) = Common::FromBigEndian(val);
		m_input_pos += sizeof (val);
	}

	// Reads a value from the output stream.
	template <typename T>
	T Output()
	{
		T out = *(T*)&output_memory[m_output_pos];
		m_output_pos += sizeof (out);
		return out;
	}

	// Combination of EXPECT_EQ and Output.
	template <typename T>
	void ExpectOut(T val)
	{
		EXPECT_EQ(val, Output<T>());
	}

	void ResetPointers()
	{
		g_video_buffer_read_ptr = &input_memory[0];
		VertexManager::s_pCurBufferPointer = &output_memory[0];
		m_input_pos = m_output_pos = 0;
	}

	u32 m_input_pos, m_output_pos;

	TVtxDesc m_vtx_desc;
	VAT m_vtx_attr;
};

TEST_F(VertexLoaderTest, PositionDirectFloatXYZ)
{
	m_vtx_desc.Position = 1;        // Direct
	m_vtx_attr.g0.PosElements = 1;  // XYZ
	m_vtx_attr.g0.PosFormat = 4;    // Float

	VertexLoader loader(m_vtx_desc, m_vtx_attr);

	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetNativeVertexDeclaration().stride);
	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetVertexSize());

	// Write some vertices.
	Input(0.0f); Input(0.0f); Input(0.0f);
	Input(1.0f); Input(0.0f); Input(0.0f);
	Input(0.0f); Input(1.0f); Input(0.0f);
	Input(0.0f); Input(0.0f); Input(1.0f);

	// Convert 4 points. "7" -> primitive are points.
	loader.RunVertices(m_vtx_attr, 7, 4);

	ExpectOut(0.0f); ExpectOut(0.0f); ExpectOut(0.0f);
	ExpectOut(1.0f); ExpectOut(0.0f); ExpectOut(0.0f);
	ExpectOut(0.0f); ExpectOut(1.0f); ExpectOut(0.0f);
	ExpectOut(0.0f); ExpectOut(0.0f); ExpectOut(1.0f);

	// Test that scale does nothing for floating point inputs.
	Input(1.0f); Input(2.0f); Input(4.0f);
	m_vtx_attr.g0.PosFrac = 1;
	loader.RunVertices(m_vtx_attr, 7, 1);
	ExpectOut(1.0f); ExpectOut(2.0f); ExpectOut(4.0f);
}

TEST_F(VertexLoaderTest, PositionDirectU16XY)
{
	m_vtx_desc.Position = 1;        // Direct
	m_vtx_attr.g0.PosElements = 0;  // XY
	m_vtx_attr.g0.PosFormat = 2;    // U16

	VertexLoader loader(m_vtx_desc, m_vtx_attr);

	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetNativeVertexDeclaration().stride);
	ASSERT_EQ(2 * sizeof (u16), (u32)loader.GetVertexSize());

	// Write some vertices.
	Input<u16>(0); Input<u16>(0);
	Input<u16>(1); Input<u16>(2);
	Input<u16>(256); Input<u16>(257);
	Input<u16>(65535); Input<u16>(65534);
	Input<u16>(12345); Input<u16>(54321);

	// Convert 5 points. "7" -> primitive are points.
	loader.RunVertices(m_vtx_attr, 7, 5);

	ExpectOut(0.0f); ExpectOut(0.0f); ExpectOut(0.0f);
	ExpectOut(1.0f); ExpectOut(2.0f); ExpectOut(0.0f);
	ExpectOut(256.0f); ExpectOut(257.0f); ExpectOut(0.0f);
	ExpectOut(65535.0f); ExpectOut(65534.0f); ExpectOut(0.0f);
	ExpectOut(12345.0f); ExpectOut(54321.0f); ExpectOut(0.0f);

	// Test that scale works on U16 inputs.
	Input<u16>(42); Input<u16>(24);
	m_vtx_attr.g0.PosFrac = 1;
	loader.RunVertices(m_vtx_attr, 7, 1);
	ExpectOut(21.0f); ExpectOut(12.0f); ExpectOut(0.0f);
}

TEST_F(VertexLoaderTest, PositionDirectFloatXYZSpeed)
{
	m_vtx_desc.Position = 1;        // Direct
	m_vtx_attr.g0.PosElements = 1;  // XYZ
	m_vtx_attr.g0.PosFormat = 4;    // Float

	VertexLoader loader(m_vtx_desc, m_vtx_attr);

	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetNativeVertexDeclaration().stride);
	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetVertexSize());

	for (int i = 0; i < 1000; ++i)
	{
		ResetPointers();
		loader.RunVertices(m_vtx_attr, 7, 100000);
	}
}

TEST_F(VertexLoaderTest, PositionDirectU16XYSpeed)
{
	m_vtx_desc.Position = 1;        // Direct
	m_vtx_attr.g0.PosElements = 0;  // XY
	m_vtx_attr.g0.PosFormat = 2;    // U16

	VertexLoader loader(m_vtx_desc, m_vtx_attr);

	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetNativeVertexDeclaration().stride);
	ASSERT_EQ(2 * sizeof (u16), (u32)loader.GetVertexSize());

	for (int i = 0; i < 1000; ++i)
	{
		ResetPointers();
		loader.RunVertices(m_vtx_attr, 7, 100000);
	}
}

TEST_F(VertexLoaderTest, LargeFloatVertexSpeed)
{
	// Enables most attributes in floating point direct mode to test speed.
	m_vtx_desc.PosMatIdx = 1;
	m_vtx_desc.Tex0MatIdx = 1;
	m_vtx_desc.Tex1MatIdx = 1;
	m_vtx_desc.Tex2MatIdx = 1;
	m_vtx_desc.Tex3MatIdx = 1;
	m_vtx_desc.Tex4MatIdx = 1;
	m_vtx_desc.Tex5MatIdx = 1;
	m_vtx_desc.Tex6MatIdx = 1;
	m_vtx_desc.Tex7MatIdx = 1;
	m_vtx_desc.Position = 1;
	m_vtx_desc.Normal = 1;
	m_vtx_desc.Color0 = 1;
	m_vtx_desc.Color1 = 1;
	m_vtx_desc.Tex0Coord = 1;
	m_vtx_desc.Tex1Coord = 1;
	m_vtx_desc.Tex2Coord = 1;
	m_vtx_desc.Tex3Coord = 1;
	m_vtx_desc.Tex4Coord = 1;
	m_vtx_desc.Tex5Coord = 1;
	m_vtx_desc.Tex6Coord = 1;
	m_vtx_desc.Tex7Coord = 1;

	m_vtx_attr.g0.PosElements = 1;        // XYZ
	m_vtx_attr.g0.PosFormat = 4;          // Float
	m_vtx_attr.g0.NormalElements = 1;     // NBT
	m_vtx_attr.g0.NormalFormat = 4;       // Float
	m_vtx_attr.g0.Color0Elements = 1;     // Has Alpha
	m_vtx_attr.g0.Color0Comp = 5;         // RGBA8888
	m_vtx_attr.g0.Color1Elements = 1;     // Has Alpha
	m_vtx_attr.g0.Color1Comp = 5;         // RGBA8888
	m_vtx_attr.g0.Tex0CoordElements = 1;  // ST
	m_vtx_attr.g0.Tex0CoordFormat = 4;    // Float
	m_vtx_attr.g1.Tex1CoordElements = 1;  // ST
	m_vtx_attr.g1.Tex1CoordFormat = 4;    // Float
	m_vtx_attr.g1.Tex2CoordElements = 1;  // ST
	m_vtx_attr.g1.Tex2CoordFormat = 4;    // Float
	m_vtx_attr.g1.Tex3CoordElements = 1;  // ST
	m_vtx_attr.g1.Tex3CoordFormat = 4;    // Float
	m_vtx_attr.g1.Tex4CoordElements = 1;  // ST
	m_vtx_attr.g1.Tex4CoordFormat = 4;    // Float
	m_vtx_attr.g2.Tex5CoordElements = 1;  // ST
	m_vtx_attr.g2.Tex5CoordFormat = 4;    // Float
	m_vtx_attr.g2.Tex6CoordElements = 1;  // ST
	m_vtx_attr.g2.Tex6CoordFormat = 4;    // Float
	m_vtx_attr.g2.Tex7CoordElements = 1;  // ST
	m_vtx_attr.g2.Tex7CoordFormat = 4;    // Float

	VertexLoader loader(m_vtx_desc, m_vtx_attr);

	// This test is only done 100x in a row since it's ~20x slower using the
	// current vertex loader implementation.
	for (int i = 0; i < 100; ++i)
	{
		ResetPointers();
		loader.RunVertices(m_vtx_attr, 7, 100000);
	}
}
