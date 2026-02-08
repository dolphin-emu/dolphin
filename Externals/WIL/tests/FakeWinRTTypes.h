#pragma once

#include <wil/resource.h>
#include <windows.foundation.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

template <typename T>
struct WinRTStorage
{
    T value = {};

    HRESULT CopyTo(T* result)
    {
        *result = value;
        return S_OK;
    }

    HRESULT Set(T val)
    {
        value = val;
        return S_OK;
    }

    static void Destroy(T&)
    {
    }

    void Reset()
    {
    }

    bool Equals(T val)
    {
        // NOTE: Padding can through this off, but this isn't intended to be a robust solution...
        return memcmp(&value, &val, sizeof(T)) == 0;
    }
};

template <>
struct WinRTStorage<HSTRING>
{
    Microsoft::WRL::Wrappers::HString value;

    HRESULT CopyTo(HSTRING* result) const
    {
        return value.CopyTo(result);
    }

    HRESULT Set(HSTRING val)
    {
        return value.Set(val);
    }

    static void Destroy(HSTRING& val)
    {
        ::WindowsDeleteString(val);
        val = nullptr;
    }

    void Reset()
    {
        value = {};
    }

    bool Equals(HSTRING val) const
    {
        return value == val;
    }
};

template <typename T>
struct WinRTStorage<T*>
{
    Microsoft::WRL::ComPtr<T> value;

    HRESULT CopyTo(T** result)
    {
        *result = Microsoft::WRL::ComPtr<T>(value).Detach();
        return S_OK;
    }

    HRESULT Set(T* val)
    {
        value = val;
        return S_OK;
    }

    static void Destroy(T*& val)
    {
        val->Release();
        val = nullptr;
    }

    void Reset()
    {
        value.Reset();
    }

    bool Equals(T* val)
    {
        return value.Get() == val;
    }
};

// Very minimal IAsyncOperation implementation that gives calling tests control over when it completes
template <typename Logical, typename Abi = Logical>
struct FakeAsyncOperation : Microsoft::WRL::RuntimeClass<
    ABI::Windows::Foundation::IAsyncInfo,
    ABI::Windows::Foundation::IAsyncOperation<Logical>>
{
    using Handler = ABI::Windows::Foundation::IAsyncOperationCompletedHandler<Logical>;

    // IAsyncInfo
    IFACEMETHODIMP get_Id(unsigned int*) override
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP get_Status(AsyncStatus* status) override
    {
        auto lock = m_lock.lock_shared();
        *status = m_status;
        return S_OK;
    }

    IFACEMETHODIMP get_ErrorCode(HRESULT* errorCode) override
    {
        auto lock = m_lock.lock_shared();
        *errorCode = m_result;
        return S_OK;
    }

    IFACEMETHODIMP Cancel() override
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP Close() override
    {
        return E_NOTIMPL;
    }

    // IAsyncOperation
    IFACEMETHODIMP put_Completed(Handler* handler) override
    {
        bool invoke = false;
        {
            auto lock = m_lock.lock_exclusive();
            if (m_handler)
            {
                return E_FAIL;
            }

            m_handler = handler;
            invoke = m_status != ABI::Windows::Foundation::AsyncStatus::Started;
        }

        if (invoke)
        {
            handler->Invoke(this, m_status);
        }

        return S_OK;
    }

    IFACEMETHODIMP get_Completed(Handler** handler) override
    {
        auto lock = m_lock.lock_shared();
        *handler = Microsoft::WRL::ComPtr<Handler>(m_handler).Detach();
        return S_OK;
    }

    IFACEMETHODIMP GetResults(Abi* results) override
    {
        return m_storage.CopyTo(results);
    }

    // Test functions
    void Complete(HRESULT hr, Abi result)
    {
        using namespace ABI::Windows::Foundation;
        Handler* handler = nullptr;
        {
            auto lock = m_lock.lock_exclusive();
            if (m_status == AsyncStatus::Started)
            {
                m_result = hr;
                m_storage.Set(result);
                m_status = SUCCEEDED(hr) ? AsyncStatus::Completed : AsyncStatus::Error;
                handler = m_handler.Get();
            }
        }

        if (handler)
        {
            handler->Invoke(this, m_status);
        }
    }

private:

    wil::srwlock m_lock;
    Microsoft::WRL::ComPtr<Handler> m_handler;
    ABI::Windows::Foundation::AsyncStatus m_status = ABI::Windows::Foundation::AsyncStatus::Started;
    HRESULT m_result = S_OK;
    WinRTStorage<Abi> m_storage;
};

