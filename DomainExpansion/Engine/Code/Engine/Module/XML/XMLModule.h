#pragma once

#include "Engine/Module/Module.h"

struct XMLKeyValueDocument
{
	unordered_map<string, string> valueByKey = {};

	void clear();
	bool contains(const string& key) const;
	const string* find(const string& key) const;
};

class XMLModule final : public StaticModule<XMLModule>
{
public:
	enum class ParseCode : int32
	{
		succeeded = 0,
		fileOpenFailed = -1,
		malformedDocument = -2,
		duplicateKey = -3,
	};

	XMLModule()
		: StaticModule("XMLModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	ParseCode loadKeyValueFile(const string& filePath, XMLKeyValueDocument& outDocument) const;
	ParseCode parseKeyValueText(const string& xmlText, XMLKeyValueDocument& outDocument) const;
};
