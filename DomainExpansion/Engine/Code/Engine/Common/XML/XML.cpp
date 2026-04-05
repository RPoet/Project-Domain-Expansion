#include "Engine/Common/XML/XML.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

struct XMLTagData
{
	string name = {};
	unordered_map<string, string> attributeValueByName = {};
};

struct XMLWriteElementNode
{
	string name = {};
	string value = {};
	bool valueAssigned = false;
	unordered_map<string, string> attributeValueByName = {};
	vector<unique_pointer<XMLWriteElementNode>> children = {};
};

using XMLParseCode = XML::ParseCode;

string XML::escapeText(const string& text) const
{
	string escapedText = {};
	escapedText.reserve(text.length());
	for (size_t characterIndex = 0; characterIndex < text.length(); ++characterIndex)
	{
		const char character = text[characterIndex];
		switch (character)
		{
		case '&':
			escapedText += "&amp;";
			break;
		case '<':
			escapedText += "&lt;";
			break;
		case '>':
			escapedText += "&gt;";
			break;
		case '"':
			escapedText += "&quot;";
			break;
		case '\'':
			escapedText += "&apos;";
			break;
		default:
			escapedText.push_back(character);
			break;
		}
	}

	return escapedText;
}

string XML::buildPropertyValueText(const string& propertyValue) const
{
	return propertyValue;
}

string XML::buildPropertyValueText(const char* propertyValue) const
{
	return propertyValue != nullptr ? propertyValue : "";
}

static string unescapeXMLText(const string& text)
{
	string unescapedText = {};
	unescapedText.reserve(text.length());
	for (size_t characterIndex = 0; characterIndex < text.length(); ++characterIndex)
	{
		if (text[characterIndex] != '&')
		{
			unescapedText.push_back(text[characterIndex]);
			continue;
		}

		if (text.compare(characterIndex, 5, "&amp;") == 0)
		{
			unescapedText.push_back('&');
			characterIndex += 4;
			continue;
		}

		if (text.compare(characterIndex, 4, "&lt;") == 0)
		{
			unescapedText.push_back('<');
			characterIndex += 3;
			continue;
		}

		if (text.compare(characterIndex, 4, "&gt;") == 0)
		{
			unescapedText.push_back('>');
			characterIndex += 3;
			continue;
		}

		if (text.compare(characterIndex, 6, "&quot;") == 0)
		{
			unescapedText.push_back('"');
			characterIndex += 5;
			continue;
		}

		if (text.compare(characterIndex, 6, "&apos;") == 0)
		{
			unescapedText.push_back('\'');
			characterIndex += 5;
			continue;
		}

		unescapedText.push_back(text[characterIndex]);
	}

	return unescapedText;
}

string XML::parsePropertyValueText(const string& propertyValueText) const
{
	return propertyValueText;
}

void XML::writeOpenTag(OutputFileStream& fileStream, const char* tagName) const
{
	assert(tagName != nullptr && "[XML][Assert] reason=tag_name_missing");
	fileStream << "<" << tagName << ">\n";
}

void XML::writeOpenTag(
	OutputFileStream& fileStream,
	const char* tagName,
	const char* attributeName,
	const string& attributeValue) const
{
	assert(tagName != nullptr && "[XML][Assert] reason=tag_name_missing");
	assert(attributeName != nullptr && "[XML][Assert] reason=attribute_name_missing");
	fileStream << "<"
			   << tagName
			   << " "
			   << attributeName
			   << "=\""
			   << escapeText(attributeValue)
			   << "\">\n";
}

void XML::writeCloseTag(OutputFileStream& fileStream, const char* tagName) const
{
	assert(tagName != nullptr && "[XML][Assert] reason=tag_name_missing");
	fileStream << "</" << tagName << ">\n";
}

static bool tryParseXMLIndexedItemName(const string& name, uint32& outIndex)
{
	outIndex = 0;
	if (!name.starts_with("item") || name.length() <= 4)
	{
		return false;
	}

	uint64 parsedIndex = 0;
	for (size_t characterIndex = 4; characterIndex < name.length(); ++characterIndex)
	{
		const char character = name[characterIndex];
		if (character < '0' || character > '9')
		{
			return false;
		}

		parsedIndex = (parsedIndex * 10ull) + static_cast<uint64>(character - '0');
		if (parsedIndex > static_cast<uint64>(uint32MaxValue))
		{
			return false;
		}
	}

	outIndex = static_cast<uint32>(parsedIndex);
	return true;
}

