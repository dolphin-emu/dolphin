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
