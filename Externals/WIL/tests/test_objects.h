#pragma once

#include "catch.hpp"

// Useful for validating that the copy constructor is never called (e.g. to validate perfect forwarding). Note that
// the copy constructor/assignment operator are not deleted since we want to be able to validate in scenarios that
// require CopyConstructible (e.g. for wistd::function)
struct fail_on_copy
{
    fail_on_copy() = default;

    fail_on_copy(const fail_on_copy&)
    {
        FAIL("Copy constructor invoked for fail_on_copy type");
    }

    fail_on_copy(fail_on_copy&&) = default;

    fail_on_copy& operator=(const fail_on_copy&)
    {
        FAIL("Copy assignment operator invoked for fail_on_copy type");
        return *this;
    }

    fail_on_copy& operator=(fail_on_copy&&) = default;
};

// Useful for validating that objects get copied e.g. as opposed to capturing a reference
struct value_holder
{
    int value = 0xbadf00d;

    ~value_holder()
    {
        value = 0xbadf00d;
    }
};

// Useful for validating that functions, etc. are callable with move-only types
// Example real type that is move only is Microsoft::WRL::Wrappers::HString
struct cannot_copy
{
    cannot_copy() = default;
    cannot_copy(const cannot_copy&) = delete;
    cannot_copy& operator=(const cannot_copy&) = delete;

    cannot_copy(cannot_copy&&) = default;
    cannot_copy& operator=(cannot_copy&&) = default;
};

// State for object_counter type. This has the unfortunate side effect that the object_counter type cannot be used in
// contexts that require a default constructible type, but has the nice property that it allows for tests to run
// concurrently
struct object_counter_state
{
    volatile LONG constructed_count = 0;
    volatile LONG destructed_count = 0;
    volatile LONG copy_count = 0;
    volatile LONG move_count = 0;

    LONG instance_count()
    {
        return constructed_count - destructed_count;
    }
};

struct object_counter
{
    object_counter_state* state;

    object_counter(object_counter_state& s) :
        state(&s)
    {
        ::InterlockedIncrement(&state->constructed_count);
    }

    object_counter(const object_counter& other) :
        state(other.state)
    {
        ::InterlockedIncrement(&state->constructed_count);
        ::InterlockedIncrement(&state->copy_count);
    }

    object_counter(object_counter&& other) WI_NOEXCEPT :
        state(other.state)
    {
        ::InterlockedIncrement(&state->constructed_count);
        ::InterlockedIncrement(&state->move_count);
    }

    ~object_counter()
    {
        ::InterlockedIncrement(&state->destructed_count);
        state = nullptr;
    }

    object_counter& operator=(const object_counter&)
    {
        ::InterlockedIncrement(&state->copy_count);
        return *this;
    }

    object_counter& operator=(object_counter&&) WI_NOEXCEPT
    {
        ::InterlockedIncrement(&state->move_count);
        return *this;
    }
};