static bool splitXMLDocumentKey(
	const string& key,
	vector<string>& outPathSegments,
	string& outAttributeName)
{
	outPathSegments.clear();
	outAttributeName.clear();
	if (key.empty())
	{
		return false;
	}

	for (size_t segmentBeginIndex = 0; segmentBeginIndex < key.length();)
	{
		const size_t segmentEndIndex = key.find('.', segmentBeginIndex);
		const size_t segmentLength = (segmentEndIndex == string::npos ? key.length() : segmentEndIndex) - segmentBeginIndex;
		if (segmentLength == 0)
		{
			return false;
		}

		outPathSegments.push_back(key.substr(segmentBeginIndex, segmentLength));
		if (segmentEndIndex == string::npos)
		{
			break;
		}

		segmentBeginIndex = segmentEndIndex + 1;
	}

	const string& lastSegment = outPathSegments.back();
	if (!lastSegment.empty() && lastSegment[0] == '@')
	{
		if (outPathSegments.size() == 1 || lastSegment.length() == 1)
		{
			return false;
		}

		outAttributeName = lastSegment.substr(1);
		outPathSegments.pop_back();
	}

	return !outPathSegments.empty();
}

static XMLWriteElementNode* findOrAddXMLWriteChildNode(XMLWriteElementNode& parentNode, const string& childName)
{
	for (uint32 childIndex = 0; childIndex < static_cast<uint32>(parentNode.children.size()); ++childIndex)
	{
		XMLWriteElementNode* childNode = parentNode.children[childIndex].get();
		if (childNode != nullptr && childNode->name == childName)
		{
			return childNode;
		}
	}

	unique_pointer<XMLWriteElementNode> childNode(new XMLWriteElementNode());
	childNode->name = childName;
	parentNode.children.push_back(moveValue(childNode));
	return parentNode.children.back().get();
}

static bool insertXMLDocumentEntry(XMLWriteElementNode& documentRoot, const string& key, const string& value)
{
	vector<string> pathSegments = {};
	string attributeName = {};
	if (!splitXMLDocumentKey(key, pathSegments, attributeName))
	{
		return false;
	}

	XMLWriteElementNode* currentNode = &documentRoot;
	for (uint32 pathSegmentIndex = 0; pathSegmentIndex < static_cast<uint32>(pathSegments.size()); ++pathSegmentIndex)
	{
		currentNode = findOrAddXMLWriteChildNode(*currentNode, pathSegments[pathSegmentIndex]);
		assert(currentNode != nullptr && "[XML][Assert] reason=document_write_node_missing");
	}

	if (!attributeName.empty())
	{
		auto attributeIterator = currentNode->attributeValueByName.find(attributeName);
		if (attributeIterator != currentNode->attributeValueByName.end())
		{
			return attributeIterator->second == value;
		}

		currentNode->attributeValueByName.emplace(attributeName, value);
		return true;
	}

	if (currentNode->valueAssigned)
	{
		return currentNode->value == value;
	}

	currentNode->valueAssigned = true;
	currentNode->value = value;
	return true;
}

static void sortXMLWriteTree(XMLWriteElementNode& node)
{
	std::sort(
		node.children.begin(),
		node.children.end(),
		[](const unique_pointer<XMLWriteElementNode>& leftNode, const unique_pointer<XMLWriteElementNode>& rightNode)
		{
			assert(leftNode != nullptr && rightNode != nullptr && "[XML][Assert] reason=document_write_child_missing");
			uint32 leftItemIndex = 0;
			uint32 rightItemIndex = 0;
			const bool leftIsIndexedItem = tryParseXMLIndexedItemName(leftNode->name, leftItemIndex);
			const bool rightIsIndexedItem = tryParseXMLIndexedItemName(rightNode->name, rightItemIndex);
			if (leftIsIndexedItem && rightIsIndexedItem)
			{
				return leftItemIndex < rightItemIndex;
			}

			return leftNode->name < rightNode->name;
		});

	for (uint32 childIndex = 0; childIndex < static_cast<uint32>(node.children.size()); ++childIndex)
	{
		XMLWriteElementNode* childNode = node.children[childIndex].get();
		assert(childNode != nullptr && "[XML][Assert] reason=document_write_child_missing");
		sortXMLWriteTree(*childNode);
	}
}

