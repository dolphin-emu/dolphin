
#include <wil/stl.h>

#include "common.h"

#ifndef WIL_ENABLE_EXCEPTIONS
#error STL tests require exceptions
#endif

struct dummy
{
    char value;
};

// Specialize std::allocator<> so that we don't actually allocate/deallocate memory
dummy g_memoryBuffer[256];
namespace std
{
    template <>
    struct allocator<dummy>
    {
        using value_type = dummy;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        dummy* allocate(std::size_t count)
        {
            REQUIRE(count <= std::size(g_memoryBuffer));
            return g_memoryBuffer;
        }

        void deallocate(dummy* ptr, std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                REQUIRE(ptr[i].value == 0);
            }
        }
    };
}

TEST_CASE("StlTests::TestSecureAllocator", "[stl][secure_allocator]")
{
    {
        wil::secure_vector<dummy> sensitiveBytes(32, dummy{ 'a' });
    }
}
