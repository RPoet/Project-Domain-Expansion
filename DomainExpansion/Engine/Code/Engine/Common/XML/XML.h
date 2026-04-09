#pragma once

#include "Engine/Common/Field.h"
#include "Engine/Common/Singleton.h"
#include "Engine/Platform/PlatformDefine.h"

struct XMLKeyValueDocument
{
	static inline constexpr uint32 invalidEntryIndex = uint32MaxValue;

	struct Entry
	{
		uint64 keyHash = 0;
		uint32 keyOffset = 0;
		uint32 keyLength = 0;
		uint32 valueOffset = 0;
		uint32 valueLength = 0;
	};

	vector<Entry> entries = {};
	vector<uint32> bucketEntryIndices = {};
	vector<uint32> touchedBucketIndices = {};
	string textStorage = {};

	void clear();

	inline bool empty() const
	{
		return entries.empty();
	}

	inline size_t size() const
	{
		return entries.size();
	}

	inline size_t bucket_count() const
	{
		return bucketEntryIndices.size();
	}

	void reserve(size_t estimatedKeyCount, size_t estimatedTextBytes = 0);
	bool contains(const string_view key) const;
	bool tryGetValueView(const string_view key, string_view& outValueText) const;
	bool tryGetValue(const string_view key, string& outValueText) const;
	bool insert(const string_view key, const string_view value);
	void set(const string_view key, const string_view value);

	template <typename callback_type>
	inline void forEach(callback_type&& callback) const
	{
		for (uint32 entryIndex = 0; entryIndex < static_cast<uint32>(entries.size()); ++entryIndex)
		{
			const Entry& entry = entries[entryIndex];
			callback(buildTextView(entry.keyOffset, entry.keyLength), buildTextView(entry.valueOffset, entry.valueLength));
		}
	}

private:
	string_view buildTextView(const uint32 textOffset, const uint32 textLength) const
	{
		return string_view(textStorage.data() + textOffset, textLength);
	}

	uint32 appendTextToStorage(const string_view text);
	uint32 findEntryIndex(const string_view key) const;
	void ensureBucketCount(const size_t minimumEntryCount);
	void rebuildBuckets(const size_t bucketCount);
	void assignValue(uint32 entryIndex, const string_view value);
	static uint64 hashText(const string_view text);
};

class XML final : public Singleton<XML>
{
	friend class Singleton<XML>;

public:
	enum class ParseCode : int32
	{
		succeeded = 0,
		fileOpenFailed = -1,
		malformedDocument = -2,
		duplicateKey = -3,
	};

private:
	XML() = default;

	string escapeText(const string& text) const;
	string buildPropertyValueText(const string& propertyValue) const;
	string buildPropertyValueText(const char* propertyValue) const;
	string parsePropertyValueText(const string_view propertyValueText) const;

	template <typename value_type>
	inline enable_if<!is_same_v<value_type, string> && !is_same_v<value_type, const char*>, string>
	buildPropertyValueText(const value_type& propertyValue) const
	{
		return to_string(propertyValue);
	}

	template <typename value_type>
	inline enable_if<!is_same_v<value_type, string>, value_type>
	parsePropertyValueText(const string_view propertyValueText) const
	{
		value_type propertyValue = {};
		const string propertyValueCopy(propertyValueText.data(), propertyValueText.length());
		string_input_stream parser(propertyValueCopy);
		parser >> propertyValue;
		assert(parser && parser.eof() && "[XML][Assert] reason=property_value_parse_failed");
		return propertyValue;
	}

public:
	void writeOpenTag(OutputFileStream& fileStream, const char* tagName) const;
	void writeOpenTag(
		OutputFileStream& fileStream,
		const char* tagName,
		const char* attributeName,
		const string& attributeValue) const;
	void writeCloseTag(OutputFileStream& fileStream, const char* tagName) const;
	bool writeDocument(OutputFileStream& fileStream, const XMLKeyValueDocument& document) const;
	bool writeDocumentFile(const string& filePath, const XMLKeyValueDocument& document) const;

	template <typename value_type>
	inline void writeProperty(
		OutputFileStream& fileStream,
		const char* propertyName,
		const value_type& propertyValue) const
	{
		PROFILE_SCOPE("xml", "writeProperty");
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");
		fileStream << "  <"
				   << propertyName
				   << ">"
				   << escapeText(buildPropertyValueText(propertyValue))
				   << "</"
				   << propertyName
				   << ">\n";
	}

	template <typename value_type>
	inline void writePropertyArray(
		OutputFileStream& fileStream,
		const char* propertyName,
		const vector<value_type>& propertyValues) const
	{
		PROFILE_SCOPE("xml", "writePropertyArray");
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");
		if (propertyValues.empty())
		{
			return;
		}

		writeOpenTag(fileStream, propertyName);
		for (uint32 propertyValueIndex = 0; propertyValueIndex < static_cast<uint32>(propertyValues.size()); ++propertyValueIndex)
		{
			const string itemPropertyName = "item" + to_string(propertyValueIndex);
			writeProperty(fileStream, itemPropertyName.c_str(), propertyValues[propertyValueIndex]);
		}
		writeCloseTag(fileStream, propertyName);
	}

