// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Common
{

// A strong reference to a COM object.
template<typename Interface>
class ComPtr
{
	// This is a COM smart-pointer. It has some advantages over the COM smart-
	// pointers provided by ATL and Windows Runtime:
	// - It includes a move constructor - ComPtr's can be stored in containers
	// - It disallows direct usage of AddRef and Release but still allows
	//   QueryInterface
	// Adapted from:
	// Kerr, Kenny. "Windows with C++ - COM Smart Pointers Revisited."
	// MSDN Magazine. <https://msdn.microsoft.com/en-us/magazine/dn904668.aspx>

	// TODO: This class may be incomplete. Feel free to tweak or extend.

	template<typename OtherInterface> friend class ComPtr;
public:
	ComPtr() noexcept = default;

	ComPtr(std::nullptr_t) noexcept { }

	// Construct ComPtr<Interface> from ComPtr<Interface>.
	ComPtr(const ComPtr& other) noexcept
		: m_ptr(other.m_ptr)
	{
		InternalAddRef();
	}

	// Construct ComPtr<Interface> from ComPtr<OtherInterface>.
	// OtherInterface must inherit from Interface.
	template<typename OtherInterface>
	ComPtr(const ComPtr<OtherInterface>& other) noexcept
		: m_ptr(other.m_ptr)
	{
		// The constructor above seems redundant, but it cannot be omitted or
		// else the compiler will assume the copy constructor is deleted and
		// will reject assignments from ComPtr<Interface> to ComPtr<Interface>.
		InternalAddRef();
	}

	// Construct ComPtr<Interface> by moving the contents of a
	// ComPtr<OtherInterface>.
	template<typename OtherInterface>
	ComPtr(ComPtr<OtherInterface>&& other) noexcept
		: m_ptr(other.m_ptr)
	{
		// A generic move constructor suffices for both cases:
		// Move ComPtr<OtherInterface> to ComPtr<Interface>
		// and Move ComPtr<Interface> to ComPtr<Interface>.
		other.m_ptr = nullptr;
	}

	ComPtr& operator=(const ComPtr& other) noexcept
	{
		Copy(other.m_ptr);
		return *this;
	}

	template<typename T>
	ComPtr& operator=(const ComPtr<T>& other) noexcept
	{
		Copy(other.m_ptr);
		return *this;
	}

	template<typename T>
	ComPtr& operator=(ComPtr<T>&& other) noexcept
	{
		InternalMove(other);
		return *this;
	}

	~ComPtr() noexcept
	{
		Release();
	}

	explicit operator bool() const noexcept
	{
		return m_ptr != nullptr;
	}

	template<typename T>
	bool operator==(const ComPtr<T>& other) const noexcept
	{
		return m_ptr == other.m_ptr;
	}

	template<typename T>
	bool operator!=(const ComPtr<T>& other) const noexcept
	{
		return !(*this == other);
	}

	bool operator==(const Interface* other) const noexcept
	{
		return m_ptr == other;
	}

	bool operator!=(const Interface* other) const noexcept
	{
		return !(*this == other);
	}

	bool operator==(std::nullptr_t) const noexcept
	{
		return m_ptr == nullptr;
	}

	bool operator!=(std::nullptr_t) const noexcept
	{
		return !(*this == nullptr);
	}

	// Release COM pointer and return number of remaining references.
	ULONG Release() noexcept
	{
		Interface* temp = m_ptr;
		if (temp)
		{
			// Null out m_ptr before releasing.
			// This prevents breakage in special cases where Releasing might
			// set off a chain of events causing this smart pointer to be
			// released again.
			m_ptr = nullptr;
			return temp->Release();
		}

		return 0;
	}

	template<typename T>
	ComPtr<T> As() const noexcept
	{
		ComPtr<T> temp;
		if (m_ptr)
		{
			m_ptr->QueryInterface(temp.GetAddressOf());
		}
		return temp;
	}

	// Copy raw pointer and call AddRef.
	void Copy(Interface* other) noexcept
	{
		if (m_ptr != other)
		{
			Release();
			m_ptr = other;
			InternalAddRef();
		}
	}

	// AddRef and copy to raw pointer.
	void CopyTo(Interface** other) const noexcept
	{
		InternalAddRef();
		*other = m_ptr;
	}

	// Attach to existing pointer without calling AddRef.
	void Attach(Interface* other) noexcept
	{
		Release();
		m_ptr = other;
	}

	// Detach pointer without releasing.
	Interface* Detach() noexcept
	{
		Interface* temp = m_ptr;
		m_ptr = nullptr;
		return temp;
	}

	Interface* Get() const noexcept
	{
		return m_ptr;
	}

	// Get address of Interface pointer.
	// This can be passed to factory functions that return newly created
	// objects in an Interface**. The currently held pointer must be null.
	Interface** GetAddressOf() noexcept
	{
		assert(m_ptr == nullptr);
		return &m_ptr;
	}

	class RemoveAddRefRelease : public Interface
	{
	private:
		~RemoveAddRefRelease();
		ULONG __stdcall AddRef();
		ULONG __stdcall Release();
	};

	RemoveAddRefRelease* operator->() const noexcept
	{
		assert(m_ptr != nullptr);
		return static_cast<RemoveAddRefRelease*>(m_ptr);
	}

private:
	void InternalAddRef() const noexcept
	{
		if (m_ptr)
		{
			m_ptr->AddRef();
		}
	}

	template<typename OtherInterface>
	void InternalMove(ComPtr<OtherInterface>& other) noexcept
	{
		if (m_ptr != other.m_ptr)
		{
			Release();
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr;
		}
	}

	Interface* m_ptr = nullptr;
};

// operators with ComPtr on right-hand side

template<typename Interface, typename OtherInterface>
bool operator==(OtherInterface* p, const ComPtr<Interface>& rhs) noexcept
{
	return rhs == p;
}

template<typename Interface, typename OtherInterface>
bool operator!=(OtherInterface* p, const ComPtr<Interface>& rhs) noexcept
{
	return rhs != p;
}

template<typename Interface>
bool operator==(std::nullptr_t, const ComPtr<Interface>& rhs) noexcept
{
	return rhs == nullptr;
}

template<typename Interface>
bool operator!=(std::nullptr_t, const ComPtr<Interface>& rhs) noexcept
{
	return rhs != nullptr;
}

// Create COM smart-pointer from existing raw COM pointer and call AddRef.
template<typename Interface>
ComPtr<Interface> CopyComPtr(Interface* p) noexcept
{
	ComPtr<Interface> result;
	result.Copy(p);
	return result;
}

// Create COM smart-pointer from existing raw COM pointer without calling
// AddRef.
template<typename Interface>
ComPtr<Interface> AttachComPtr(Interface* p) noexcept
{
	ComPtr<Interface> result;
	result.Attach(p);
	return result;
}

} // End of namespace Common
