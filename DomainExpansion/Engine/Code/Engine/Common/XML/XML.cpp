#include "Engine/Common/XML/XML.h"

#include "Engine/Common/StringSlice.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Profiler/ProfilerModule.h"
#include "Engine/Module/Timer/Timer.h"

inline constexpr size_t xmlParserMaxElementDepth = 64;
inline constexpr size_t xmlParserMaxAttributeCount = 10;
inline constexpr size_t xmlParserMaxTotalAttributeCount = xmlParserMaxElementDepth * xmlParserMaxAttributeCount;
inline constexpr unsigned char xmlCharacterFlagWhitespace = 1u << 0;
inline constexpr unsigned char xmlCharacterFlagName = 1u << 1;

struct XMLCharacterTable
{
	unsigned char flags[256] = {};
};

static const XMLCharacterTable xmlCharacterTable = []()
{
	XMLCharacterTable table = {};
	for (int32 characterCode = 'a'; characterCode <= 'z'; ++characterCode)
	{
		table.flags[characterCode] |= xmlCharacterFlagName;
	}

	for (int32 characterCode = 'A'; characterCode <= 'Z'; ++characterCode)
	{
		table.flags[characterCode] |= xmlCharacterFlagName;
	}

	for (int32 characterCode = '0'; characterCode <= '9'; ++characterCode)
	{
		table.flags[characterCode] |= xmlCharacterFlagName;
	}

	table.flags[static_cast<unsigned char>('_')] |= xmlCharacterFlagName;
	table.flags[static_cast<unsigned char>('-')] |= xmlCharacterFlagName;
	table.flags[static_cast<unsigned char>(':')] |= xmlCharacterFlagName;
	table.flags[static_cast<unsigned char>('.')] |= xmlCharacterFlagName;
	table.flags[static_cast<unsigned char>(' ')] |= xmlCharacterFlagWhitespace;
	table.flags[static_cast<unsigned char>('\t')] |= xmlCharacterFlagWhitespace;
	table.flags[static_cast<unsigned char>('\r')] |= xmlCharacterFlagWhitespace;
	table.flags[static_cast<unsigned char>('\n')] |= xmlCharacterFlagWhitespace;
	return table;
}();

struct XMLStringRange
{
	uint32 beginIndex = 0;
	uint32 endIndex = 0;

	size_t length() const
	{
		return endIndex - beginIndex;
	}
};

struct XMLTagAttribute
{
	XMLStringRange nameRange = {};
	XMLStringRange valueRange = {};
};

using XMLAttributeStorage = InplaceVector<XMLTagAttribute, xmlParserMaxTotalAttributeCount>;

struct XMLWriteElementNode
{
	string name = {};
	string value = {};
	bool valueAssigned = false;
	unordered_map<string, string> attributeValueByName = {};
	vector<unique_pointer<XMLWriteElementNode>> children = {};
};

using XMLParseCode = XML::ParseCode;

static size_t buildXMLDocumentBucketCount(const size_t minimumEntryCount)
{
	size_t bucketCount = 16;
	const size_t requiredBucketCount = std::max<size_t>(minimumEntryCount * 2, 16);
	while (bucketCount < requiredBucketCount)
	{
		bucketCount *= 2;
	}

	return bucketCount;
}

uint64 XMLKeyValueDocument::hashText(const std::string_view text)
{
	uint64 hash = 14695981039346656037ull;
	for (size_t characterIndex = 0; characterIndex < text.length(); ++characterIndex)
	{
		hash ^= static_cast<unsigned char>(text[characterIndex]);
		hash *= 1099511628211ull;
	}

	return hash;
}

uint32 XMLKeyValueDocument::appendTextToStorage(const std::string_view text)
{
	assert(textStorage.size() + text.length() <= static_cast<size_t>(uint32MaxValue) && "[XML][Assert] reason=document_text_storage_overflow");
	const uint32 textOffset = static_cast<uint32>(textStorage.size());
	textStorage.append(text.data(), text.length());
	return textOffset;
}

void XMLKeyValueDocument::rebuildBuckets(const size_t bucketCount)
{
	bucketEntryIndices.clear();
	bucketEntryIndices.resize(bucketCount, invalidEntryIndex);
	touchedBucketIndices.clear();
	touchedBucketIndices.reserve(entries.size());
	const uint32 bucketMask = static_cast<uint32>(bucketEntryIndices.size() - 1);
	for (uint32 entryIndex = 0; entryIndex < static_cast<uint32>(entries.size()); ++entryIndex)
	{
		const Entry& entry = entries[entryIndex];
		uint32 bucketIndex = static_cast<uint32>(entry.keyHash) & bucketMask;
		while (bucketEntryIndices[bucketIndex] != invalidEntryIndex)
		{
			bucketIndex = (bucketIndex + 1) & bucketMask;
		}

		bucketEntryIndices[bucketIndex] = entryIndex;
		touchedBucketIndices.push_back(bucketIndex);
	}
}

void XMLKeyValueDocument::ensureBucketCount(const size_t minimumEntryCount)
{
	const size_t requiredBucketCount = buildXMLDocumentBucketCount(minimumEntryCount);
	if (bucketEntryIndices.size() >= requiredBucketCount)
	{
		return;
	}

	rebuildBuckets(requiredBucketCount);
}

void XMLKeyValueDocument::clear()
{
	for (uint32 touchedBucketIndex = 0; touchedBucketIndex < static_cast<uint32>(touchedBucketIndices.size()); ++touchedBucketIndex)
	{
		bucketEntryIndices[touchedBucketIndices[touchedBucketIndex]] = invalidEntryIndex;
	}

	touchedBucketIndices.clear();
	entries.clear();
	textStorage.clear();
}

void XMLKeyValueDocument::releaseStorage()
{
	vector<Entry> releasedEntries = {};
	entries.swap(releasedEntries);

	vector<uint32> releasedBucketEntryIndices = {};
	bucketEntryIndices.swap(releasedBucketEntryIndices);

	vector<uint32> releasedTouchedBucketIndices = {};
	touchedBucketIndices.swap(releasedTouchedBucketIndices);

	string releasedTextStorage = {};
	textStorage.swap(releasedTextStorage);
}

