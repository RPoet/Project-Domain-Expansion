#pragma once

#include <type_traits>

#include "Engine/Platform/PlatformDefine.h"

template <typename value_type, typename traits_type>
struct FieldSchema
{
	const char* name = nullptr;
	value_type defaultValue = {};
};

template <typename value_type>
struct DefaultFieldTypeTraits
{
	static bool shouldWrite(const value_type& value, const value_type& defaultValue)
	{
		return value != defaultValue;
	}
};

template <typename element_type>
struct ArrayFieldTypeTraits
{
	using value_type = vector<element_type>;

	static bool shouldWrite(const value_type& value, const value_type& defaultValue)
	{
		return value != defaultValue;
	}
};

template <typename value_type, typename traits_type, typename schema_type>
struct FieldValue
{
	using schema = schema_type;
	using storage_type = value_type;
	using traits = traits_type;

	value_type value = schema_type::default_value;

	FieldValue() = default;
	FieldValue(const FieldValue&) = default;
	FieldValue(FieldValue&&) = default;

	FieldValue(const value_type& inValue)
		: value(inValue)
	{
	}

	FieldValue(value_type&& inValue)
		: value(moveValue(inValue))
	{
	}

	FieldValue& operator=(const FieldValue&) = default;
	FieldValue& operator=(FieldValue&&) = default;

	FieldValue& operator=(const value_type& inValue)
	{
		value = inValue;
		return *this;
	}

	FieldValue& operator=(value_type&& inValue)
	{
		value = moveValue(inValue);
		return *this;
	}

	operator const value_type&() const { return value; }
	operator value_type&() { return value; }

	const value_type& get() const { return value; }
	value_type& get() { return value; }

	void reset()
	{
		value = schema_type::default_value;
	}

	bool operator==(const FieldValue& other) const = default;

	bool operator==(const value_type& other) const
	{
		return value == other;
	}

	bool operator!=(const value_type& other) const
	{
		return value != other;
	}

	friend bool operator==(const value_type& left, const FieldValue& right)
	{
		return left == right.value;
	}

	friend bool operator!=(const value_type& left, const FieldValue& right)
	{
		return left != right.value;
	}
};

template <typename value_type, typename = void>
struct IsFieldValue : false_type
{
};

template <typename value_type>
struct IsFieldValue<
	value_type,
	void_t<
		typename remove_cv_t<remove_reference_t<value_type>>::schema,
		typename remove_cv_t<remove_reference_t<value_type>>::storage_type,
		typename remove_cv_t<remove_reference_t<value_type>>::traits>>
	: true_type
{
};

#define DECLARE_FIELD(type, fieldName, defaultValue) \
	struct fieldName##_field_schema final \
	{ \
		inline static constexpr const char* name = #fieldName; \
		inline static const type default_value = defaultValue; \
	}; \
	using fieldName##_field = FieldValue<type, DefaultFieldTypeTraits<type>, fieldName##_field_schema>; \
	fieldName##_field fieldName = {}; \
	inline static const FieldSchema<type, DefaultFieldTypeTraits<type>> fieldName##_schema = {#fieldName, defaultValue}

#define DECLARE_ARRAY_FIELD(elementType, fieldName, defaultValue) \
	struct fieldName##_field_schema final \
	{ \
		inline static constexpr const char* name = #fieldName; \
		inline static const vector<elementType> default_value = defaultValue; \
	}; \
	using fieldName##_field = FieldValue<vector<elementType>, ArrayFieldTypeTraits<elementType>, fieldName##_field_schema>; \
	fieldName##_field fieldName = {}; \
	inline static const FieldSchema<vector<elementType>, ArrayFieldTypeTraits<elementType>> fieldName##_schema = {#fieldName, defaultValue}
