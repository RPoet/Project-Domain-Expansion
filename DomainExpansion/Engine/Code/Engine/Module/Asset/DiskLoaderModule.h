#pragma once

#include "Engine/Module/Module.h"

class DiskLoaderModule final : public StaticModule<DiskLoaderModule>
{
public:
	DiskLoaderModule()
		: StaticModule("DiskLoaderModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	bool loadBinaryFile(const string& absolutePath, vector<char>& outBinaryData) const;
	bool saveBinaryFile(const string& absolutePath, const vector<char>& binaryData) const;
};