static bool isXMLWhitespace(const char character)
{
	return character == ' ' || character == '\t' || character == '\r' || character == '\n';
}

static bool isXMLNameCharacter(const char character)
{
	return (character >= 'a' && character <= 'z')
		|| (character >= 'A' && character <= 'Z')
		|| (character >= '0' && character <= '9')
		|| character == '_'
		|| character == '-'
		|| character == ':'
		|| character == '.';
}

static string trimXMLText(const string& text)
{
	size_t beginIndex = 0;
	while (beginIndex < text.length() && isXMLWhitespace(text[beginIndex]))
	{
		++beginIndex;
	}

	size_t endIndex = text.length();
	while (endIndex > beginIndex && isXMLWhitespace(text[endIndex - 1]))
	{
		--endIndex;
	}

	return text.substr(beginIndex, endIndex - beginIndex);
}

static bool startsWithXMLToken(const string& text, const size_t characterIndex, const char* tokenText)
{
	if (characterIndex > text.length())
	{
		return false;
	}

	size_t tokenLength = 0;
	while (tokenText[tokenLength] != '\0')
	{
		++tokenLength;
	}

	return tokenLength <= text.length() - characterIndex
		&& text.compare(characterIndex, tokenLength, tokenText) == 0;
}

static void skipXMLWhitespace(const string& xmlText, size_t& inOutCharacterIndex)
{
	while (inOutCharacterIndex < xmlText.length() && isXMLWhitespace(xmlText[inOutCharacterIndex]))
	{
		++inOutCharacterIndex;
	}
}

static XMLParseCode skipXMLComment(const string& xmlText, size_t& inOutCharacterIndex)
{
	assert(startsWithXMLToken(xmlText, inOutCharacterIndex, "<!--")
		&& "[XML][Assert] reason=comment_parse_must_start_at_comment");

	const size_t commentEndIndex = xmlText.find("-->", inOutCharacterIndex + 4);
	if (commentEndIndex == string::npos)
	{
		return XMLParseCode::malformedDocument;
	}

	inOutCharacterIndex = commentEndIndex + 3;
	return XMLParseCode::succeeded;
}

static XMLParseCode skipXMLProcessingInstruction(const string& xmlText, size_t& inOutCharacterIndex)
{
	assert(startsWithXMLToken(xmlText, inOutCharacterIndex, "<?")
		&& "[XML][Assert] reason=instruction_parse_must_start_at_instruction");

	const size_t instructionEndIndex = xmlText.find("?>", inOutCharacterIndex + 2);
	if (instructionEndIndex == string::npos)
	{
		return XMLParseCode::malformedDocument;
	}

	inOutCharacterIndex = instructionEndIndex + 2;
	return XMLParseCode::succeeded;
}