void XMLKeyValueDocument::reserve(const size_t estimatedKeyCount, const size_t estimatedTextBytes)
{
	entries.reserve(estimatedKeyCount);
	touchedBucketIndices.reserve(estimatedKeyCount);
	if (estimatedTextBytes > textStorage.capacity())
	{
		textStorage.reserve(estimatedTextBytes);
	}

	if (estimatedKeyCount > 0)
	{
		ensureBucketCount(estimatedKeyCount);
	}
}

uint32 XMLKeyValueDocument::findEntryIndex(const std::string_view key) const
{
	if (key.empty() || bucketEntryIndices.empty())
	{
		return invalidEntryIndex;
	}

	const uint64 keyHash = hashText(key);
	const uint32 bucketMask = static_cast<uint32>(bucketEntryIndices.size() - 1);
	uint32 bucketIndex = static_cast<uint32>(keyHash) & bucketMask;
	for (;;)
	{
		const uint32 entryIndex = bucketEntryIndices[bucketIndex];
		if (entryIndex == invalidEntryIndex)
		{
			return invalidEntryIndex;
		}

		const Entry& entry = entries[entryIndex];
		if (entry.keyHash == keyHash && buildTextView(entry.keyOffset, entry.keyLength) == key)
		{
			return entryIndex;
		}

		bucketIndex = (bucketIndex + 1) & bucketMask;
	}
}

bool XMLKeyValueDocument::contains(const std::string_view key) const
{
	return findEntryIndex(key) != invalidEntryIndex;
}

bool XMLKeyValueDocument::tryGetValueView(const std::string_view key, std::string_view& outValueText) const
{
	const uint32 entryIndex = findEntryIndex(key);
	if (entryIndex == invalidEntryIndex)
	{
		outValueText = {};
		return false;
	}

	const Entry& entry = entries[entryIndex];
	outValueText = buildTextView(entry.valueOffset, entry.valueLength);
	return true;
}

bool XMLKeyValueDocument::tryGetValue(const std::string_view key, string& outValueText) const
{
	std::string_view valueText = {};
	if (!tryGetValueView(key, valueText))
	{
		outValueText.clear();
		return false;
	}

	outValueText.assign(valueText.data(), valueText.length());
	return true;
}

void XMLKeyValueDocument::assignValue(const uint32 entryIndex, const std::string_view value)
{
	assert(entryIndex < static_cast<uint32>(entries.size()) && "[XML][Assert] reason=document_assign_value_entry_missing");
	Entry& entry = entries[entryIndex];
	entry.valueOffset = appendTextToStorage(value);
	entry.valueLength = static_cast<uint32>(value.length());
}

bool XMLKeyValueDocument::insert(const std::string_view key, const std::string_view value)
{
	if (key.empty())
	{
		return true;
	}

	ensureBucketCount(entries.size() + 1);
	const uint64 keyHash = hashText(key);
	const uint32 bucketMask = static_cast<uint32>(bucketEntryIndices.size() - 1);
	uint32 bucketIndex = static_cast<uint32>(keyHash) & bucketMask;
	for (;;)
	{
		const uint32 entryIndex = bucketEntryIndices[bucketIndex];
		if (entryIndex == invalidEntryIndex)
		{
			Entry& entry = entries.emplace_back();
			entry.keyHash = keyHash;
			entry.keyOffset = appendTextToStorage(key);
			entry.keyLength = static_cast<uint32>(key.length());
			entry.valueOffset = appendTextToStorage(value);
			entry.valueLength = static_cast<uint32>(value.length());
			bucketEntryIndices[bucketIndex] = static_cast<uint32>(entries.size() - 1);
			touchedBucketIndices.push_back(bucketIndex);
			return true;
		}

		const Entry& existingEntry = entries[entryIndex];
		if (existingEntry.keyHash == keyHash && buildTextView(existingEntry.keyOffset, existingEntry.keyLength) == key)
		{
			return false;
		}

		bucketIndex = (bucketIndex + 1) & bucketMask;
	}
}

void XMLKeyValueDocument::set(const std::string_view key, const std::string_view value)
{
	if (key.empty())
	{
		return;
	}

	const uint32 entryIndex = findEntryIndex(key);
	if (entryIndex != invalidEntryIndex)
	{
		assignValue(entryIndex, value);
		return;
	}

	const bool inserted = insert(key, value);
	assert(inserted && "[XML][Assert] reason=document_set_insert_failed");
}

struct XMLReadMetrics
{
	uint64 fileSizeBytes = 0;
	double readMilliseconds = 0.0;
	double parseMilliseconds = 0.0;
};

struct XMLReadScratch
{
	string xmlText = {};
};

static XMLReadScratch& getXMLReadScratch()
{
	static thread_local XMLReadScratch readScratch;
	return readScratch;
}

static XML::ParseCode readXMLDocumentTextInternal(const string& xmlText, XMLKeyValueDocument& outDocument);

static const char* getXMLParseCodeText(const XML::ParseCode parseCode)
{
	switch (parseCode)
	{
	case XML::ParseCode::succeeded:
		return "succeeded";
	case XML::ParseCode::fileOpenFailed:
		return "file_open_failed";
	case XML::ParseCode::malformedDocument:
		return "malformed_document";
	case XML::ParseCode::duplicateKey:
		return "duplicate_key";
	default:
		return "unknown";
	}
}

static XML::ParseCode readXMLDocumentStream(
	const XML& xml,
	InputFileStream& fileStream,
	XMLKeyValueDocument& outDocument,
	XMLReadMetrics* outReadMetrics)
{
	if (outReadMetrics != nullptr)
	{
		*outReadMetrics = {};
	}

	fileStream.seekg(0, InputFileStream::end);
	const stream_position fileSize = fileStream.tellg();
	if (fileSize < 0)
	{
		return XML::ParseCode::fileOpenFailed;
	}

	XMLReadScratch& readScratch = getXMLReadScratch();
	string& xmlText = readScratch.xmlText;
	xmlText.resize(static_cast<size_t>(fileSize));
	if (outReadMetrics != nullptr)
	{
		outReadMetrics->fileSizeBytes = static_cast<uint64>(xmlText.size());
	}

	fileStream.seekg(0, InputFileStream::beg);
	if (!xmlText.empty())
	{
		PROFILE_SCOPE("xml", "XML::readDocumentBytes");
		Stopwatch readStopwatch = {};
		fileStream.read(xmlText.data(), static_cast<stream_size>(xmlText.size()));
		if (outReadMetrics != nullptr)
		{
			outReadMetrics->readMilliseconds = readStopwatch.getElapsedMilliseconds();
		}
	}

	if (!fileStream && !xmlText.empty())
	{
		return XML::ParseCode::fileOpenFailed;
	}

	Stopwatch parseStopwatch = {};
	const XML::ParseCode parseCode = readXMLDocumentTextInternal(xmlText, outDocument);
	if (outReadMetrics != nullptr)
	{
		outReadMetrics->parseMilliseconds = parseStopwatch.getElapsedMilliseconds();
	}

	return parseCode;
}

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

