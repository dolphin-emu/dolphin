#include <unordered_set>
#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Core/HW/MMIO.h"

// Tests that the UniqueID function returns a "unique enough" identifier
// number: that is, it is unique in the address ranges we care about.
TEST(UniqueID, UniqueEnough)
{
	std::unordered_set<u32> ids;
	for (u32 i = 0xCC000000; i < 0xCC010000; ++i)
	{
		u32 unique_id = MMIO::UniqueID(i);
		EXPECT_EQ(ids.end(), ids.find(unique_id));
		ids.insert(unique_id);
	}
	for (u32 i = 0xCD000000; i < 0xCD010000; ++i)
	{
		u32 unique_id = MMIO::UniqueID(i);
		EXPECT_EQ(ids.end(), ids.find(unique_id));
		ids.insert(unique_id);
	}
}

TEST(IsMMIOAddress, SpecialAddresses)
{
	// WG Pipe address, should not be handled by MMIO.
	EXPECT_FALSE(MMIO::IsMMIOAddress(0xCC008000));

	// Memory zone used by games using the "MMU Speedhack".
	EXPECT_FALSE(MMIO::IsMMIOAddress(0xE0000000));
}

class MappingTest : public testing::Test
{
protected:
	virtual void SetUp() override
	{
		m_mapping = new MMIO::Mapping();
	}

	virtual void TearDown() override
	{
		delete m_mapping;
	}

	MMIO::Mapping* m_mapping;
};

TEST_F(MappingTest, ReadConstant)
{
	m_mapping->Register(0xCC001234, MMIO::Constant<u8>(0x42), MMIO::Nop<u8>());
	m_mapping->Register(0xCC001234, MMIO::Constant<u16>(0x1234), MMIO::Nop<u16>());
	m_mapping->Register(0xCC001234, MMIO::Constant<u32>(0xdeadbeef), MMIO::Nop<u32>());

	u8 val8;   m_mapping->Read(0xCC001234, &val8);
	u16 val16; m_mapping->Read(0xCC001234, &val16);
	u32 val32; m_mapping->Read(0xCC001234, &val32);

	EXPECT_EQ(0x42, val8);
	EXPECT_EQ(0x1234, val16);
	EXPECT_EQ(0xdeadbeef, val32);
}

TEST_F(MappingTest, ReadWriteDirect)
{
	u8 target_8 = 0;
	u16 target_16 = 0;
	u32 target_32 = 0;

	m_mapping->Register(0xCC001234, MMIO::DirectRead<u8>(&target_8), MMIO::DirectWrite<u8>(&target_8));
	m_mapping->Register(0xCC001234, MMIO::DirectRead<u16>(&target_16), MMIO::DirectWrite<u16>(&target_16));
	m_mapping->Register(0xCC001234, MMIO::DirectRead<u32>(&target_32), MMIO::DirectWrite<u32>(&target_32));

	for (u32 i = 0; i < 100; ++i)
	{
		u8 val8;   m_mapping->Read(0xCC001234, &val8);  EXPECT_EQ(i, val8);
		u16 val16; m_mapping->Read(0xCC001234, &val16); EXPECT_EQ(i, val16);
		u32 val32; m_mapping->Read(0xCC001234, &val32); EXPECT_EQ(i, val32);

		val8 += 1; m_mapping->Write(0xCC001234, val8);
		val16 += 1; m_mapping->Write(0xCC001234, val16);
		val32 += 1; m_mapping->Write(0xCC001234, val32);
	}
}

TEST_F(MappingTest, ReadWriteComplex)
{
	bool read_called = false, write_called = false;

	m_mapping->Register(0xCC001234,
		MMIO::ComplexRead<u8>([&read_called](u32 addr) {
			EXPECT_EQ(0xCC001234, addr);
			read_called = true;
			return 0x12;
		}),
		MMIO::ComplexWrite<u8>([&write_called](u32 addr, u8 val) {
			EXPECT_EQ(0xCC001234, addr);
			EXPECT_EQ(0x34, val);
			write_called = true;
		})
	);

	u8 val; m_mapping->Read(0xCC001234, &val); EXPECT_EQ(0x12, val);
	m_mapping->Write(0xCC001234, (u8)0x34);

	EXPECT_TRUE(read_called);
	EXPECT_TRUE(write_called);
}