static XMLParseCode skipXMLIgnorableContent(const string& xmlText, size_t& inOutCharacterIndex)
{
	for (;;)
	{
		skipXMLWhitespace(xmlText, inOutCharacterIndex);
		if (startsWithXMLToken(xmlText, inOutCharacterIndex, "<!--"))
		{
			const XMLParseCode parseCode = skipXMLComment(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			continue;
		}

		if (startsWithXMLToken(xmlText, inOutCharacterIndex, "<?"))
		{
			const XMLParseCode parseCode = skipXMLProcessingInstruction(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			continue;
		}

		return XMLParseCode::succeeded;
	}
}

static XMLParseCode parseXMLName(const string& xmlText, size_t& inOutCharacterIndex, string& outName)
{
	outName.clear();
	if (inOutCharacterIndex >= xmlText.length() || !isXMLNameCharacter(xmlText[inOutCharacterIndex]))
	{
		return XMLParseCode::malformedDocument;
	}

	const size_t nameBeginIndex = inOutCharacterIndex;
	while (inOutCharacterIndex < xmlText.length() && isXMLNameCharacter(xmlText[inOutCharacterIndex]))
	{
		++inOutCharacterIndex;
	}

	outName = xmlText.substr(nameBeginIndex, inOutCharacterIndex - nameBeginIndex);
	return XMLParseCode::succeeded;
}

static XMLParseCode parseXMLAttributeValue(const string& xmlText, size_t& inOutCharacterIndex, string& outValue)
{
	outValue.clear();
	if (inOutCharacterIndex >= xmlText.length())
	{
		return XMLParseCode::malformedDocument;
	}

	const char quoteCharacter = xmlText[inOutCharacterIndex];
	if (quoteCharacter != '"' && quoteCharacter != '\'')
	{
		return XMLParseCode::malformedDocument;
	}

	++inOutCharacterIndex;
	const size_t valueBeginIndex = inOutCharacterIndex;
	while (inOutCharacterIndex < xmlText.length() && xmlText[inOutCharacterIndex] != quoteCharacter)
	{
		++inOutCharacterIndex;
	}

	if (inOutCharacterIndex >= xmlText.length())
	{
		return XMLParseCode::malformedDocument;
	}

	outValue = unescapeXMLText(xmlText.substr(valueBeginIndex, inOutCharacterIndex - valueBeginIndex));
	++inOutCharacterIndex;
	return XMLParseCode::succeeded;
}

static XMLParseCode parseXMLOpenTag(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	XMLTagData& outTagData,
	bool& outSelfClosing)
{
	outTagData = {};
	outSelfClosing = false;

	if (inOutCharacterIndex >= xmlText.length() || xmlText[inOutCharacterIndex] != '<')
	{
		return XMLParseCode::malformedDocument;
	}

	++inOutCharacterIndex;
	if (inOutCharacterIndex < xmlText.length() && xmlText[inOutCharacterIndex] == '/')
	{
		return XMLParseCode::malformedDocument;
	}

	XMLParseCode parseCode = parseXMLName(xmlText, inOutCharacterIndex, outTagData.name);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	for (;;)
	{
		skipXMLWhitespace(xmlText, inOutCharacterIndex);
		if (inOutCharacterIndex >= xmlText.length())
		{
			return XMLParseCode::malformedDocument;
		}

		if (xmlText[inOutCharacterIndex] == '>')
		{
			++inOutCharacterIndex;
			return XMLParseCode::succeeded;
		}

		if (xmlText[inOutCharacterIndex] == '/'
			&& inOutCharacterIndex + 1 < xmlText.length()
			&& xmlText[inOutCharacterIndex + 1] == '>')
		{
			inOutCharacterIndex += 2;
			outSelfClosing = true;
			return XMLParseCode::succeeded;
		}

		string attributeName = {};
		parseCode = parseXMLName(xmlText, inOutCharacterIndex, attributeName);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}

		skipXMLWhitespace(xmlText, inOutCharacterIndex);
		if (inOutCharacterIndex >= xmlText.length() || xmlText[inOutCharacterIndex] != '=')
		{
			return XMLParseCode::malformedDocument;
		}

		++inOutCharacterIndex;
		skipXMLWhitespace(xmlText, inOutCharacterIndex);

		string attributeValue = {};
		parseCode = parseXMLAttributeValue(xmlText, inOutCharacterIndex, attributeValue);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}

		outTagData.attributeValueByName.emplace(attributeName, attributeValue);
	}
}

static XMLParseCode parseXMLClosingTag(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	const string& expectedTagName)
{
	if (!startsWithXMLToken(xmlText, inOutCharacterIndex, "</"))
	{
		return XMLParseCode::malformedDocument;
	}

	inOutCharacterIndex += 2;
	string closingTagName = {};
	const XMLParseCode parseCode = parseXMLName(xmlText, inOutCharacterIndex, closingTagName);
	if (parseCode != XMLParseCode::succeeded || closingTagName != expectedTagName)
	{
		return XMLParseCode::malformedDocument;
	}

	skipXMLWhitespace(xmlText, inOutCharacterIndex);
	if (inOutCharacterIndex >= xmlText.length() || xmlText[inOutCharacterIndex] != '>')
	{
		return XMLParseCode::malformedDocument;
	}

	++inOutCharacterIndex;
	return XMLParseCode::succeeded;
}

