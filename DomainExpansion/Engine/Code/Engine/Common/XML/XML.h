#pragma once

#include "Engine/Common/Singleton.h"
#include "Engine/Platform/PlatformDefine.h"

struct XMLKeyValueDocument
{
	unordered_map<string, string> valueByKey = {};

	void clear();
	bool contains(const string& key) const;
	const string* find(const string& key) const;
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
	string parsePropertyValueText(const string& propertyValueText) const;

	template <typename value_type>
	inline std::enable_if_t<!std::is_same_v<value_type, string> && !std::is_same_v<value_type, const char*>, string>
	buildPropertyValueText(const value_type& propertyValue) const
	{
		return to_string(propertyValue);
	}

	template <typename value_type>
	inline std::enable_if_t<!std::is_same_v<value_type, string>, value_type>
	parsePropertyValueText(const string& propertyValueText) const
	{
		value_type propertyValue = {};
		string_input_stream parser(propertyValueText);
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

	template <typename value_type>
	inline void writeProperty(
		OutputFileStream& fileStream,
		const char* propertyName,
		const value_type& propertyValue) const
	{
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");

		fileStream << string("  <")
				   << string(propertyName)
				   << string(">")
				   << escapeText(buildPropertyValueText(propertyValue))
				   << string("</")
				   << string(propertyName)
				   << string(">\n");
	}

	template <typename value_type>
	inline bool readProperty(
		const XMLKeyValueDocument& document,
		const char* propertyName,
		value_type& outPropertyValue) const
	{
		assert(propertyName != nullptr && "[XML][Assert] reason=property_name_missing");
		const string* propertyValueText = document.find(propertyName);
		if (propertyValueText == nullptr)
		{
			return false;
		}

		if constexpr (std::is_same_v<value_type, string>)
		{
			outPropertyValue = parsePropertyValueText(*propertyValueText);
		}
		else
		{
			outPropertyValue = parsePropertyValueText<value_type>(*propertyValueText);
		}
		return true;
	}

	ParseCode readDocument(InputFileStream& fileStream, XMLKeyValueDocument& outDocument) const;
	ParseCode readDocumentFile(const string& filePath, XMLKeyValueDocument& outDocument) const;
	ParseCode readDocumentText(const string& xmlText, XMLKeyValueDocument& outDocument) const;
};