string XML::parsePropertyValueText(const std::string_view propertyValueText) const
{
	return string(propertyValueText.data(), propertyValueText.length());
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

		outPathSegments.push_back(sliceString(key, segmentBeginIndex, segmentLength));
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

		outAttributeName = sliceString(lastSegment, 1);
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
	return (xmlCharacterTable.flags[static_cast<unsigned char>(character)] & xmlCharacterFlagWhitespace) != 0;
}

static bool isXMLNameCharacter(const char character)
{
	return (xmlCharacterTable.flags[static_cast<unsigned char>(character)] & xmlCharacterFlagName) != 0;
}

static void trimXMLTextRange(const string& text, size_t& inOutBeginIndex, size_t& inOutEndIndex)
{
	while (inOutBeginIndex < inOutEndIndex && isXMLWhitespace(text[inOutBeginIndex]))
	{
		++inOutBeginIndex;
	}

	while (inOutEndIndex > inOutBeginIndex && isXMLWhitespace(text[inOutEndIndex - 1]))
	{
		--inOutEndIndex;
	}
}

static bool isXMLStringRangeEqualToText(
	const string& xmlText,
	const XMLStringRange& range,
	const char* comparisonText)
{
	assert(comparisonText != nullptr && "[XML][Assert] reason=comparison_text_missing");
	const size_t comparisonLength = std::char_traits<char>::length(comparisonText);
	return range.length() == comparisonLength
		&& std::char_traits<char>::compare(xmlText.data() + range.beginIndex, comparisonText, comparisonLength) == 0;
}

static void buildXMLUnescapedText(
	string& outUnescapedText,
	const string& text,
	const size_t beginIndex,
	const size_t endIndex)
{
	outUnescapedText.clear();
	if (beginIndex >= endIndex)
	{
		return;
	}

	const size_t firstEscapeIndex = text.find('&', beginIndex);
	if (firstEscapeIndex == string::npos || firstEscapeIndex >= endIndex)
	{
		outUnescapedText.assign(text, beginIndex, endIndex - beginIndex);
		return;
	}

	outUnescapedText.reserve(endIndex - beginIndex);
	outUnescapedText.append(text, beginIndex, firstEscapeIndex - beginIndex);
	for (size_t characterIndex = firstEscapeIndex; characterIndex < endIndex; ++characterIndex)
	{
		if (text[characterIndex] != '&')
		{
			outUnescapedText.push_back(text[characterIndex]);
			continue;
		}

		if (text.compare(characterIndex, 5, "&amp;") == 0)
		{
			outUnescapedText.push_back('&');
			characterIndex += 4;
			continue;
		}

		if (text.compare(characterIndex, 4, "&lt;") == 0)
		{
			outUnescapedText.push_back('<');
			characterIndex += 3;
			continue;
		}

		if (text.compare(characterIndex, 4, "&gt;") == 0)
		{
			outUnescapedText.push_back('>');
			characterIndex += 3;
			continue;
		}

		if (text.compare(characterIndex, 6, "&quot;") == 0)
		{
			outUnescapedText.push_back('"');
			characterIndex += 5;
			continue;
		}

		if (text.compare(characterIndex, 6, "&apos;") == 0)
		{
			outUnescapedText.push_back('\'');
			characterIndex += 5;
			continue;
		}

		outUnescapedText.push_back(text[characterIndex]);
	}
}

static string buildXMLUnescapedText(const string& text, size_t beginIndex, const size_t endIndex)
{
	string unescapedText = {};
	buildXMLUnescapedText(unescapedText, text, beginIndex, endIndex);
	return unescapedText;
}