template <typename Logical, typename Abi = Logical, size_t MaxSize = 250>
struct FakeVector : Microsoft::WRL::RuntimeClass<
    ABI::Windows::Foundation::Collections::IVector<Logical>,
    ABI::Windows::Foundation::Collections::IVectorView<Logical>>
{
    // IVector
    IFACEMETHODIMP GetAt(unsigned index, Abi* item) override
    {
        if (index >= m_size)
        {
            return E_BOUNDS;
        }

        return m_data[index].CopyTo(item);
    }

    IFACEMETHODIMP get_Size(unsigned* size) override
    {
        *size = static_cast<unsigned>(m_size);
        return S_OK;
    }

    IFACEMETHODIMP GetView(ABI::Windows::Foundation::Collections::IVectorView<Logical>** view) override
    {
        this->AddRef();
        *view = this;
        return S_OK;
    }

    IFACEMETHODIMP IndexOf(Abi value, unsigned* index, boolean* found) override
    {
        for (size_t i = 0; i < m_size; ++i)
        {
            if (m_data[i].Equals(value))
            {
                *index = static_cast<unsigned>(i);
                *found = true;
                return S_OK;
            }
        }

        *index = 0;
        *found = false;
        return S_OK;
    }

    IFACEMETHODIMP SetAt(unsigned index, Abi item) override
    {
        if (index >= m_size)
        {
            return E_BOUNDS;
        }

        return m_data[index].Set(item);
    }

    IFACEMETHODIMP InsertAt(unsigned index, Abi item) override
    {
        // Insert at the end and swap it into place
        if (index > m_size)
        {
            return E_BOUNDS;
        }

        auto hr = Append(item);
        if (SUCCEEDED(hr))
        {
            for (size_t i = m_size - 1; i > index; --i)
            {
                wistd::swap_wil(m_data[i], m_data[i - 1]);
            }
        }

        return hr;
    }

    IFACEMETHODIMP RemoveAt(unsigned index) override
    {
        if (index >= m_size)
        {
            return E_BOUNDS;
        }

        for (size_t i = index + 1; i < m_size; ++i)
        {
            wistd::swap_wil(m_data[i], m_data[i - 1]);
        }

        m_data[--m_size].Reset();
        return S_OK;
    }

    IFACEMETHODIMP Append(Abi item) override
    {
        if (m_size > MaxSize)
        {
            return E_OUTOFMEMORY;
        }

        auto hr = m_data[m_size].Set(item);
        if (SUCCEEDED(hr))
        {
            ++m_size;
        }

        return hr;
    }

    IFACEMETHODIMP RemoveAtEnd() override
    {
        if (m_size == 0)
        {
            return E_BOUNDS;
        }

        m_data[--m_size].Reset();
        return S_OK;
    }

    IFACEMETHODIMP Clear() override
    {
        for (size_t i = 0; i < m_size; ++i)
        {
            m_data[i].Reset();
        }

        m_size = 0;
        return S_OK;
    }

    IFACEMETHODIMP GetMany(unsigned startIndex, unsigned capacity, Abi* value, unsigned* actual) override
    {
        *actual = 0;
        if (startIndex >= m_size)
        {
            return S_OK;
        }

        auto count = m_size - startIndex;
        count = (count > capacity) ? capacity : count;

        HRESULT hr = S_OK;
        unsigned i;
        for (i = 0; (i < count) && SUCCEEDED(hr); ++i)
        {
            hr = m_data[startIndex + i].CopyTo(value + i);
        }

        if (SUCCEEDED(hr))
        {
            *actual = static_cast<unsigned>(count);
        }
        else
        {
            while (i--)
            {
                WinRTStorage<Abi>::Destroy(value[i]);
            }
        }

        return hr;
    }

    IFACEMETHODIMP ReplaceAll(unsigned count, Abi* value) override
    {
        if (count > MaxSize)
        {
            return E_OUTOFMEMORY;
        }

        Clear();

        HRESULT hr = S_OK;
        for (size_t i = 0; (i < count) && SUCCEEDED(hr); ++i)
        {
            hr = m_data[i].Set(value[i]);
        }

        if (FAILED(hr))
        {
            Clear();
        }

        return hr;
    }

private:

    size_t m_size = 0;
    WinRTStorage<Abi> m_data[MaxSize];
};
