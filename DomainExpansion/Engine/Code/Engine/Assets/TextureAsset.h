#pragma once

#include "Asset.h"

class TextureAsset : public Asset
{
public:
	DECLARE_ASSET(TextureAsset);
	constexpr static uint32 version = 1;

	TextureAsset()
		: Asset(true)
	{
	}

	void clear() override;

	uint32 getWidth() const;
	uint32 getHeight() const;
	uint32 getChannelCount() const;
	const string& getSource() const;
	vector<char>& getPixelData();
	const vector<char>& getPixelData() const;
	void setWidth(uint32 inWidth);
	void setHeight(uint32 inHeight);
	void setChannelCount(uint32 inChannelCount);
	void setSource(const string& inSource);
	bool isValid() const;

	void serialize(OutputFileStream& fileStream) const override;
	void deserialize(InputFileStream& fileStream) override;

private:
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

	uint32 width = 0;
	uint32 height = 0;
	uint32 channelCount = 4;
	string source = {};
	vector<char> pixelData = {};
};