static string buildXMLUnescapedText(const string& text, const XMLStringRange& range)
{
	return buildXMLUnescapedText(text, range.beginIndex, range.endIndex);
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
	const size_t xmlLength = xmlText.length();
	const char* xmlCharacters = xmlText.data();
	while (inOutCharacterIndex < xmlLength && isXMLWhitespace(xmlCharacters[inOutCharacterIndex]))
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

static XMLParseCode parseXMLName(const string& xmlText, size_t& inOutCharacterIndex, XMLStringRange& outNameRange)
{
	outNameRange = {};
	const size_t xmlLength = xmlText.length();
	const char* xmlCharacters = xmlText.data();
	if (inOutCharacterIndex >= xmlLength || !isXMLNameCharacter(xmlCharacters[inOutCharacterIndex]))
	{
		return XMLParseCode::malformedDocument;
	}

	const size_t nameBeginIndex = inOutCharacterIndex;
	do
	{
		++inOutCharacterIndex;
	}
	while (inOutCharacterIndex < xmlLength && isXMLNameCharacter(xmlCharacters[inOutCharacterIndex]));

	outNameRange.beginIndex = static_cast<uint32>(nameBeginIndex);
	outNameRange.endIndex = static_cast<uint32>(inOutCharacterIndex);
	return XMLParseCode::succeeded;
}

static XMLParseCode parseXMLAttributeValue(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	XMLStringRange& outValueRange)
{
	outValueRange = {};
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
	const size_t valueEndIndex = xmlText.find(quoteCharacter, valueBeginIndex);
	if (valueEndIndex == string::npos)
	{
		return XMLParseCode::malformedDocument;
	}

	outValueRange.beginIndex = static_cast<uint32>(valueBeginIndex);
	outValueRange.endIndex = static_cast<uint32>(valueEndIndex);
	inOutCharacterIndex = valueEndIndex + 1;
	return XMLParseCode::succeeded;
}

static XMLParseCode parseXMLOpenTag(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	XMLStringRange& outNameRange,
	XMLAttributeStorage& inOutAttributeStorage,
	uint32& outAttributeBeginIndex,
	uint16& outAttributeCount,
	bool& outSelfClosing)
{
	outNameRange = {};
	outSelfClosing = false;
	outAttributeBeginIndex = static_cast<uint32>(inOutAttributeStorage.size());
	outAttributeCount = 0;
	const size_t xmlLength = xmlText.length();
	const char* xmlCharacters = xmlText.data();

	if (inOutCharacterIndex >= xmlLength || xmlCharacters[inOutCharacterIndex] != '<')
	{
		return XMLParseCode::malformedDocument;
	}

	++inOutCharacterIndex;
	if (inOutCharacterIndex < xmlLength && xmlCharacters[inOutCharacterIndex] == '/')
	{
		return XMLParseCode::malformedDocument;
	}

	XMLParseCode parseCode = parseXMLName(xmlText, inOutCharacterIndex, outNameRange);
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	for (;;)
	{
		if (inOutCharacterIndex >= xmlLength)
		{
			return XMLParseCode::malformedDocument;
		}

		const char currentCharacter = xmlCharacters[inOutCharacterIndex];
		if (currentCharacter == '>')
		{
			++inOutCharacterIndex;
			return XMLParseCode::succeeded;
		}

		if (currentCharacter == '/'
			&& inOutCharacterIndex + 1 < xmlLength
			&& xmlCharacters[inOutCharacterIndex + 1] == '>')
		{
			inOutCharacterIndex += 2;
			outSelfClosing = true;
			return XMLParseCode::succeeded;
		}

		if (isXMLWhitespace(currentCharacter))
		{
			do
			{
				++inOutCharacterIndex;
			}
			while (inOutCharacterIndex < xmlLength && isXMLWhitespace(xmlCharacters[inOutCharacterIndex]));
			continue;
		}

		XMLStringRange attributeNameRange = {};
		parseCode = parseXMLName(xmlText, inOutCharacterIndex, attributeNameRange);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}

		while (inOutCharacterIndex < xmlLength && isXMLWhitespace(xmlCharacters[inOutCharacterIndex]))
		{
			++inOutCharacterIndex;
		}

		if (inOutCharacterIndex >= xmlLength || xmlCharacters[inOutCharacterIndex] != '=')
		{
			return XMLParseCode::malformedDocument;
		}

		++inOutCharacterIndex;
		while (inOutCharacterIndex < xmlLength && isXMLWhitespace(xmlCharacters[inOutCharacterIndex]))
		{
			++inOutCharacterIndex;
		}

		XMLStringRange attributeValueRange = {};
		parseCode = parseXMLAttributeValue(xmlText, inOutCharacterIndex, attributeValueRange);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}

		if (outAttributeCount >= xmlParserMaxAttributeCount
			|| inOutAttributeStorage.size() >= inOutAttributeStorage.capacity())
		{
			return XMLParseCode::malformedDocument;
		}

		XMLTagAttribute& attribute = inOutAttributeStorage.emplace_back();
		attribute.nameRange = attributeNameRange;
		attribute.valueRange = attributeValueRange;
		++outAttributeCount;
	}
}

