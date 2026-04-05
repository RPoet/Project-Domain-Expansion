#pragma once

template <typename type_name>
inline constexpr bool platformComPointerUsesReferenceCounting =
	requires(type_name* referenceCandidate)
	{
		referenceCandidate->AddRef();
		referenceCandidate->Release();
	};

template <typename type_name>
class platform_com_pointer
{
public:
	template <typename other_type_name>
	friend class platform_com_pointer;

	platform_com_pointer() = default;
	platform_com_pointer(decltype(nullptr))
	{
	}

	platform_com_pointer(type_name* rawPointer)
		: pointer(rawPointer)
	{
		internalAddRef();
	}

	platform_com_pointer(const platform_com_pointer& other)
		: pointer(other.pointer)
	{
		internalAddRef();
	}

	platform_com_pointer(platform_com_pointer&& other) noexcept
		: pointer(other.pointer)
	{
		other.pointer = nullptr;
	}

	~platform_com_pointer()
	{
		internalRelease();
	}

	platform_com_pointer& operator=(const platform_com_pointer& other)
	{
		if (this == addressof(other))
		{
			return *this;
		}

		assignRawPointer(other.pointer);
		return *this;
	}

	platform_com_pointer& operator=(platform_com_pointer&& other) noexcept
	{
		if (this == addressof(other))
		{
			return *this;
		}

		internalRelease();
		pointer = other.pointer;
		other.pointer = nullptr;
		return *this;
	}

	platform_com_pointer& operator=(type_name* rawPointer)
	{
		assignRawPointer(rawPointer);
		return *this;
	}

	platform_com_pointer& operator=(decltype(nullptr))
	{
		Reset();
		return *this;
	}

	type_name* Get() const
	{
		return pointer;
	}

	type_name** operator&()
	{
		return ReleaseAndGetAddressOf();
	}

	type_name** GetAddressOf()
	{
		return &pointer;
	}

	type_name* const* GetAddressOf() const
	{
		return &pointer;
	}

	type_name** ReleaseAndGetAddressOf()
	{
		internalRelease();
		return &pointer;
	}

	void Reset()
	{
		internalRelease();
	}

	type_name* operator->() const
	{
		return pointer;
	}

	explicit operator bool() const
	{
		return pointer != nullptr;
	}

	bool operator==(decltype(nullptr)) const
	{
		return pointer == nullptr;
	}

	bool operator!=(decltype(nullptr)) const
	{
		return pointer != nullptr;
	}

private:
	void internalAddRef()
	{
		if constexpr (platformComPointerUsesReferenceCounting<type_name>)
		{
			if (pointer != nullptr)
			{
				pointer->AddRef();
			}
		}
	}

	void internalRelease()
	{
		type_name* releasingPointer = pointer;
		pointer = nullptr;
		if constexpr (platformComPointerUsesReferenceCounting<type_name>)
		{
			if (releasingPointer != nullptr)
			{
				releasingPointer->Release();
			}
		}
	}

	void assignRawPointer(type_name* rawPointer)
	{
		if (pointer == rawPointer)
		{
			return;
		}

		if constexpr (platformComPointerUsesReferenceCounting<type_name>)
		{
			if (rawPointer != nullptr)
			{
				rawPointer->AddRef();
			}
		}

		internalRelease();
		pointer = rawPointer;
	}

	type_name* pointer = nullptr;
};