static string buildXMLPath(const vector<string>& pathSegments, const string& leafName)
{
	string pathText = {};
	for (size_t segmentIndex = 0; segmentIndex < pathSegments.size(); ++segmentIndex)
	{
		if (!pathText.empty())
		{
			pathText += '.';
		}

		pathText += pathSegments[segmentIndex];
	}

	if (!pathText.empty())
	{
		pathText += '.';
	}

	pathText += leafName;
	return pathText;
}

static XMLParseCode insertXMLKeyValue(
	XMLKeyValueDocument& document,
	const string& key,
	const string& value)
{
	if (key.empty())
	{
		return XMLParseCode::succeeded;
	}

	const bool inserted = document.valueByKey.emplace(key, value).second;
	return inserted
		? XMLParseCode::succeeded
		: XMLParseCode::duplicateKey;
}

static XMLParseCode recordXMLTagAttributes(
	XMLKeyValueDocument& document,
	const XMLTagData& tagData,
	const string& elementPath)
{
	for (auto attributeIterator = tagData.attributeValueByName.begin();
		attributeIterator != tagData.attributeValueByName.end();
		++attributeIterator)
	{
		const string attributeKey = elementPath + ".@" + attributeIterator->first;
		const XMLParseCode parseCode = insertXMLKeyValue(document, attributeKey, attributeIterator->second);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}
	}

	return XMLParseCode::succeeded;
}

static XMLParseCode recordXMLTagValue(
	XMLKeyValueDocument& document,
	const vector<string>& pathSegments,
	const XMLTagData& tagData,
	const bool hasChildElements,
	const string& rawTextValue)
{
	string trimmedTextValue = {};
	string elementValue = {};
	if (!rawTextValue.empty())
	{
		trimmedTextValue = trimXMLText(rawTextValue);
		if (!trimmedTextValue.empty())
		{
			elementValue = unescapeXMLText(trimmedTextValue);
		}
	}

	const auto foundKeyAttribute = tagData.attributeValueByName.find("key");
	const auto foundNameAttribute = tagData.attributeValueByName.find("name");
	const auto foundValueAttribute = tagData.attributeValueByName.find("value");
	const bool hasExplicitKeyValue = foundKeyAttribute != tagData.attributeValueByName.end();
	const bool hasExplicitNameValue = foundKeyAttribute == tagData.attributeValueByName.end()
		&& foundNameAttribute != tagData.attributeValueByName.end()
		&& foundValueAttribute != tagData.attributeValueByName.end();

	if (hasChildElements && !trimmedTextValue.empty() && !hasExplicitKeyValue && !hasExplicitNameValue)
	{
		return XMLParseCode::malformedDocument;
	}

	if (hasExplicitKeyValue)
	{
		const string explicitValue = foundValueAttribute != tagData.attributeValueByName.end()
			? foundValueAttribute->second
			: elementValue;
		return insertXMLKeyValue(document, foundKeyAttribute->second, explicitValue);
	}

	if (hasExplicitNameValue)
	{
		return insertXMLKeyValue(document, foundNameAttribute->second, foundValueAttribute->second);
	}

	const string elementPath = buildXMLPath(pathSegments, tagData.name);
	XMLParseCode parseCode = recordXMLTagAttributes(document, tagData, elementPath);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	if (hasChildElements)
	{
		return XMLParseCode::succeeded;
	}

	if (!trimmedTextValue.empty() || tagData.attributeValueByName.empty())
	{
		return insertXMLKeyValue(document, elementPath, elementValue);
	}

	return XMLParseCode::succeeded;
}

