
#include <wil/wistd_functional.h>

#include "common.h"
#include "test_objects.h"

// Test methods/objects
int GetValue()
{
    return 42;
}

int GetOtherValue()
{
    return 8;
}

int Negate(int value)
{
    return -value;
}

int Add(int lhs, int rhs)
{
    return lhs + rhs;
}

TEST_CASE("WistdFunctionTests::CallOperatorTest", "[wistd]")
{
    wistd::function<int()> getValue = GetValue;
    REQUIRE(GetValue() == getValue());

    wistd::function<int(int)> negate = Negate;
    REQUIRE(Negate(42) == negate(42));

    wistd::function<int(int, int)> add = Add;
    REQUIRE(Add(42, 8) == add(42, 8));
}

TEST_CASE("WistdFunctionTests::AssignmentOperatorTest", "[wistd]")
{
    wistd::function<int()> fn = GetValue;
    REQUIRE(GetValue() == fn());

    fn = GetOtherValue;
    REQUIRE(GetOtherValue() == fn());
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("WistdFunctionTests::StdFunctionConstructionTest", "[wistd]")
{
    // We should be able to capture a std::function in a wistd::function
    wistd::function<int()> fn;

    {
        value_holder holder{ 42 };
        std::function<int()> stdFn = [holder]()
        {
            return holder.value;
        };

        fn = stdFn;
    }

    REQUIRE(42 == fn());
}
#endif

TEST_CASE("WistdFunctionTests::CopyConstructionTest", "[wistd]")
{
    object_counter_state state;
    {
        wistd::function<int()> copyFrom = [counter = object_counter{ state }]()
        {
            return counter.state->copy_count;
        };
        REQUIRE(0 == copyFrom());

        auto copyTo = copyFrom;
        REQUIRE(1 == copyTo());
    }

    REQUIRE(0 == state.instance_count());
}

TEST_CASE("WistdFunctionTests::CopyAssignmentTest", "[wistd]")
{
    object_counter_state state;
    {
        wistd::function<int()> copyTo;
        {
            wistd::function<int()> copyFrom = [counter = object_counter{ state }]()
            {
                return counter.state->copy_count;
            };
            REQUIRE(0 == copyFrom());

            copyTo = copyFrom;
        }

        REQUIRE(1 == copyTo());
    }

    REQUIRE(0 == state.instance_count());
}

TEST_CASE("WistdFunctionTests::MoveConstructionTest", "[wistd]")
{
    object_counter_state state;
    {
        wistd::function<int()> moveFrom = [counter = object_counter{ state }]()
        {
            return counter.state->copy_count;
        };
        REQUIRE(0 == moveFrom());

        auto moveTo = std::move(moveFrom);
        REQUIRE(0 == moveTo());

        // Because we move the underlying function object, we _must_ invalidate the moved from function
        REQUIRE_FALSE(moveFrom != nullptr);
    }

    REQUIRE(0 == state.instance_count());
}

TEST_CASE("WistdFunctionTests::MoveAssignmentTest", "[wistd]")
{
    object_counter_state state;
    {
        wistd::function<int()> moveTo;
        {
            wistd::function<int()> moveFrom = [counter = object_counter{ state }]()
            {
                return counter.state->copy_count;
            };
            REQUIRE(0 == moveFrom());

            moveTo = std::move(moveFrom);
        }

        REQUIRE(0 == moveTo());
    }

    REQUIRE(0 == state.instance_count());
}

TEST_CASE("WistdFunctionTests::SwapTest", "[wistd]")
{
    object_counter_state state;
    {
        wistd::function<int()> first;
        wistd::function<int()> second;

        first.swap(second);
        REQUIRE_FALSE(first != nullptr);
        REQUIRE_FALSE(second != nullptr);

        first = [counter = object_counter{ state }]()
        {
            return counter.state->copy_count;
        };

        first.swap(second);
        REQUIRE_FALSE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(0 == second());

        first.swap(second);
        REQUIRE(first != nullptr);
        REQUIRE_FALSE(second != nullptr);
        REQUIRE(0 == first());

        second = [counter = object_counter{ state }]()
        {
            return counter.state->copy_count;
        };

        first.swap(second);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(0 == first());
    }

    REQUIRE(0 == state.instance_count());
}

// MSVC's optimizer has had issues with wistd::function in the past when forwarding wistd::function objects to a
// function that accepts the arguments by value. This test exercises the workaround that we have in place. Note
// that this of course requires building with optimizations enabled
void ForwardingTest(wistd::function<int()> getValue, wistd::function<int(int)> negate, wistd::function<int(int, int)> add)
{
    // Previously, this would cause a runtime crash
    REQUIRE(Add(GetValue(), Negate(8)) == add(getValue(), negate(8)));
}

template <typename... Args>
void CallForwardingTest(Args&&... args)
{
    ForwardingTest(wistd::forward<Args>(args)...);
}

TEST_CASE("WistdFunctionTests::OptimizationRegressionTest", "[wistd]")
{
    CallForwardingTest(
        wistd::function<int()>(GetValue),
        wistd::function<int(int)>(Negate),
        wistd::function<int(int, int)>(Add));
}