	template <typename value_type>
	inline enable_if<!IsFieldValue<value_type>::value, bool>
	readProperty(
		const XMLKeyValueDocument& document,
		const char* propertyName,
		value_type& outPropertyValue) const
	{
		PROFILE_SCOPE("xml", "readProperty");
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");
		string_view propertyValueText = {};
		if (document.tryGetValueView(propertyName, propertyValueText))
		{
			if constexpr (is_same_v<value_type, string>)
			{
				outPropertyValue = parsePropertyValueText(propertyValueText);
			}
			else
			{
				outPropertyValue = parsePropertyValueText<value_type>(propertyValueText);
			}

			return true;
		}

		return false;
	}

	template <typename value_type>
	inline value_type readPropertyOrDefault(
		const XMLKeyValueDocument& document,
		const char* propertyName,
		value_type defaultValue) const
	{
		readProperty(document, propertyName, defaultValue);
		return defaultValue;
	}

	template <typename value_type>
	inline void readPropertyArray(
		const XMLKeyValueDocument& document,
		const char* propertyName,
		vector<value_type>& outPropertyValues) const
	{
		PROFILE_SCOPE("xml", "readPropertyArray");
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");
		outPropertyValues.clear();

		for (uint32 propertyValueIndex = 0;; ++propertyValueIndex)
		{
			value_type propertyValue = {};
			const string indexedPropertyPath = string(propertyName) + ".item" + to_string(propertyValueIndex);
			if (!readProperty(document, indexedPropertyPath.c_str(), propertyValue))
			{
				break;
			}

			outPropertyValues.push_back(propertyValue);
		}
	}

	template <typename value_type>
	inline void writeFieldValue(
		OutputFileStream& fileStream,
		const char* propertyName,
		const value_type& propertyValue) const
	{
		writeProperty(fileStream, propertyName, propertyValue);
	}

	template <typename value_type>
	inline void writeFieldValue(
		OutputFileStream& fileStream,
		const char* propertyName,
		const vector<value_type>& propertyValues) const
	{
		writePropertyArray(fileStream, propertyName, propertyValues);
	}

	template <typename value_type>
	inline bool readFieldValue(
		const XMLKeyValueDocument& document,
		const char* propertyPath,
		value_type& outPropertyValue) const
	{
		return readProperty(document, propertyPath, outPropertyValue);
	}

	template <typename value_type>
	inline bool readFieldValue(
		const XMLKeyValueDocument& document,
		const char* propertyPath,
		vector<value_type>& outPropertyValues) const
	{
		const string firstPropertyPath = string(propertyPath) + ".item0";
		if (!document.contains(firstPropertyPath))
		{
			return false;
		}

		readPropertyArray(document, propertyPath, outPropertyValues);
		return true;
	}

	template <typename field_type>
	inline enable_if<IsFieldValue<field_type>::value>
	writeProperty(
		OutputFileStream& fileStream,
		const field_type& fieldValue) const
	{
		using normalized_field_type = remove_cv_t<remove_reference_t<field_type>>;
		assert(normalized_field_type::getPropertyName() != nullptr && "[XML][Assert] reason=field_schema_name_missing");
		if (!fieldValue.shouldWrite())
		{
			return;
		}

		writeFieldValue(fileStream, normalized_field_type::getPropertyName(), fieldValue.get());
	}

	template <typename field_type, typename value_type>
	inline enable_if<IsFieldValue<field_type>::value>
	writeProperty(
		OutputFileStream& fileStream,
		const field_type& fieldMetadata,
		const value_type& propertyValue) const
	{
		using normalized_field_type = remove_cv_t<remove_reference_t<field_type>>;
		using traits_type = typename normalized_field_type::traits;
		unused(fieldMetadata);
		assert(normalized_field_type::getPropertyName() != nullptr && "[XML][Assert] reason=field_schema_name_missing");
		if (!traits_type::shouldWrite(propertyValue, normalized_field_type::getDefaultValue()))
		{
			return;
		}

		writeFieldValue(fileStream, normalized_field_type::getPropertyName(), propertyValue);
	}

	template <typename field_type>
	inline enable_if<IsFieldValue<field_type>::value, bool>
	readProperty(
		const XMLKeyValueDocument& document,
		const char* documentPathPrefix,
		field_type& outFieldValue) const
	{
		using normalized_field_type = remove_cv_t<remove_reference_t<field_type>>;
		assert(documentPathPrefix != nullptr && "[XML][Assert] reason=document_path_prefix_missing");
		assert(normalized_field_type::getPropertyName() != nullptr && "[XML][Assert] reason=field_schema_name_missing");
		const string fieldPath = string(documentPathPrefix) + "." + normalized_field_type::getPropertyName();
		return readFieldValue(document, fieldPath.c_str(), outFieldValue.get());
	}

	ParseCode readDocument(InputFileStream& fileStream, XMLKeyValueDocument& outDocument) const;
	ParseCode readDocumentFile(const string& filePath, XMLKeyValueDocument& outDocument) const;
	ParseCode readDocumentText(const string& xmlText, XMLKeyValueDocument& outDocument) const;
	XMLKeyValueDocument readDocument(InputFileStream& fileStream) const;
	XMLKeyValueDocument readDocumentFile(const string& filePath) const;
	XMLKeyValueDocument readDocumentText(const string& xmlText) const;
};