static XMLParseCode parseXMLElement(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	vector<string>& inOutPathSegments,
	XMLKeyValueDocument& inOutDocument)
{
	XMLTagData tagData = {};
	bool selfClosing = false;
	XMLParseCode parseCode = parseXMLOpenTag(xmlText, inOutCharacterIndex, tagData, selfClosing);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	if (selfClosing)
	{
		return recordXMLTagValue(inOutDocument, inOutPathSegments, tagData, false, {});
	}

	inOutPathSegments.push_back(tagData.name);
	string textContent = {};
	bool hasChildElements = false;
	for (;;)
	{
		if (startsWithXMLToken(xmlText, inOutCharacterIndex, "<!--"))
		{
			parseCode = skipXMLComment(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				inOutPathSegments.pop_back();
				return parseCode;
			}

			continue;
		}

		if (startsWithXMLToken(xmlText, inOutCharacterIndex, "<?"))
		{
			parseCode = skipXMLProcessingInstruction(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				inOutPathSegments.pop_back();
				return parseCode;
			}

			continue;
		}

		if (inOutCharacterIndex >= xmlText.length())
		{
			inOutPathSegments.pop_back();
			return XMLParseCode::malformedDocument;
		}

		if (startsWithXMLToken(xmlText, inOutCharacterIndex, "</"))
		{
			parseCode = parseXMLClosingTag(xmlText, inOutCharacterIndex, tagData.name);
			inOutPathSegments.pop_back();
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			return recordXMLTagValue(inOutDocument, inOutPathSegments, tagData, hasChildElements, textContent);
		}

		if (xmlText[inOutCharacterIndex] == '<')
		{
			hasChildElements = true;
			parseCode = parseXMLElement(xmlText, inOutCharacterIndex, inOutPathSegments, inOutDocument);
			if (parseCode != XMLParseCode::succeeded)
			{
				inOutPathSegments.pop_back();
				return parseCode;
			}

			continue;
		}

		const size_t textBeginIndex = inOutCharacterIndex;
		while (inOutCharacterIndex < xmlText.length() && xmlText[inOutCharacterIndex] != '<')
		{
			++inOutCharacterIndex;
		}

		textContent.append(xmlText, textBeginIndex, inOutCharacterIndex - textBeginIndex);
	}
}

void XMLKeyValueDocument::clear()
{
	valueByKey.clear();
}

bool XMLKeyValueDocument::contains(const string& key) const
{
	return valueByKey.find(key) != valueByKey.end();
}

const string* XMLKeyValueDocument::find(const string& key) const
{
	auto foundValue = valueByKey.find(key);
	return foundValue != valueByKey.end()
		? &foundValue->second
		: nullptr;
}

bool XML::writeDocument(OutputFileStream& fileStream, const XMLKeyValueDocument& document) const
{
	if (!fileStream.good() || document.valueByKey.empty())
	{
		return false;
	}

	XMLWriteElementNode documentRoot = {};
	vector<string> sortedKeys = {};
	sortedKeys.reserve(document.valueByKey.size());
	for (auto keyValueIterator = document.valueByKey.begin(); keyValueIterator != document.valueByKey.end(); ++keyValueIterator)
	{
		sortedKeys.push_back(keyValueIterator->first);
	}

	std::sort(sortedKeys.begin(), sortedKeys.end());
	for (uint32 keyIndex = 0; keyIndex < static_cast<uint32>(sortedKeys.size()); ++keyIndex)
	{
		const string* value = document.find(sortedKeys[keyIndex]);
		assert(value != nullptr && "[XML][Assert] reason=document_write_value_missing");
		if (!insertXMLDocumentEntry(documentRoot, sortedKeys[keyIndex], *value))
		{
			return false;
		}
	}

	if (documentRoot.children.size() != 1)
	{
		return false;
	}

	XMLWriteElementNode* rootNode = documentRoot.children[0].get();
	assert(rootNode != nullptr && "[XML][Assert] reason=document_write_root_missing");
	sortXMLWriteTree(*rootNode);

	fileStream << "<?xml version=\"1.0\"?>\n";
	const auto writeNode = [&](const auto& self, const XMLWriteElementNode& node, const uint32 indentDepth) -> bool
	{
		const string indent(indentDepth * 2, ' ');
		fileStream << indent << "<" << node.name;

		vector<string> attributeNames = {};
		attributeNames.reserve(node.attributeValueByName.size());
		for (auto attributeIterator = node.attributeValueByName.begin();
			attributeIterator != node.attributeValueByName.end();
			++attributeIterator)
		{
			attributeNames.push_back(attributeIterator->first);
		}

		std::sort(attributeNames.begin(), attributeNames.end());
		for (uint32 attributeIndex = 0; attributeIndex < static_cast<uint32>(attributeNames.size()); ++attributeIndex)
		{
			const string& attributeName = attributeNames[attributeIndex];
			const auto attributeIterator = node.attributeValueByName.find(attributeName);
			assert(attributeIterator != node.attributeValueByName.end() && "[XML][Assert] reason=document_write_attribute_missing");
			fileStream << " " << attributeName << "=\"" << escapeText(attributeIterator->second) << "\"";
		}

		if (node.children.empty())
		{
			const string valueText = node.valueAssigned ? node.value : "";
			fileStream << ">" << escapeText(valueText) << "</" << node.name << ">\n";
			return fileStream.good();
		}

		assert((!node.valueAssigned || node.value.empty()) && "[XML][Assert] reason=document_write_mixed_content_unsupported");
		fileStream << ">\n";
		for (uint32 childIndex = 0; childIndex < static_cast<uint32>(node.children.size()); ++childIndex)
		{
			const XMLWriteElementNode* childNode = node.children[childIndex].get();
			assert(childNode != nullptr && "[XML][Assert] reason=document_write_child_missing");
			if (!self(self, *childNode, indentDepth + 1))
			{
				return false;
			}
		}

		fileStream << indent << "</" << node.name << ">\n";
		return fileStream.good();
	};

	return writeNode(writeNode, *rootNode, 0) && fileStream.good();
}