static XMLParseCode parseXMLClosingTag(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	const XMLStringRange& expectedTagNameRange)
{
	if (inOutCharacterIndex + 1 >= xmlText.length()
		|| xmlText[inOutCharacterIndex] != '<'
		|| xmlText[inOutCharacterIndex + 1] != '/')
	{
		return XMLParseCode::malformedDocument;
	}

	inOutCharacterIndex += 2;
	XMLStringRange closingTagNameRange = {};
	const XMLParseCode parseCode = parseXMLName(xmlText, inOutCharacterIndex, closingTagNameRange);
	if (parseCode != XMLParseCode::succeeded
		|| closingTagNameRange.length() != expectedTagNameRange.length()
		|| std::char_traits<char>::compare(
			   xmlText.data() + closingTagNameRange.beginIndex,
			   xmlText.data() + expectedTagNameRange.beginIndex,
			   expectedTagNameRange.length()) != 0)
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

struct XMLTagValueAttributeRanges
{
	const XMLStringRange* keyAttributeValueRange = nullptr;
	const XMLStringRange* nameAttributeValueRange = nullptr;
	const XMLStringRange* valueAttributeValueRange = nullptr;
};

static XMLTagValueAttributeRanges findXMLTagValueAttributeRanges(
	const string& xmlText,
	const XMLAttributeStorage& attributeStorage,
	const uint32 attributeBeginIndex,
	const uint16 attributeCount)
{
	XMLTagValueAttributeRanges result = {};
	for (uint16 attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex)
	{
		const XMLTagAttribute& attribute = attributeStorage[attributeBeginIndex + attributeIndex];
		const size_t attributeNameLength = attribute.nameRange.length();
		const char* attributeName = xmlText.data() + attribute.nameRange.beginIndex;
		if (result.keyAttributeValueRange == nullptr
			&& attributeNameLength == 3
			&& attributeName[0] == 'k'
			&& attributeName[1] == 'e'
			&& attributeName[2] == 'y')
		{
			result.keyAttributeValueRange = &attribute.valueRange;
			continue;
		}

		if (result.nameAttributeValueRange == nullptr
			&& attributeNameLength == 4
			&& attributeName[0] == 'n'
			&& attributeName[1] == 'a'
			&& attributeName[2] == 'm'
			&& attributeName[3] == 'e')
		{
			result.nameAttributeValueRange = &attribute.valueRange;
			continue;
		}

		if (result.valueAttributeValueRange == nullptr
			&& attributeNameLength == 5
			&& attributeName[0] == 'v'
			&& attributeName[1] == 'a'
			&& attributeName[2] == 'l'
			&& attributeName[3] == 'u'
			&& attributeName[4] == 'e')
		{
			result.valueAttributeValueRange = &attribute.valueRange;
		}

		if (result.keyAttributeValueRange != nullptr
			&& result.nameAttributeValueRange != nullptr
			&& result.valueAttributeValueRange != nullptr)
		{
			break;
		}
	}

	return result;
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

	const bool inserted = document.insert(key, value);
	return inserted
		? XMLParseCode::succeeded
		: XMLParseCode::duplicateKey;
}

static XMLParseCode insertXMLKeyValue(
	XMLKeyValueDocument& document,
	const string& key,
	string&& value)
{
	if (key.empty())
	{
		return XMLParseCode::succeeded;
	}

	const bool inserted = document.insert(key, value);
	return inserted
		? XMLParseCode::succeeded
		: XMLParseCode::duplicateKey;
}

static XMLParseCode insertXMLKeyValue(
	XMLKeyValueDocument& document,
	string&& key,
	string&& value)
{
	if (key.empty())
	{
		return XMLParseCode::succeeded;
	}

	const bool inserted = document.insert(key, value);
	return inserted
		? XMLParseCode::succeeded
		: XMLParseCode::duplicateKey;
}

static XMLParseCode insertXMLKeyValue(
	XMLKeyValueDocument& document,
	const string& key,
	const string& sourceText,
	const size_t sourceBeginIndex,
	const size_t sourceLength)
{
	if (key.empty())
	{
		return XMLParseCode::succeeded;
	}

	const bool inserted = document.insert(key, std::string_view(sourceText.data() + sourceBeginIndex, sourceLength));
	return inserted
		? XMLParseCode::succeeded
		: XMLParseCode::duplicateKey;
}

static size_t estimateXMLDocumentKeyCount(const size_t xmlLength)
{
	if (xmlLength == 0)
	{
		return 0;
	}

	const size_t estimatedKeyCount = (xmlLength / 48) + 1;
	return std::min<size_t>(estimatedKeyCount, 8192);
}

static bool shouldReleaseXMLDocumentScratch(
	const XMLKeyValueDocument& document,
	const size_t estimatedKeyCount,
	const size_t estimatedTextBytes)
{
	const size_t requiredBucketCount = estimatedKeyCount > 0
		? buildXMLDocumentBucketCount(estimatedKeyCount)
		: 0;
	if (requiredBucketCount > 0 && document.bucket_count() > requiredBucketCount * 4)
	{
		return true;
	}

	const size_t minimumEntryCapacity = estimatedKeyCount > 0 ? estimatedKeyCount : 1;
	if (document.entries.capacity() > minimumEntryCapacity * 4 && document.entries.capacity() > 256)
	{
		return true;
	}

	const size_t minimumTextCapacity = estimatedTextBytes > 0 ? estimatedTextBytes : 1;
	if (document.textStorage.capacity() > minimumTextCapacity * 4 && document.textStorage.capacity() > 16384)
	{
		return true;
	}

	return false;
}

struct XMLTextCapture
{
	static inline constexpr uint32 invalidBeginIndex = ~0x0u;

	uint32 beginIndex = invalidBeginIndex;
	uint32 endIndex = 0;
	string segmentedText = {};

	void clear()
	{
		beginIndex = invalidBeginIndex;
		endIndex = 0;
		segmentedText.clear();
	}

	void append(const string& xmlText, const size_t segmentBeginIndex, const size_t segmentEndIndex)
	{
		if (segmentBeginIndex >= segmentEndIndex)
		{
			return;
		}

		if (segmentedText.empty())
		{
			if (beginIndex == invalidBeginIndex)
			{
				beginIndex = static_cast<uint32>(segmentBeginIndex);
				endIndex = static_cast<uint32>(segmentEndIndex);
				return;
			}

			if (endIndex == segmentBeginIndex)
			{
				endIndex = static_cast<uint32>(segmentEndIndex);
				return;
			}

			segmentedText.append(xmlText, beginIndex, endIndex - beginIndex);
		}

		segmentedText.append(xmlText, segmentBeginIndex, segmentEndIndex - segmentBeginIndex);
		beginIndex = invalidBeginIndex;
		endIndex = 0;
	}
};

struct XMLParseElementState
{
	XMLTextCapture textCapture = {};
	XMLStringRange nameRange = {};
	uint32 attributeBeginIndex = 0;
	uint32 previousPathLength = 0;
	uint16 attributeCount = 0;
	bool hasChildElements = false;
};

struct XMLParserScratch
{
	XMLAttributeStorage attributeStorage = {};
	InplaceVector<XMLParseElementState, xmlParserMaxElementDepth> elementStack = {};
	string currentPath = {};
	string elementValue = {};
	string attributeKey = {};
	string attributeValue = {};
};

static XMLParserScratch& getXMLParserScratch()
{
	static thread_local XMLParserScratch parserScratch;
	return parserScratch;
}

static void appendXMLPathSegment(
	string& outPath,
	const string& xmlText,
	const XMLStringRange& nameRange)
{
	if (!outPath.empty())
	{
		outPath.push_back('.');
	}

	outPath.append(xmlText, nameRange.beginIndex, nameRange.length());
}

static bool tryGetXMLFastContiguousTextValueRange(
	const string& xmlText,
	const XMLTextCapture& textCapture,
	size_t& outBeginIndex,
	size_t& outEndIndex)
{
	if (textCapture.beginIndex == XMLTextCapture::invalidBeginIndex || textCapture.beginIndex >= textCapture.endIndex)
	{
		return false;
	}

	const size_t beginIndex = textCapture.beginIndex;
	const size_t endIndex = textCapture.endIndex;
	const char* xmlCharacters = xmlText.data();
	if (isXMLWhitespace(xmlCharacters[beginIndex]) || isXMLWhitespace(xmlCharacters[endIndex - 1]))
	{
		return false;
	}

	const size_t escapeIndex = xmlText.find('&', beginIndex);
	if (escapeIndex != string::npos && escapeIndex < endIndex)
	{
		return false;
	}

	outBeginIndex = beginIndex;
	outEndIndex = endIndex;
	return true;
}

static XMLParseCode recordXMLTagAttributes(
	XMLKeyValueDocument& document,
	const string& xmlText,
	const XMLAttributeStorage& attributeStorage,
	const uint32 attributeBeginIndex,
	const uint16 attributeCount,
	const string& elementPath)
{
	PROFILE_SCOPE("xml", "recordXMLTagAttributes");
	XMLParserScratch& parserScratch = getXMLParserScratch();
	string& attributeKey = parserScratch.attributeKey;
	string& attributeValue = parserScratch.attributeValue;
	for (uint16 attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex)
	{
		const XMLTagAttribute& attribute = attributeStorage[attributeBeginIndex + attributeIndex];
		attributeKey.clear();
		attributeKey.reserve(elementPath.length() + 2 + attribute.nameRange.length());
		attributeKey += elementPath;
		attributeKey += ".@";
		attributeKey.append(xmlText, attribute.nameRange.beginIndex, attribute.nameRange.length());
		buildXMLUnescapedText(attributeValue, xmlText, attribute.valueRange.beginIndex, attribute.valueRange.endIndex);
		const XMLParseCode parseCode = insertXMLKeyValue(document, moveValue(attributeKey), moveValue(attributeValue));
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}
	}

	return XMLParseCode::succeeded;
}

