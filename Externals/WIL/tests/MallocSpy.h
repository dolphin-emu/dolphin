#pragma once

#include "catch.hpp"
#include <objbase.h>
#include <wil/wistd_functional.h>
#include <wrl/implements.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

// IMallocSpy requires you to implement all methods, but we often only want one or two...
struct MallocSpy : Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IMallocSpy>
{
    wistd::function<SIZE_T(SIZE_T)> PreAllocCallback;
    virtual SIZE_T STDMETHODCALLTYPE PreAlloc(SIZE_T requestSize) override
    {
        if (PreAllocCallback)
        {
            return PreAllocCallback(requestSize);
        }

        return requestSize;
    }

    wistd::function<void*(void*)> PostAllocCallback;
    virtual void* STDMETHODCALLTYPE PostAlloc(void* ptr) override
    {
        if (PostAllocCallback)
        {
            return PostAllocCallback(ptr);
        }

        return ptr;
    }

    wistd::function<void*(void*)> PreFreeCallback;
    virtual void* STDMETHODCALLTYPE PreFree(void* ptr, BOOL wasSpyed) override
    {
        if (wasSpyed && PreFreeCallback)
        {
            return PreFreeCallback(ptr);
        }

        return ptr;
    }

    virtual void STDMETHODCALLTYPE PostFree(BOOL /*wasSpyed*/) override
    {
    }

    wistd::function<SIZE_T(void*, SIZE_T, void**)> PreReallocCallback;
    virtual SIZE_T STDMETHODCALLTYPE PreRealloc(void* ptr, SIZE_T requestSize, void** newPtr, BOOL wasSpyed) override
    {
        *newPtr = ptr;
        if (wasSpyed && PreReallocCallback)
        {
            return PreReallocCallback(ptr, requestSize, newPtr);
        }

        return requestSize;
    }

    wistd::function<void*(void*)> PostReallocCallback;
    virtual void* STDMETHODCALLTYPE PostRealloc(void* ptr, BOOL wasSpyed) override
    {
        if (wasSpyed && PostReallocCallback)
        {
            return PostReallocCallback(ptr);
        }

        return ptr;
    }

    wistd::function<void*(void*)> PreGetSizeCallback;
    virtual void* STDMETHODCALLTYPE PreGetSize(void* ptr, BOOL wasSpyed) override
    {
        if (wasSpyed && PreGetSizeCallback)
        {
            return PreGetSizeCallback(ptr);
        }

        return ptr;
    }

    wistd::function<SIZE_T(SIZE_T)> PostGetSizeCallback;
    virtual SIZE_T STDMETHODCALLTYPE PostGetSize(SIZE_T size, BOOL wasSpyed) override
    {
        if (wasSpyed && PostGetSizeCallback)
        {
            return PostGetSizeCallback(size);
        }

        return size;
    }

    wistd::function<void*(void*)> PreDidAllocCallback;
    virtual void* STDMETHODCALLTYPE PreDidAlloc(void* ptr, BOOL wasSpyed) override
    {
        if (wasSpyed && PreDidAllocCallback)
        {
            return PreDidAllocCallback(ptr);
        }

        return ptr;
    }

    virtual int STDMETHODCALLTYPE PostDidAlloc(void* /*ptr*/, BOOL /*wasSpyed*/, int result) override
    {
        return result;
    }

    virtual void STDMETHODCALLTYPE PreHeapMinimize() override
    {
    }

    virtual void STDMETHODCALLTYPE PostHeapMinimize() override
    {
    }
};

Microsoft::WRL::ComPtr<MallocSpy> MakeSecureDeleterMallocSpy()
{
    using namespace Microsoft::WRL;
    auto result = Make<MallocSpy>();
    REQUIRE(result);

    result->PreFreeCallback = [](void* ptr)
    {
        ComPtr<IMalloc> malloc;
        if (SUCCEEDED(::CoGetMalloc(1, &malloc)))
        {
            auto size = malloc->GetSize(ptr);
            auto buffer = static_cast<byte*>(ptr);
            for (size_t i = 0; i < size; ++i)
            {
                REQUIRE(buffer[i] == 0);
            }
        }

        return ptr;
    };

    return result;
}

#endif