bool XML::writeDocumentFile(const string& filePath, const XMLKeyValueDocument& document) const
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[XML][Assert] reason=disk_loader_module_missing");

	string absoluteFilePath = {};
	if (!diskLoaderModule->resolveAbsolutePathFromResources(filePath, absoluteFilePath))
	{
		return false;
	}

	OutputFileStream fileStream = {};
	if (!diskLoaderModule->openOutputFileStream(absoluteFilePath, fileStream, false, true))
	{
		return false;
	}

	return writeDocument(fileStream, document);
}

XML::ParseCode XML::readDocumentFile(const string& filePath, XMLKeyValueDocument& outDocument) const
{
	outDocument.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string absoluteFilePath = {};
	if (!diskLoaderModule->resolvePathFromResources(filePath, absoluteFilePath))
	{
		return ParseCode::fileOpenFailed;
	}

	InputFileStream fileStream = {};
	if (!diskLoaderModule->openInputFileStream(absoluteFilePath, fileStream, true))
	{
		return ParseCode::fileOpenFailed;
	}

	return readDocument(fileStream, outDocument);
}

XMLKeyValueDocument XML::readDocumentFile(const string& filePath) const
{
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocumentFile(filePath, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_file_read_failed");
	return document;
}

XML::ParseCode XML::readDocument(InputFileStream& fileStream, XMLKeyValueDocument& outDocument) const
{
	outDocument.clear();

	fileStream.seekg(0, InputFileStream::end);
	const stream_position fileSize = fileStream.tellg();
	if (fileSize < 0)
	{
		return ParseCode::fileOpenFailed;
	}

	string xmlText = {};
	xmlText.resize(static_cast<size_t>(fileSize));
	fileStream.seekg(0, InputFileStream::beg);
	if (!xmlText.empty())
	{
		fileStream.read(xmlText.data(), static_cast<stream_size>(xmlText.size()));
	}

	if (!fileStream && !xmlText.empty())
	{
		return ParseCode::fileOpenFailed;
	}

	return readDocumentText(xmlText, outDocument);
}

XMLKeyValueDocument XML::readDocument(InputFileStream& fileStream) const
{
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocument(fileStream, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_read_failed");
	return document;
}

XML::ParseCode XML::readDocumentText(const string& xmlText, XMLKeyValueDocument& outDocument) const
{
	outDocument.clear();

	size_t characterIndex = 0;
	if (xmlText.compare(0, 3, "\xEF\xBB\xBF") == 0)
	{
		characterIndex = 3;
	}

	XMLParseCode parseCode = skipXMLIgnorableContent(xmlText, characterIndex);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	if (characterIndex >= xmlText.length())
	{
		return ParseCode::malformedDocument;
	}

	vector<string> pathSegments = {};
	parseCode = parseXMLElement(xmlText, characterIndex, pathSegments, outDocument);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	parseCode = skipXMLIgnorableContent(xmlText, characterIndex);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	return characterIndex == xmlText.length()
		? ParseCode::succeeded
		: ParseCode::malformedDocument;
}

XMLKeyValueDocument XML::readDocumentText(const string& xmlText) const
{
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocumentText(xmlText, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_text_read_failed");
	return document;
}