static XMLParseCode recordXMLTagValue(
	XMLKeyValueDocument& document,
	const string& xmlText,
	const string& elementPath,
	const XMLAttributeStorage& attributeStorage,
	const uint32 attributeBeginIndex,
	const uint16 attributeCount,
	const bool hasChildElements,
	const XMLTextCapture& textCapture)
{
	PROFILE_SCOPE("xml", "recordXMLTagValue");
	XMLParserScratch& parserScratch = getXMLParserScratch();
	string& elementValue = parserScratch.elementValue;
	bool hasTrimmedTextValue = false;
	bool hasFastContiguousTextValue = false;
	size_t fastContiguousTextBeginIndex = 0;
	size_t fastContiguousTextEndIndex = 0;
	if (textCapture.beginIndex != XMLTextCapture::invalidBeginIndex)
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.text.contiguous");
		if (tryGetXMLFastContiguousTextValueRange(
			xmlText,
			textCapture,
			fastContiguousTextBeginIndex,
			fastContiguousTextEndIndex))
		{
			hasTrimmedTextValue = true;
			hasFastContiguousTextValue = true;
		}
		else
		{
			size_t trimmedBeginIndex = textCapture.beginIndex;
			size_t trimmedEndIndex = textCapture.endIndex;
			trimXMLTextRange(xmlText, trimmedBeginIndex, trimmedEndIndex);
			if (trimmedBeginIndex < trimmedEndIndex)
			{
				elementValue.clear();
				hasTrimmedTextValue = true;
				buildXMLUnescapedText(elementValue, xmlText, trimmedBeginIndex, trimmedEndIndex);
			}
		}
	}
	else if (!textCapture.segmentedText.empty())
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.text.segmented");
		size_t trimmedBeginIndex = 0;
		size_t trimmedEndIndex = textCapture.segmentedText.length();
		trimXMLTextRange(textCapture.segmentedText, trimmedBeginIndex, trimmedEndIndex);
		if (trimmedBeginIndex < trimmedEndIndex)
		{
			elementValue.clear();
			hasTrimmedTextValue = true;
			buildXMLUnescapedText(elementValue, textCapture.segmentedText, trimmedBeginIndex, trimmedEndIndex);
		}
	}

	if (attributeCount == 0)
	{
		if (hasChildElements)
		{
			return hasTrimmedTextValue
				? XMLParseCode::malformedDocument
				: XMLParseCode::succeeded;
		}

		PROFILE_SCOPE("xml", "recordXMLTagValue.insertLeaf");
		if (hasFastContiguousTextValue)
		{
			return insertXMLKeyValue(
				document,
				elementPath,
				xmlText,
				fastContiguousTextBeginIndex,
				fastContiguousTextEndIndex - fastContiguousTextBeginIndex);
		}

		if (!hasTrimmedTextValue)
		{
			static const string emptyElementValue = {};
			return insertXMLKeyValue(document, elementPath, emptyElementValue);
		}

		return insertXMLKeyValue(document, elementPath, moveValue(elementValue));
	}

	const XMLStringRange* foundKeyAttributeValueRange = nullptr;
	const XMLStringRange* foundNameAttributeValueRange = nullptr;
	const XMLStringRange* foundValueAttributeValueRange = nullptr;
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.lookupAttributes");
		const XMLTagValueAttributeRanges foundAttributeRanges = findXMLTagValueAttributeRanges(
			xmlText,
			attributeStorage,
			attributeBeginIndex,
			attributeCount);
		foundKeyAttributeValueRange = foundAttributeRanges.keyAttributeValueRange;
		foundNameAttributeValueRange = foundAttributeRanges.nameAttributeValueRange;
		foundValueAttributeValueRange = foundAttributeRanges.valueAttributeValueRange;
	}
	const bool hasExplicitKeyValue = foundKeyAttributeValueRange != nullptr;
	const bool hasExplicitNameValue = foundKeyAttributeValueRange == nullptr
		&& foundNameAttributeValueRange != nullptr
		&& foundValueAttributeValueRange != nullptr;

	if (hasChildElements && hasTrimmedTextValue && !hasExplicitKeyValue && !hasExplicitNameValue)
	{
		return XMLParseCode::malformedDocument;
	}

	if (hasExplicitKeyValue)
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.explicitKey");
		string explicitKey = buildXMLUnescapedText(xmlText, *foundKeyAttributeValueRange);
		string explicitValue = {};
		if (foundValueAttributeValueRange != nullptr)
		{
			buildXMLUnescapedText(explicitValue, xmlText, foundValueAttributeValueRange->beginIndex, foundValueAttributeValueRange->endIndex);
		}
		else if (hasFastContiguousTextValue)
		{
			explicitValue.assign(xmlText, fastContiguousTextBeginIndex, fastContiguousTextEndIndex - fastContiguousTextBeginIndex);
		}
		else
		{
			explicitValue = moveValue(elementValue);
		}

		return insertXMLKeyValue(document, moveValue(explicitKey), moveValue(explicitValue));
	}

	if (hasExplicitNameValue)
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.explicitNameValue");
		string explicitName = buildXMLUnescapedText(xmlText, *foundNameAttributeValueRange);
		string explicitValue = buildXMLUnescapedText(xmlText, *foundValueAttributeValueRange);
		return insertXMLKeyValue(document, moveValue(explicitName), moveValue(explicitValue));
	}

	XMLParseCode parseCode = XMLParseCode::succeeded;
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.recordAttributes");
		parseCode = recordXMLTagAttributes(document, xmlText, attributeStorage, attributeBeginIndex, attributeCount, elementPath);
	}
	if (parseCode != XMLParseCode::succeeded)
	{
		return parseCode;
	}

	if (hasChildElements)
	{
		return XMLParseCode::succeeded;
	}

	if (hasTrimmedTextValue)
	{
		PROFILE_SCOPE("xml", "recordXMLTagValue.insertLeaf");
		if (hasFastContiguousTextValue)
		{
			return insertXMLKeyValue(
				document,
				elementPath,
				xmlText,
				fastContiguousTextBeginIndex,
				fastContiguousTextEndIndex - fastContiguousTextBeginIndex);
		}

		return insertXMLKeyValue(document, elementPath, moveValue(elementValue));
	}

	return XMLParseCode::succeeded;
}

