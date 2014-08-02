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
	VAT vat;
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

class VertexLoaderTest : public testing::Test
{
protected:
	void SetUp() override
	{
		// The vertex loading code is still using global variables to specify
		// where to load and store data.
		//
		// Reserve 64K of memory for both.
		m_input_memory.clear(); m_input_memory.resize(65536);
		m_output_memory.clear(); m_output_memory.resize(65536);
		g_pVideoData = m_input_memory.data();
		VertexManager::s_pCurBufferPointer = m_output_memory.data();

		m_input_pos = m_output_pos = 0;
	}

	// Pushes a value to the input stream.
	template <typename T>
	void Input(T val)
	{
		// Converts *to* big endian, not from.
		*(T*)(&m_input_memory[m_input_pos]) = Common::FromBigEndian(val);
		m_input_pos += sizeof (val);
	}

	// Reads a value from the output stream.
	template <typename T>
	T Output()
	{
		T out = *(T*)&m_output_memory[m_output_pos];
		m_output_pos += sizeof (out);
		return out;
	}

	u32 m_input_pos, m_output_pos;
	std::vector<u8> m_input_memory, m_output_memory;
};

TEST_F(VertexLoaderTest, PositionDirectXYZ)
{
	TVtxDesc vtx_desc;
	VAT vtx_attr;

	memset(&vtx_desc, 0, sizeof (vtx_desc));
	memset(&vtx_attr, 0, sizeof (vtx_attr));

	vtx_desc.Position = 1;        // Direct.
	vtx_attr.g0.PosElements = 1;  // XYZ
	vtx_attr.g0.PosFormat = 4;    // Float

	VertexLoader loader(vtx_desc, vtx_attr);

	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetNativeVertexDeclaration().stride);
	ASSERT_EQ(3 * sizeof (float), (u32)loader.GetVertexSize());

	// Write some vertices.
	Input(0.0f); Input(0.0f); Input(0.0f);
	Input(1.0f); Input(0.0f); Input(0.0f);
	Input(0.0f); Input(1.0f); Input(0.0f);
	Input(0.0f); Input(0.0f); Input(1.0f);

	// Convert 4 points. "7" -> primitive are points.
	loader.RunVertices(vtx_attr, 7, 4);

	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());

	EXPECT_EQ(1.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());

	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(1.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());

	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(0.0f, Output<float>());
	EXPECT_EQ(1.0f, Output<float>());
}