static void rewindXMLAttributeStorage(XMLAttributeStorage& inOutAttributeStorage, const uint32 targetAttributeCount)
{
	while (inOutAttributeStorage.size() > targetAttributeCount)
	{
		inOutAttributeStorage.pop_back();
	}
}

static XMLParseCode parseXMLElement(
	const string& xmlText,
	size_t& inOutCharacterIndex,
	XMLKeyValueDocument& inOutDocument)
{
	PROFILE_SCOPE("xml", "parseXMLElement");
	XMLParserScratch& parserScratch = getXMLParserScratch();
	XMLAttributeStorage& attributeStorage = parserScratch.attributeStorage;
	InplaceVector<XMLParseElementState, xmlParserMaxElementDepth>& elementStack = parserScratch.elementStack;
	string& currentPath = parserScratch.currentPath;
	attributeStorage.clear();
	elementStack.clear();
	currentPath.clear();
	const size_t xmlLength = xmlText.length();
	const char* xmlCharacters = xmlText.data();
	for (;;)
	{
		if (elementStack.empty())
		{
			PROFILE_SCOPE("xml", "parseXMLElement.rootOpen");
			XMLStringRange rootNameRange = {};
			uint32 rootAttributeBeginIndex = 0;
			uint16 rootAttributeCount = 0;
			bool rootSelfClosing = false;
			XMLParseCode parseCode = parseXMLOpenTag(
				xmlText,
				inOutCharacterIndex,
				rootNameRange,
				attributeStorage,
				rootAttributeBeginIndex,
				rootAttributeCount,
				rootSelfClosing);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			const uint32 rootPreviousPathLength = static_cast<uint32>(currentPath.length());
			appendXMLPathSegment(currentPath, xmlText, rootNameRange);
			if (rootSelfClosing)
			{
				parseCode = recordXMLTagValue(
					inOutDocument,
					xmlText,
					currentPath,
					attributeStorage,
					rootAttributeBeginIndex,
					rootAttributeCount,
					false,
					{});
				rewindXMLAttributeStorage(attributeStorage, rootAttributeBeginIndex);
				currentPath.resize(rootPreviousPathLength);
				return parseCode;
			}

			if (elementStack.size() >= elementStack.capacity())
			{
				return XMLParseCode::malformedDocument;
			}

			XMLParseElementState& rootElementState = elementStack.emplace_back();
			rootElementState.textCapture.clear();
			rootElementState.nameRange = rootNameRange;
			rootElementState.attributeBeginIndex = rootAttributeBeginIndex;
			rootElementState.previousPathLength = rootPreviousPathLength;
			rootElementState.attributeCount = rootAttributeCount;
			rootElementState.hasChildElements = false;
		}

		XMLParseElementState& currentElementState = elementStack[elementStack.size() - 1];
		if (inOutCharacterIndex >= xmlLength)
		{
			return XMLParseCode::malformedDocument;
		}

		const char currentCharacter = xmlCharacters[inOutCharacterIndex];
		if (currentCharacter != '<')
		{
			const size_t textBeginIndex = inOutCharacterIndex;
			const size_t textEndIndex = xmlText.find('<', inOutCharacterIndex);
			inOutCharacterIndex = textEndIndex != string::npos
				? textEndIndex
				: xmlLength;

			PROFILE_SCOPE("xml", "parseXMLElement.appendText");
			currentElementState.textCapture.append(xmlText, textBeginIndex, inOutCharacterIndex);
			continue;
		}

		if (inOutCharacterIndex + 1 >= xmlLength)
		{
			return XMLParseCode::malformedDocument;
		}

		const char nextCharacter = xmlCharacters[inOutCharacterIndex + 1];
		if (nextCharacter == '!'
			&& inOutCharacterIndex + 3 < xmlLength
			&& xmlCharacters[inOutCharacterIndex + 2] == '-'
			&& xmlCharacters[inOutCharacterIndex + 3] == '-')
		{
			PROFILE_SCOPE("xml", "parseXMLElement.skipComment");
			XMLParseCode parseCode = skipXMLComment(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			continue;
		}

		if (nextCharacter == '?')
		{
			PROFILE_SCOPE("xml", "parseXMLElement.skipProcessingInstruction");
			XMLParseCode parseCode = skipXMLProcessingInstruction(xmlText, inOutCharacterIndex);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			continue;
		}

		if (nextCharacter == '/')
		{
			PROFILE_SCOPE("xml", "parseXMLElement.closeTag");
			XMLParseCode parseCode = parseXMLClosingTag(xmlText, inOutCharacterIndex, currentElementState.nameRange);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			const uint32 completedAttributeBeginIndex = currentElementState.attributeBeginIndex;
			const uint32 completedPreviousPathLength = currentElementState.previousPathLength;
			const uint16 completedAttributeCount = currentElementState.attributeCount;
			parseCode = recordXMLTagValue(
				inOutDocument,
				xmlText,
				currentPath,
				attributeStorage,
				completedAttributeBeginIndex,
				completedAttributeCount,
				currentElementState.hasChildElements,
				currentElementState.textCapture);
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			rewindXMLAttributeStorage(attributeStorage, completedAttributeBeginIndex);
			elementStack.pop_back();
			currentPath.resize(completedPreviousPathLength);
			if (elementStack.empty())
			{
				return XMLParseCode::succeeded;
			}

			continue;
		}

		PROFILE_SCOPE("xml", "parseXMLElement.childOpen");
		currentElementState.hasChildElements = true;

		XMLStringRange childNameRange = {};
		uint32 childAttributeBeginIndex = 0;
		uint16 childAttributeCount = 0;
		bool childSelfClosing = false;
		XMLParseCode parseCode = parseXMLOpenTag(
			xmlText,
			inOutCharacterIndex,
			childNameRange,
			attributeStorage,
			childAttributeBeginIndex,
			childAttributeCount,
			childSelfClosing);
		if (parseCode != XMLParseCode::succeeded)
		{
			return parseCode;
		}

		const uint32 childPreviousPathLength = static_cast<uint32>(currentPath.length());
		appendXMLPathSegment(currentPath, xmlText, childNameRange);
		if (childSelfClosing)
		{
			parseCode = recordXMLTagValue(
				inOutDocument,
				xmlText,
				currentPath,
				attributeStorage,
				childAttributeBeginIndex,
				childAttributeCount,
				false,
				{});
			if (parseCode != XMLParseCode::succeeded)
			{
				return parseCode;
			}

			rewindXMLAttributeStorage(attributeStorage, childAttributeBeginIndex);
			currentPath.resize(childPreviousPathLength);
			continue;
		}

		if (elementStack.size() >= elementStack.capacity())
		{
			return XMLParseCode::malformedDocument;
		}

		XMLParseElementState& childElementState = elementStack.emplace_back();
		childElementState.textCapture.clear();
		childElementState.nameRange = childNameRange;
		childElementState.attributeBeginIndex = childAttributeBeginIndex;
		childElementState.previousPathLength = childPreviousPathLength;
		childElementState.attributeCount = childAttributeCount;
		childElementState.hasChildElements = false;
	}
}

bool XML::writeDocument(OutputFileStream& fileStream, const XMLKeyValueDocument& document) const
{
	PROFILE_SCOPE("xml", "writeDocument");
	if (!fileStream.good() || document.empty())
	{
		return false;
	}

	XMLWriteElementNode documentRoot = {};
	vector<std::string_view> sortedKeys = {};
	sortedKeys.reserve(document.size());
	document.forEach(
		[&](const std::string_view keyText, const std::string_view valueText)
		{
			unused(valueText);
			sortedKeys.push_back(keyText);
		});

	std::sort(sortedKeys.begin(), sortedKeys.end());
	for (uint32 keyIndex = 0; keyIndex < static_cast<uint32>(sortedKeys.size()); ++keyIndex)
	{
		std::string_view valueText = {};
		const bool foundValue = document.tryGetValueView(sortedKeys[keyIndex], valueText);
		assert(foundValue && "[XML][Assert] reason=document_write_value_missing");
		if (!insertXMLDocumentEntry(
				documentRoot,
				string(sortedKeys[keyIndex].data(), sortedKeys[keyIndex].length()),
				string(valueText.data(), valueText.length())))
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
	PROFILE_SCOPE("xml", "writeDocumentFile");
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
	PROFILE_SCOPE_DETAIL("xml", "XML::readDocumentFile", filePath);
	outDocument.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string absoluteFilePath = {};
	if (!diskLoaderModule->resolvePathFromResources(filePath, absoluteFilePath))
	{
		ProfilerModule::get()->recordXMLDocumentLoad({
			.filePath = filePath,
			.parseResult = getXMLParseCodeText(ParseCode::fileOpenFailed),
		});
		return ParseCode::fileOpenFailed;
	}

	InputFileStream fileStream = {};
	if (!diskLoaderModule->openInputFileStream(absoluteFilePath, fileStream, true))
	{
		ProfilerModule::get()->recordXMLDocumentLoad({
			.filePath = filePath,
			.parseResult = getXMLParseCodeText(ParseCode::fileOpenFailed),
		});
		return ParseCode::fileOpenFailed;
	}

	XMLReadMetrics readMetrics = {};
	const ParseCode parseCode = readXMLDocumentStream(*this, fileStream, outDocument, &readMetrics);
		ProfilerModule::get()->recordXMLDocumentLoad({
			.filePath = filePath,
			.parseResult = getXMLParseCodeText(parseCode),
			.fileSizeBytes = readMetrics.fileSizeBytes,
			.keyCount = static_cast<uint64>(outDocument.size()),
			.readMilliseconds = readMetrics.readMilliseconds,
			.parseMilliseconds = readMetrics.parseMilliseconds,
		});
	return parseCode;
}

XMLKeyValueDocument XML::readDocumentFile(const string& filePath) const
{
	PROFILE_SCOPE("xml", "readDocumentFile");
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocumentFile(filePath, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_file_read_failed");
	return document;
}

XML::ParseCode XML::readDocument(InputFileStream& fileStream, XMLKeyValueDocument& outDocument) const
{
	PROFILE_SCOPE("xml", "readDocument");
	outDocument.clear();
	return readXMLDocumentStream(*this, fileStream, outDocument, nullptr);
}

XMLKeyValueDocument XML::readDocument(InputFileStream& fileStream) const
{
	PROFILE_SCOPE("xml", "readDocument");
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocument(fileStream, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_read_failed");
	return document;
}

static XML::ParseCode readXMLDocumentTextInternal(const string& xmlText, XMLKeyValueDocument& outDocument)
{
	const size_t estimatedKeyCount = estimateXMLDocumentKeyCount(xmlText.length());
	if (shouldReleaseXMLDocumentScratch(outDocument, estimatedKeyCount, xmlText.length()))
	{
		outDocument.releaseStorage();
	}
	outDocument.reserve(estimatedKeyCount, xmlText.length());

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
		return XML::ParseCode::malformedDocument;
	}

	parseCode = parseXMLElement(xmlText, characterIndex, outDocument);
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
		? XML::ParseCode::succeeded
		: XML::ParseCode::malformedDocument;
}

XML::ParseCode XML::readDocumentText(const string& xmlText, XMLKeyValueDocument& outDocument) const
{
	PROFILE_SCOPE("xml", "XML::readDocumentText");
	outDocument.clear();
	return readXMLDocumentTextInternal(xmlText, outDocument);
}

XMLKeyValueDocument XML::readDocumentText(const string& xmlText) const
{
	PROFILE_SCOPE("xml", "readDocumentText");
	XMLKeyValueDocument document = {};
	const ParseCode parseCode = readDocumentText(xmlText, document);
	assert(parseCode == ParseCode::succeeded && "[XML][Assert] reason=document_text_read_failed");
	return document;
}
